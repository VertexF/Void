#include "Game.hpp"

#include "Application/Window.hpp"
#include "Application/Keys.hpp"
#include "Application/Audio.hpp"
#include "Application/UserInterface.hpp"

#include "Graphics/CommandBuffer.hpp"
#include "Graphics/LoadGLTF.hpp"
#include "Graphics/ShaderData.hpp"
#include "Graphics/Skybox.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"
#include "cglm/struct/cam.h"

#include "vender/imgui/imgui.h"
//#include "vender/tracy/tracy/Tracy.hpp"

#include "Foundation/File.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/Time.hpp"
#include "Foundation/Array.hpp"

#include "Physics/Physics.hpp"

#include <stdlib.h>
#include <SDL3/SDL.h>
#include <vender/stb_image.h>

namespace
{
    //TODO: Figure out if you need this stuff.
    PipelineHandle mainPipeline;
    PipelineHandle debugPipeline;
    PipelineHandle particleComputePipeline;

    DescriptorSetLayoutHandle mainDescriptorSetLayout;

    BufferHandle positionalBuffer[FRAMES_IN_FLIGHT];
    BufferHandle debugGlobalBuffer;
    BufferHandle indirectCountBufferHandle = INVALID_BUFFER;
    BufferHandle indirectBufferHandle = INVALID_BUFFER;

    void uploadMaterial(MaterialData& meshData, const MeshDraw& meshDraw)
    {
        meshData.textures[0] = meshDraw.diffuseTextureIndex;
        meshData.textures[1] = meshDraw.roughnessTextureIndex;
        meshData.textures[2] = meshDraw.normalTextureIndex;
        meshData.textures[3] = meshDraw.occlusionTextureIndex;

        meshData.emissiveFactor =
        {
            meshDraw.emissiveFactor.x,
            meshDraw.emissiveFactor.y,
            meshDraw.emissiveFactor.z
        };

        meshData.emissiveTextureIndex = meshDraw.emisiveTextureIndex;

        meshData.baseColourFactor = meshDraw.baseColourFactor;
        meshData.metallicRoughnessOcclusionFactor = meshDraw.metallicRoughnessOcclusionFactor;
        meshData.alphaCutoff = meshDraw.alphaCutoff;
        meshData.iorFactor = meshDraw.iorFactor;
        meshData.specularValue = meshDraw.specularValue;
        meshData.flags = meshDraw.flags;

        // NOTE: for left-handed systems (as defined in cglm) need to invert positive and negative Z.
        mat4s model = meshDraw.model;
        meshData.model = model;
        meshData.modelInv = glms_mat4_inv(glms_mat4_transpose(model));
    }

    static constexpr uint16_t INVALID_SCENE_TEXTURE_INDEX = UINT16_MAX;
}

void Game::init(GPUDevice& inGPU, AudioSystem& inAudioSystem, ImguiService& inImgui)
{
    gpu = &inGPU;
    audioSystem = &inAudioSystem;
    imgui = &inImgui;

    //Create pipeline state
    PipelineCreation pipelineCreation{};

    //Depth
    pipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //Shader state
    FileReadResult vertexShaderCode = fileReadBinary("Assets/Shaders/coreShader.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragShaderCode = fileReadBinary("Assets/Shaders/coreShaderNew.frag.spv", &MemoryService::instance()->scratchAllocator);

    pipelineCreation.shaders.setName("main")
        .addStage(vertexShaderCode.data, uint32_t(vertexShaderCode.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragShaderCode.data, uint32_t(fragShaderCode.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    pipelineCreation.pushConstants.createPushConstants((VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, 24);

    //Descriptor set layout.
    DescriptorSetLayoutCreation mainCreation{};
    mainCreation.addBinding({ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .binding = 0, .count = 1, .stage = VK_SHADER_STAGE_ALL_GRAPHICS, .name = "MaterialConstant" })
        .setSetIndex(1);
    mainCreation.bindless = false;

    //Setting it into pipeline.
    //This descriptor set layout will be ran every draw calls
    mainDescriptorSetLayout = gpu->createDescriptorSetLayout(mainCreation);
    //This descriptor set layout will be ran every frame
    pipelineCreation.addDescriptorSetLayout(gpu->bindlessDescriptorSetLayoutHandle)
                    .addDescriptorSetLayout(mainDescriptorSetLayout);

    mainPipeline = gpu->createPipeline(pipelineCreation);

    //Debug renderer
    PipelineCreation debugPipelineCreation{};
    debugPipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    //debugPipelineCreation.depthStencil.depthEnable = false;

    //Shader state
    FileReadResult vertDebug = fileReadBinary("Assets/Shaders/debugRendering.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragDebug = fileReadBinary("Assets/Shaders/debugRendering.frag.spv", &MemoryService::instance()->scratchAllocator);

    debugPipelineCreation.shaders.setName("debugRenderer")
        .addStage(vertDebug.data, uint32_t(vertDebug.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragDebug.data, uint32_t(fragDebug.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    debugPipelineCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0, 24);

    debugPipeline = gpu->createPipeline(debugPipelineCreation, /*debugRendering=*/ true);

    //Particle compute
    PipelineCreation computeParticlePipelineCreation{};

    //Shader state
    FileReadResult particleComp = fileReadBinary("Assets/Shaders/particles.comp.spv", &MemoryService::instance()->scratchAllocator);

    computeParticlePipelineCreation.shaders.setName("debugRenderer")
        .addStage(particleComp.data, uint32_t(particleComp.size), VK_SHADER_STAGE_COMPUTE_BIT)
        .setSPVInput(true);

    computeParticlePipelineCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, 16);

    particleComputePipeline = gpu->createPipeline(computeParticlePipelineCreation);

    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
    // This needs to be done before any other Jolt function is called.
    JPH::RegisterDefaultAllocator();

    Physics::instance();

    scene.initScene(&MemoryService::instance()->systemAllocator, *gpu, mainDescriptorSetLayout);
    scene.buildScene();
    //scene.buildPerformance();
    //scene.buildDebugScene();

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    Physics::instance().physicsSystem.OptimizeBroadPhase();

    BufferCreation bufferCreation{};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        bufferCreation.reset()
            .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(EntityData) * scene.entityData.size)
            .setName("othername")
            .setData(scene.entityData.data);
        positionalBuffer[i] = gpu->createBindlessBuffer(bufferCreation);
    }

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(UniformData))
        .setName("debugGlobalBuffer");
    debugGlobalBuffer = gpu->createBindlessBuffer(bufferCreation);

    beginFrameTick = timeNow();

    gameCamera.internal3DCamera.initPerspective(0.01f, 5000.f, 60.f, (float)Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(7.f, 3.0f, 0.1f);

    initSkybox(*gpu);
    renderer2D.init(*gpu);
    renderer2D.loadTexture("Assets/Textures/particles.png");
    userInterface.init(renderer2D);
    userInterface.buildGameUI();

    float xCentre = (((float)Window::instance()->width) - 256) / 2;
    float yCentre = (((float)Window::instance()->height) - 256) / 2;

    vec2s spriteOffset = { .x = 1, .y = 1 };
    vec2s buttonSize = { 256.f, 256.f };
    vec2s subSpriteSize = { 64.f, 64.f };

    renderer2D.addQuad({ 0.f, 0.f, 0.f }, buttonSize, subSpriteSize, { 0, 3 }, spriteOffset);
    renderer2D.loadBuffer(BILLBOARD_FLAG_2D);

    modelScale = 1.0f;

    debugRenderer = true;
    element = 0;
    recreatePositionBuffer = false;

    setupDrawCalls();
}

void Game::loop(InputHandler& inputHandler, [[maybe_unused]] GPUProfiler& gpuProfiler)
{
    while (Window::instance()->exitRequested == false)
    {
        //ZoneScoped;
        inputHandler.onEvent(gpu, &userInterface);
        if (inputHandler.isKeyDown(Keys::KEY_ESCAPE))
        {
            Window::instance()->exitRequested = true;
        }
        else if (inputHandler.isKeyJustReleased(Keys::KEY_F))
        {
            Window::instance()->setFullscreen();
        }

        //New Frame
        if (Window::instance()->minimised == false)
        {
            //playerPosition = convertToVec3(static_cast<Player*>(scene.entities[0].entityData)->character->GetPosition());

            //This is only false when we can't recreate the swapchain because of 0 height due to VK_ERROR_OUT_OF_DATE_KHR constantly being hit.
            //We still need to acquire an image to re-check if can now correctly fetch a swapchain image. 
            if (gpu->newFrame() == false)
            {
                continue;
            }

            mat4s globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

            if (Window::instance()->resizeRequested)
            {
                Window::instance()->resizeRequested = false;

                gpu->resize(Window::instance()->width, Window::instance()->height);
                gameCamera.internal3DCamera.setAspectRatio(Window::instance()->width * 1.f / Window::instance()->height);
            }

            //static_cast<Player*>(scene.entities[0].entityData)->handleEvents(inputHandler, convertToVec3JPH(gameCamera.internal3DCamera.direction));

            if (inputHandler.isKeyJustReleased(Keys::KEY_1))
            {
                debugRenderer = !debugRenderer;
            }
            else if (inputHandler.isKeyJustReleased(Keys::KEY_SPACE))
            {
                audioSystem->playSoundEffect(sfx::Lazer);
            }
            else if (inputHandler.isKeyJustReleased(Keys::KEY_R))
            {
                //static_cast<Player*>(scene.entities[0].entityData)->resetPosition();
                gameCamera.resetPlayerCamera();
            }

            ////NOTE: This must be after the OS messages.
            //imgui->newFrame();

            //if (ImGui::Begin("Void ImGui"))
            //{
            //    ImGui::InputFloat("Model Scale", &modelScale, 0.001f);
            //}
            //ImGui::End();

            //if (ImGui::Begin("GPU"))
            //{
            //    gpuProfiler.imguiDraw();
            //}
            //ImGui::End();

            //Moves key pressed events stores then in a key-pressed array. This allows us to know if a key is being held down, rather than just pressed. 
            inputHandler.newFrame();
            //Saves the mouse position in screen coordinates and handles events that are for re-mapped key bindings 
            inputHandler.update();

            //I want the physics delta outside of the loop for now.
            const int64_t currentTick = timeNow();
            float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
            beginFrameTick = currentTick;

            Physics::instance().updatePhysics(deltaTime);

            //static_cast<Player*>(scene.entities[0].entityData)->update(deltaTime, *audioSystem);

            gameCamera.update(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, deltaTime);
            //gameCamera.updatePlayerCamera(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, playerPosition, { 0.f, 0.f, 0.f, 0.f }, deltaTime);
            Window::instance()->centerMouse(inputHandler.isMouseDragging(MouseButtons::MOUSE_BUTTON_RIGHT));
            
            deleteEntity();

            CommandBuffer* gpuCommands = gpu->getCommandBuffer(VK_QUEUE_GRAPHICS_BIT, true);
            gpuCommands->pushMarker("Frame");

            gpu->beginRenderingTransition(gpuCommands);
            runParticleCompute(gpuCommands);
            gpuCommands->beginRendering();

            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            PushConstants pushConstants{};
            Buffer* globalSceneBuffer = gpu->accessBuffer(debugGlobalBuffer);
            pushConstants.sceneAddress = globalSceneBuffer->bufferAddress;
            pushConstants.vertexDataAddress = 0;
            pushConstants.modelPositionAddress = 0;

            UniformData globalSceneData{};
            globalSceneData.globalModel = globalModel;
            globalSceneData.view = gameCamera.internal3DCamera.view;
            globalSceneData.project = gameCamera.internal3DCamera.projection;
            globalSceneData.eye = vec4s{ gameCamera.internal3DCamera.direction.x, gameCamera.internal3DCamera.direction.y, gameCamera.internal3DCamera.direction.z, 1.f };
            vec3s lightPosition = glms_vec3_add(playerPosition, glms_vec3_scale(gameCamera.internal3DCamera.direction, 10.f));
            globalSceneData.light = vec4s{ lightPosition.x, lightPosition.y, lightPosition.z, 1.f };
            //globalSceneData.light = vec4s{ gameCamera.internal3DCamera.position.x, gameCamera.internal3DCamera.position.y, gameCamera.internal3DCamera.position.z, 1.f };

            vmaCopyMemoryToAllocation(gpu->VMAAllocator, &globalSceneData, globalSceneBuffer->vmaAllocation, 0, sizeof(UniformData));

            //Scene
            gpuCommands->bindPipeline(mainPipeline);

            gpuCommands->bindlessDescriptorSet(0);

            Buffer* positionBuff = gpu->accessBuffer(positionalBuffer[gpu->currentFrame]);
            pushConstants.modelPositionAddress = positionBuff->bufferAddress;

            for (uint32_t entityIndex = 0; entityIndex < scene.entities.size; ++entityIndex)
            {
                const Entity& entity = scene.entities[entityIndex];
                uint32_t entityIdx = scene.entities[entityIndex].entityIndex;

                if (entity.isDeleted) 
                {
                    continue;
                }

                if (entity.entityType != PLAYER)
                {
                    if (entity.bodyID.IsInvalid() == false && entity.isDynamic)
                    {
                        JPH::RMat44 newPos = Physics::instance().bodyInterface->GetWorldTransform(entity.bodyID);
                        mat4s modelPosition = convertToMat4(newPos);
                        scene.entityData[entityIdx].position = modelPosition;
                    }
                }
                else
                {
                    //JPH::RMat44 newPos = Physics::instance().bodyInterface->GetWorldTransform(static_cast<Player*>(scene.entities[0].entityData)->character->GetBodyID());
                    //mat4s modelPosition = convertToMat4(newPos);
                    //scene.entityData[entityIdx].position = modelPosition;
                }
            }

            vmaCopyMemoryToAllocation(gpu->VMAAllocator, scene.entityData.data, positionBuff->vmaAllocation, 0, sizeof(EntityData) * scene.entityData.size);

            uint32_t instanceCountOffset = 0;
            for (int32_t modelIndexType = scene.models.size - 1; modelIndexType >= 0; --modelIndexType)
            {
                for (uint32_t meshIndex = 0; meshIndex < scene.models[modelIndexType].meshDraws.size; ++meshIndex)
                {
                    MeshDraw meshDraw = scene.models[modelIndexType].meshDraws[meshIndex];

                    MapBufferParameters materialMap = { meshDraw.materialBuffer, 0, 0 };
                    MaterialData* materialBufferData = reinterpret_cast<MaterialData*>(gpu->mapBuffer(materialMap));

                    uploadMaterial(*materialBufferData, meshDraw);
                    gpu->unmapBuffer(materialMap);

                    Buffer* vertexDataBuf = gpu->accessBuffer(meshDraw.vertexBuffer);
                    pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                    vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

                    gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);
                    gpuCommands->bindDescriptorSet(&meshDraw.descriptorSet, 1, nullptr, 0, 1);

                    gpuCommands->drawIndexed(meshDraw.count, scene.models[modelIndexType].instanceCount, 0, 0, instanceCountOffset);
                }

                instanceCountOffset += scene.models[modelIndexType].instanceCount;
            }

            if (debugRenderer)
            {
                //Debug
                gpuCommands->bindPipeline(debugPipeline);

                pushConstants.modelPositionAddress = positionBuff->bufferAddress;
                pushConstants.sceneAddress = globalSceneBuffer->bufferAddress;

                instanceCountOffset = 0;
                for (int32_t modelIndexType = scene.debugModels.size - 1; modelIndexType >= 0; --modelIndexType)
                {
                    VOID_ASSERTM(scene.debugModels[modelIndexType].meshDraws.size == 1, "Collider geometry have have one draw call.\n");

                    MeshDraw meshDraw = scene.debugModels[modelIndexType].meshDraws[0];

                    Buffer* vertexDataBuf = gpu->accessBuffer(meshDraw.vertexBuffer);
                    pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                    vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

                    gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);

                    gpuCommands->drawIndexed(meshDraw.count, scene.debugModels[modelIndexType].instanceCount, 0, 0, instanceCountOffset);

                    instanceCountOffset += scene.debugModels[modelIndexType].instanceCount;
                }
            }

            drawSkybox(*gpu, *gpuCommands, pushConstants);
            renderer2D.drawQuad3D(*gpuCommands, gameCamera.internal3DCamera, indirectBufferHandle, indirectCountBufferHandle);

            //imgui->render(*gpuCommands);

            gpuCommands->popMarker();

            //gpuProfiler.update(gpu);

            gpu->queueCommandBuffer(gpuCommands);
            gpu->present();
        }
        else
        {
            //ImGui::Render();
        }

        //FrameMark;
    }
}

void Game::shutdown() 
{
    vkDeviceWaitIdle(gpu->vulkanDevice);

    shutdownSkybox(*gpu);
    renderer2D.shutdown();

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        gpu->destroyBuffer(positionalBuffer[i]);
    }

    gpu->destroyBuffer(debugGlobalBuffer);

    scene.shutdownScene(*gpu);
    Physics::instance().shutdownPhysics();

    gpu->destroyDescriptorSetLayout(mainDescriptorSetLayout);
    gpu->destroyPipeline(mainPipeline);
    gpu->destroyPipeline(debugPipeline);
    gpu->destroyPipeline(particleComputePipeline);
}

void Game::setupDrawCalls() 
{
    BufferHandle drawCountHandle = { gpu->buffers.obtainResource() };
    if (drawCountHandle.index == INVALID_INDEX)
    {
        indirectCountBufferHandle = INVALID_BUFFER;
        return;
    }

    Buffer* bufferCount = gpu->accessBuffer(drawCountHandle);
    bufferCount->name = "Indirect Draw Count Buffer";
    bufferCount->size = 4;
    bufferCount->typeFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bufferCount->handle = drawCountHandle;
    bufferCount->globalOffset = 0;

    VkBufferCreateInfo bufferInfoCount{};
    bufferInfoCount.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfoCount.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bufferInfoCount.size = 4;

    VmaAllocationCreateInfo memoryInfoCount{};
    memoryInfoCount.usage = VMA_MEMORY_USAGE_AUTO;
    memoryInfoCount.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VmaAllocationInfo allocationInfoCount{};
    VkResult result = vmaCreateBuffer(gpu->VMAAllocator, &bufferInfoCount, &memoryInfoCount, &bufferCount->vkBuffer, &bufferCount->vmaAllocation, &allocationInfoCount);
    VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Asset Code %u", result)

    gpu->setResourceName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bufferCount->vkBuffer), "Indirect Draw Count Buffer");
    bufferCount->vkDeviceMemory = allocationInfoCount.deviceMemory;

    VkBufferDeviceAddressInfo bufferBDAInfoCount{};
    bufferBDAInfoCount.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferBDAInfoCount.buffer = bufferCount->vkBuffer;

    bufferCount->bufferAddress = vkGetBufferDeviceAddress(gpu->vulkanDevice, &bufferBDAInfoCount);

    indirectCountBufferHandle = drawCountHandle;

    BufferHandle drawHandle = { gpu->buffers.obtainResource() };
    if (drawHandle.index == INVALID_INDEX)
    {
        indirectBufferHandle = INVALID_BUFFER;
        return;
    }

    Buffer* bufferIndirect = gpu->accessBuffer(drawHandle);
    bufferIndirect->name = "Indirect Draw Buffer";
    //I don't know why yet you would want to make a draw command this large.
    bufferIndirect->size = 128 * 1024 * 1024;
    bufferIndirect->typeFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bufferIndirect->handle = drawHandle;
    bufferIndirect->globalOffset = 0;

    VkBufferCreateInfo bufferInfoIndirect{};
    bufferInfoIndirect.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfoIndirect.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    //I don't know why yet you would want to make a draw command this large.
    bufferInfoIndirect.size = 128 * 1024 * 1024;

    VmaAllocationCreateInfo memoryInfoIndirect{};
    memoryInfoIndirect.usage = VMA_MEMORY_USAGE_AUTO;
    memoryInfoIndirect.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VmaAllocationInfo allocationInfoIndirect{};
    result = vmaCreateBuffer(gpu->VMAAllocator, &bufferInfoIndirect, &memoryInfoIndirect, &bufferIndirect->vkBuffer, &bufferIndirect->vmaAllocation, &allocationInfoIndirect);
    VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Asset Code %u", result)

    gpu->setResourceName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bufferIndirect->vkBuffer), "Indirect Draw Buffer");
    bufferIndirect->vkDeviceMemory = allocationInfoIndirect.deviceMemory;
    //buffer->vkDeviceMemory = allocationInfo.deviceMemory;

    VkBufferDeviceAddressInfo bufferBDAInfoIndirect{};
    bufferBDAInfoIndirect.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferBDAInfoIndirect.buffer = bufferIndirect->vkBuffer;

    bufferIndirect->bufferAddress = vkGetBufferDeviceAddress(gpu->vulkanDevice, &bufferBDAInfoIndirect);

    indirectBufferHandle = drawHandle;
}

void Game::runParticleCompute(CommandBuffer* commandBuffer)
{
    Buffer* indirectBufferCount = gpu->accessBuffer(indirectCountBufferHandle);
    Buffer* indirectBuffer = gpu->accessBuffer(indirectBufferHandle);

    VkBufferMemoryBarrier2 bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo dependencyInfoCount{};
    dependencyInfoCount.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfoCount.bufferMemoryBarrierCount = 1;
    dependencyInfoCount.pBufferMemoryBarriers = &bufferBarrier;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyInfoCount);

    vkCmdFillBuffer(commandBuffer->vkCommandBuffer, indirectBufferCount->vkBuffer, 0, 4, 0);

    VkBufferMemoryBarrier2 bufferComputeBarrier{};
    bufferComputeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    bufferComputeBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    bufferComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferComputeBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeBarrier.buffer = indirectBuffer->vkBuffer;
    bufferComputeBarrier.offset = 0;
    bufferComputeBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferComputeCountBarrier{};
    bufferComputeCountBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferComputeCountBarrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferComputeCountBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferComputeCountBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferComputeCountBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferComputeCountBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeCountBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeCountBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferComputeCountBarrier.offset = 0;
    bufferComputeCountBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 computeBarriers[] =
    {
        bufferComputeBarrier,
        bufferComputeCountBarrier
    };

    VkDependencyInfo dependencyComputeInfo{};
    dependencyComputeInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyComputeInfo.bufferMemoryBarrierCount = 2;
    dependencyComputeInfo.pBufferMemoryBarriers = computeBarriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyComputeInfo);

    ComputePushConstant pushConstants{};
    uint32_t drawCount = 1;
    uint32_t computeLocalXID = 32;
    commandBuffer->bindPipeline(particleComputePipeline);

    pushConstants.indirectAddress = indirectBuffer->bufferAddress;
    pushConstants.indirectCountAddress = indirectBufferCount->bufferAddress;

    vkCmdPushConstants(commandBuffer->vkCommandBuffer, commandBuffer->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &pushConstants);

    uint32_t numberOfXThreads = (drawCount + computeLocalXID - 1) / computeLocalXID;

    vkCmdDispatch(commandBuffer->vkCommandBuffer, numberOfXThreads, 1, 1);

    VkBufferMemoryBarrier2 bufferIndirectBarrier{};
    bufferIndirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferIndirectBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferIndirectBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferIndirectBarrier.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    bufferIndirectBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT; //current we don't read thin the sahder.*/ 
    bufferIndirectBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectBarrier.buffer = indirectBuffer->vkBuffer;
    bufferIndirectBarrier.offset = 0;
    bufferIndirectBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferIndirectCountBarrier{};
    bufferIndirectCountBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferIndirectCountBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferIndirectCountBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferIndirectCountBarrier.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    bufferIndirectCountBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    bufferIndirectCountBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectCountBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectCountBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferIndirectCountBarrier.offset = 0;
    bufferIndirectCountBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 barriers[] =
    {
        bufferIndirectBarrier,
        bufferIndirectCountBarrier
    };

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = 2;
    dependencyInfo.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyInfo);
}

void Game::deleteEntity() 
{
    if (Physics::instance().contactListener.toDeleteQueue.size > 0)
    {
        for (uint32_t i = 0; i < Physics::instance().contactListener.toDeleteQueue.size;)
        {
            uint32_t index = Physics::instance().contactListener.toDeleteQueue[i];
            scene.entities[index].isDeleted = true;
            scene.entityData[index].position = glms_mat4_identity();
            scene.entityData[index].position.m30 = FLT_MAX;
            scene.entityData[index].position.m31 = FLT_MAX;
            scene.entityData[index].position.m32 = FLT_MAX;

            Physics::instance().bodyInterface->DeactivateBody(scene.entities[index].bodyID);

            Physics::instance().contactListener.toDeleteQueue.pop();
        }
    }
}
