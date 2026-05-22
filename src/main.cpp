#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/Keys.hpp"
#include "Application/Audio.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/CommandBuffer.hpp"
#include "Graphics/VoidImgui.hpp"
#include "Graphics/GPUProfiler.hpp"
#include "Graphics/LoadGLTF.hpp"

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

#include "Game/Player.hpp"
#include "Game/Scene.hpp"

#include <stdlib.h>
#include <SDL3/SDL.h>
#include <vender/stb_image.h>

namespace
{
    //TODO: Figure out if you need this stuff.
    PipelineHandle cubePipeline;
    PipelineHandle skyboxPipeline;
    PipelineHandle debugPipeline;
    //BufferHandle skyboxUniformBuffer;
    BufferHandle skyboxMaterialBuffer;
    DescriptorSetLayoutHandle mainDescriptorSetLayout;
    DescriptorSetLayoutHandle skyboxDescriptorSetLayout;
    DescriptorSetHandle skyboxDescriptorSet;

    BufferHandle positionalBuffer;
    BufferHandle debugRendererDataBuffer;
    BufferHandle debugGlobalBuffer;

    struct SkyboxData
    {
        vec3s testColour;
        uint32_t skyboxTextureIndex;
    };

    struct UniformData
    {
        mat4s viewPerspective;
        mat4s view;
        mat4s project;
        mat4s globalModel;
        vec4s eye;
        vec4s light;
    };

    struct PushConstants
    {
        VkDeviceAddress vertexDataAddress;
        VkDeviceAddress modelPositionAddress;
        VkDeviceAddress sceneAddress;
    };

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
        meshData.flags = meshDraw.flags;

        // NOTE: for left-handed systems (as defined in cglm) need to invert positive and negative Z.
        mat4s model = meshDraw.model;
        meshData.model = model;
        meshData.modelInv = glms_mat4_inv(glms_mat4_transpose(model));
    }

    static constexpr uint16_t INVALID_SCENE_TEXTURE_INDEX = UINT16_MAX;

    //TODO: Move this to a place that make sense.
    TextureHandle createACubemap(GPUDevice& gpu, const Array<const char*>& images, Array<uint8_t*> &skyboxImageArray, const char* name)
    {
        int comp;
        int width;
        int height;

        for (uint32_t i = 0; i < images.size; ++i)
        {
            if (images[i])
            {
                stbi_set_flip_vertically_on_load(1);
                //Load 6 images.
                uint8_t* imageData = stbi_load(images[i], &width, &height, &comp, 4);
                if (imageData == nullptr)
                {
                    VOID_ERROR("Error loading texture %s", images[i]);
                    return INVALID_TEXTURE;
                }

                skyboxImageArray.push(imageData);
                //free(imageData);
            }
        }

        //Create the single texture.
        TextureCreation creation{};
        creation.setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE)
            .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(width), 1)
            .setImages(skyboxImageArray, skyboxImageArray.size)
            .setName(name);
        TextureHandle newTexture = gpu.createTexture(creation);

        return newTexture;
    }
}

int main(int argc, char** argv)
{
    //Init services
    MemoryService::instance()->init(/*heapSize=*/ void_giga(1ull), /*stackSize=*/ void_mega(8), /*physicsStackSiz=*/ void_mega(40));
    timeServiceInit();

    HeapAllocator* allocator = &MemoryService::instance()->systemAllocator;
    StackAllocator scratchAllocator = MemoryService::instance()->scratchAllocator;

    Window::instance()->init(1280, 800, "Void Engine");

    InputHandler inputHandler{};
    inputHandler.init(allocator);

    DeviceCreation deviceCreation;
    deviceCreation.setWindow(Window::instance()->width, Window::instance()->height, Window::instance()->platformHandle)
        .setAllocator(allocator)
        .setLinearAllocator(&scratchAllocator);

    GPUDevice gpu;
    gpu.init(deviceCreation);

    GPUProfiler gpuProfiler;
    gpuProfiler.init(allocator, 100);

    ImguiService* imgui = ImguiService::instance();
    ImguiServiceConfiguration imguiConfig = { &gpu, Window::instance()->platformHandle };
    imgui->init(&imguiConfig);
 
    //Window::instance()->setFullscreen(true);

    //Create pipeline state
    PipelineCreation pipelineCreation;

    //Depth
    pipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //Shader state
    FileReadResult vertexShaderCode = fileReadBinary("Assets/Shaders/coreShader.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragShaderCode = fileReadBinary("Assets/Shaders/coreShader.frag.spv", &MemoryService::instance()->scratchAllocator);

    pipelineCreation.shaders.setName("Cube")
        .addStage(vertexShaderCode.data, uint32_t(vertexShaderCode.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragShaderCode.data, uint32_t(fragShaderCode.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    //Descriptor set layout.
    DescriptorSetLayoutCreation cubeRLLCreation{};
    cubeRLLCreation
        .addBinding({ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .binding = 0, .count = 1, .stage = VK_SHADER_STAGE_ALL, .name = "MaterialConstant" })
        .setSetIndex(0);
    cubeRLLCreation.bindless = false;

    //Setting it into pipeline.
    //This descriptor set layout will be ran every draw calls
    mainDescriptorSetLayout = gpu.createDescriptorSetLayout(cubeRLLCreation);
    //This descriptor set layout will be ran every frame
    pipelineCreation.addDescriptorSetLayout(mainDescriptorSetLayout)
        .addDescriptorSetLayout(gpu.bindlessDescriptorSetLayoutHandle);

    cubePipeline = gpu.createPipeline(pipelineCreation);

    //Depth
    PipelineCreation skyboxPipelineCreation{};
    skyboxPipelineCreation.depthStencil.depthEnable = true;
    skyboxPipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //Shader state
    FileReadResult vertSkybox = fileReadBinary("Assets/Shaders/skybox.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragSkybox = fileReadBinary("Assets/Shaders/skybox.frag.spv", &MemoryService::instance()->scratchAllocator);

    skyboxPipelineCreation.shaders.setName("skybox")
        .addStage(vertSkybox.data, uint32_t(vertSkybox.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragSkybox.data, uint32_t(fragSkybox.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    //Descriptor set layout.
    DescriptorSetLayoutCreation skyboxSetLayout{};
    skyboxSetLayout
        .addBinding({ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .binding = 0, .count = 1, .stage = VK_SHADER_STAGE_ALL, .name = "SkyboxMaterial" })
        .setSetIndex(0);
    skyboxSetLayout.bindless = false;

    //Setting it into pipeline.
    //This descriptor set layout will be ran every draw calls
    skyboxDescriptorSetLayout = gpu.createDescriptorSetLayout(skyboxSetLayout);
    //This descriptor set layout will be ran every frame
    skyboxPipelineCreation.addDescriptorSetLayout(skyboxDescriptorSetLayout)
        .addDescriptorSetLayout(gpu.bindlessDescriptorSetLayoutHandle);

    skyboxPipeline = gpu.createPipeline(skyboxPipelineCreation);

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

    debugPipeline = gpu.createPipeline(debugPipelineCreation, /*debugRendering=*/ true);

    Array<uint8_t*> skyboxImageArray;
    skyboxImageArray.init(&MemoryService::instance()->systemAllocator, 6);

    Array<const char*> cubemapsImage;
    cubemapsImage.init(allocator, 6);
    cubemapsImage.push("Assets/Textures/4.png");
    cubemapsImage.push("Assets/Textures/2.png");
    cubemapsImage.push("Assets/Textures/6.png");
    cubemapsImage.push("Assets/Textures/5.png");
    cubemapsImage.push("Assets/Textures/1.png");
    cubemapsImage.push("Assets/Textures/3.png");

    TextureHandle skyboxTextureHandle = createACubemap(gpu, cubemapsImage, skyboxImageArray, "SpaceCubeMap");
    SamplerCreation skyboxSamplerCreation{};
    skyboxSamplerCreation.minFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.magFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerHandle skyboxSampler = gpu.createSampler(skyboxSamplerCreation);
    gpu.linkTextureSampler(skyboxTextureHandle, skyboxSampler);

    cubemapsImage.shutdown();

    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
    // This needs to be done before any other Jolt function is called.
    JPH::RegisterDefaultAllocator();

    Physics::instance();

    Scene scene;
    scene.initScene(allocator, gpu, mainDescriptorSetLayout);
    scene.buildScene(Physics::instance());

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    Physics::instance().physicsSystem.OptimizeBroadPhase();

    PushConstants pushConstants{};

    BufferCreation bufferCreation{};
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(EntityData) * scene.entityData.size)
        .setName("othername")
        .setData(scene.entityData.data);
    positionalBuffer = gpu.createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(DebugRendererData) * scene.debugRendererData.size)
        .setName("debugRenderer")
        .setData(scene.debugRendererData.data);
    debugRendererDataBuffer = gpu.createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(UniformData))
        .setName("debugGlobalBuffer");
    debugGlobalBuffer = gpu.createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(SkyboxData))
        .setName("SkyboxData");
    skyboxMaterialBuffer = gpu.createBuffer(bufferCreation);

    DescriptorSetCreation dsCreation{};
    dsCreation.buffer(skyboxMaterialBuffer, 0);
    dsCreation.setLayout(skyboxDescriptorSetLayout);

    skyboxDescriptorSet = gpu.createDescriptorSet(dsCreation);

    int64_t beginFrameTick = timeNow();

    vec3s eye = vec3s{ 0.f, 1.f, 0.f };

    GameCamera gameCamera;
    gameCamera.internal3DCamera.initPerspective(0.01f, 5000.f, 60.f, (float)Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(7.f, 3.0f, 0.1f);

    float modelScale = 1.0f;
    bool fullscreen = false;

    AudioSystem audioSystem;
    audioSystem.init();
    audioSystem.loadAudio();

    Player player;
    player.init();

    bool debugRenderer = true;
    UniformData globalDebugData{};
    vec3s playerPosition{};
    while (Window::instance()->exitRequested == false)
    {
        //ZoneScoped;
        inputHandler.onEvent(&gpu);
        if (inputHandler.isKeyDown(Keys::KEY_ESCAPE))
        {
            Window::instance()->exitRequested = true;
        }
        else if (inputHandler.isKeyJustReleased(Keys::KEY_F))
        {
            fullscreen = !fullscreen;
            Window::instance()->setFullscreen(fullscreen);
        }

        //New Frame
        if (Window::instance()->minimised == false)
        {
            playerPosition = convertToVec3(player.character->GetPosition());

            //This is only false when we can't recreate the swapchain because of 0 height due to VK_ERROR_OUT_OF_DATE_KHR constantly being hit.
            //We still need to acquire an image to re-check if can now correctly fetch a swapchain image. 
            if (gpu.newFrame() == false)
            {
                continue;
            }

            mat4s globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

            if (Window::instance()->resizeRequested)
            {
                Window::instance()->resizeRequested = false;

                gpu.resize(Window::instance()->width, Window::instance()->height);
                gameCamera.internal3DCamera.setAspectRatio(Window::instance()->width * 1.f / Window::instance()->height);
            }

            player.handleEvents(inputHandler, convertToVec3JPH(gameCamera.internal3DCamera.direction));

            if (inputHandler.isKeyJustReleased(Keys::KEY_1))
            {
                debugRenderer = !debugRenderer;
            }
            else if (inputHandler.isKeyJustReleased(Keys::KEY_SPACE))
            {
                audioSystem.playSoundEffect(sfx::Lazer);
            }
            else if (inputHandler.isKeyJustReleased(Keys::KEY_R))
            {
                player.resetPosition();
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

            //Audio
            //audio.play();
            
            //gameCamera.update(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, deltaTime);
            gameCamera.updatePlayerCamera(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, playerPosition, {0.f, 0.f, 0.f, 0.f}, deltaTime);
            Window::instance()->centerMouse(inputHandler.isMouseDragging(MouseButtons::MOUSE_BUTTON_RIGHT));

            CommandBuffer* gpuCommands = gpu.getCommandBuffer(VK_QUEUE_GRAPHICS_BIT, true);
            gpuCommands->pushMarker("Frame");

            gpu.beginRenderingTransition(gpuCommands);
            gpuCommands->beginRendering();
            //gpuCommands->clear(0.7f, 0.9f, 1.f, 1.f);
            gpuCommands->clear(1.f, 1.f, 1.f, 1.f);
            gpuCommands->clearDepthStencil(0.f, 0);

            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            Buffer* debugGlobalData = gpu.accessBuffer(debugGlobalBuffer);
            pushConstants.sceneAddress = debugGlobalData->bufferAddress;
            pushConstants.vertexDataAddress = 0;
            pushConstants.modelPositionAddress = 0;

            globalDebugData.globalModel = globalModel;
            globalDebugData.view = gameCamera.internal3DCamera.view;
            globalDebugData.project = gameCamera.internal3DCamera.projection;
            globalDebugData.viewPerspective = gameCamera.internal3DCamera.viewProjection;
            globalDebugData.eye = vec4s{ eye.x, eye.y, eye.z, 1.f };
            vec3s lightPosition = glms_vec3_add(playerPosition, glms_vec3_scale(gameCamera.internal3DCamera.direction, 10.f));
            globalDebugData.light = vec4s{ lightPosition.x, lightPosition.y, lightPosition.z, 1.f };

            //Scene
            gpuCommands->bindPipeline(cubePipeline);

            gpuCommands->bindlessDescriptorSet(1);

            Buffer* positionBuff = gpu.accessBuffer(positionalBuffer);
            pushConstants.modelPositionAddress = positionBuff->bufferAddress;

            uint32_t physicsMarker = scratchAllocator.getMarker();
            Array<EntityData> physicsUpdateDataArray;
            physicsUpdateDataArray.init(&scratchAllocator, scene.totalEntities);

            vmaCopyMemoryToAllocation(gpu.VMAAllocator, &globalDebugData, debugGlobalData->vmaAllocation, 0, sizeof(UniformData));

            for (uint32_t entityIndex = 0; entityIndex < scene.totalEntities; ++entityIndex)
            {
                const Entity& entity = scene.entities[entityIndex];

                if (entity.isPlayer == false)
                {
                    if (entity.debugRendererIndex != UINT32_MAX && entity.isDynamic)
                    {
                        EntityData physicsPosition{};
                        JPH::RMat44 newPos = Physics::instance().bodyInterface->GetWorldTransform(scene.entities[entity.debugRendererIndex].bodyID);
                        physicsPosition.pos = convertToMat4(newPos);
                        physicsPosition.colour = scene.entityData[entityIndex].colour;

                        physicsUpdateDataArray.push(physicsPosition);
                    }
                }
                else
                {
                    player.update(deltaTime);
                    EntityData physicsPosition{};
                    JPH::RMat44 newPos = Physics::instance().bodyInterface->GetWorldTransform(player.character->GetBodyID());
                    physicsPosition.pos = convertToMat4(newPos);
                    physicsPosition.colour = scene.entityData[0].colour;

                    physicsUpdateDataArray.push(physicsPosition);
                }
            }

            vmaCopyMemoryToAllocation(gpu.VMAAllocator, physicsUpdateDataArray.data, positionBuff->vmaAllocation, 0, sizeof(EntityData) * physicsUpdateDataArray.size);
            scratchAllocator.freeMarker(physicsMarker);

            for (uint32_t modelIndexType = 0; modelIndexType < scene.models.size; ++modelIndexType)
            {
                for (uint32_t meshIndex = 0; meshIndex < scene.models[modelIndexType].meshDraws.size; ++meshIndex)
                {
                    MeshDraw meshDraw = scene.models[modelIndexType].meshDraws[meshIndex];

                    MapBufferParameters materialMap = { meshDraw.materialBuffer, 0, 0 };
                    MaterialData* materialBufferData = reinterpret_cast<MaterialData*>(gpu.mapBuffer(materialMap));

                    uploadMaterial(*materialBufferData, meshDraw);
                    gpu.unmapBuffer(materialMap);

                    Buffer* vertexDataBuf = gpu.accessBuffer(meshDraw.vertexBuffer);
                    pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                    vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

                    gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);
                    gpuCommands->bindDescriptorSet(&meshDraw.descriptorSet, 1, nullptr, 0, 0);

                    if (modelIndexType == scene.models.size - 1)
                    {
                        gpuCommands->drawIndexed(meshDraw.count, scene.models[modelIndexType].countOfModelType, 0, 0, 0);
                    }
                    else
                    {
                        gpuCommands->drawIndexed(meshDraw.count, scene.models[modelIndexType].countOfModelType, 0, 0, scene.models[modelIndexType + 1].countOfModelType);
                    }
                }
            }

            if (debugRenderer)
            {
                //Debug
                gpuCommands->bindPipeline(debugPipeline);

                uint32_t debugRendererMarker = scratchAllocator.getMarker();

                Array<DebugRendererData> debugRenderingDataArray;
                debugRenderingDataArray.init(&scratchAllocator, scene.totalEntities);

                Buffer* debugBufferRendererData = gpu.accessBuffer(debugRendererDataBuffer);
                pushConstants.modelPositionAddress = debugBufferRendererData->bufferAddress;

                globalDebugData.globalModel = globalModel;
                globalDebugData.viewPerspective = gameCamera.internal3DCamera.viewProjection;

                pushConstants.sceneAddress = debugGlobalData->bufferAddress;

                vmaCopyMemoryToAllocation(gpu.VMAAllocator, &globalDebugData, debugGlobalData->vmaAllocation, 0, sizeof(UniformData));

                for (uint32_t entityIndex = 0; entityIndex < scene.totalEntities; ++entityIndex)
                {
                    const Entity& entity = scene.entities[entityIndex];
                    if (entity.isPlayer == false)
                    {
                        if (entity.debugRendererIndex != UINT32_MAX && entity.isDynamic)
                        {
                            DebugRendererData debugRenderData{};
                            JPH::RMat44 newPos = Physics::instance().bodyInterface->GetWorldTransform(scene.entities[entityIndex].bodyID);
                            debugRenderData.position = convertToMat4(newPos);
                            debugRenderData.model = scene.debugRendererData[entityIndex].model;
                            debugRenderData.colour = scene.debugRendererData[entityIndex].colour;

                            debugRenderingDataArray.push(debugRenderData);
                        }
                    }
                    else 
                    {
                        DebugRendererData debugRenderData{};
                        JPH::RMat44 newPos = Physics::instance().bodyInterface->GetWorldTransform(player.character->GetBodyID());
                        debugRenderData.position = convertToMat4(newPos);
                        debugRenderData.position.m31 += 0.6f;
                        debugRenderData.position.m30 += 0.2f;
                        debugRenderData.model = scene.debugRendererData[entityIndex].model;
                        debugRenderData.colour = scene.debugRendererData[entityIndex].colour;

                        debugRenderingDataArray.push(debugRenderData);
                    }
                }

                vmaCopyMemoryToAllocation(gpu.VMAAllocator, debugRenderingDataArray.data, debugBufferRendererData->vmaAllocation, 0, sizeof(DebugRendererData)* debugRenderingDataArray.size);
                scratchAllocator.freeMarker(debugRendererMarker);

                for (uint32_t modelIndexType = 0; modelIndexType < scene.debugModels.size; ++modelIndexType)
                {
                    VOID_ASSERTM(scene.debugModels[modelIndexType].meshDraws.size == 1, "Collider geometry have have one draw call.\n");

                    MeshDraw meshDraw = scene.debugModels[modelIndexType].meshDraws[0];

                    Buffer* vertexDataBuf = gpu.accessBuffer(meshDraw.vertexBuffer);
                    pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                    vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

                    gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);

                    //TODO: Add the data structures needed to allow for this.
                    if (modelIndexType == scene.debugModels.size - 1)
                    {
                        gpuCommands->drawIndexed(meshDraw.count, scene.debugModels[modelIndexType].countOfModelType, 0, 0, 0);
                    }
                    else
                    {
                        gpuCommands->drawIndexed(meshDraw.count, scene.debugModels[modelIndexType].countOfModelType, 0, 0, scene.debugModels[modelIndexType + 1].countOfModelType);
                    }
                }
            }

            //Skybox!
            gpuCommands->bindPipeline(skyboxPipeline);

            //Maybe we can make this non-dymanic after things are working?
            Buffer* skyboxMaterialDataBuffer = gpu.accessBuffer(skyboxMaterialBuffer);

            SkyboxData skyboxData{};
            skyboxData.skyboxTextureIndex = skyboxTextureHandle.index;
            skyboxData.testColour = vec3s{ 0.f, 1.f, 0.f };

            vmaCopyMemoryToAllocation(gpu.VMAAllocator, &skyboxData, skyboxMaterialDataBuffer->vmaAllocation, 0, sizeof(SkyboxData));

            vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

            gpuCommands->bindDescriptorSet(&skyboxDescriptorSet, 1, nullptr, 0, 0);
            gpuCommands->bindlessDescriptorSet(1);

            vmaCopyMemoryToAllocation(gpu.VMAAllocator, &globalDebugData, debugGlobalData->vmaAllocation, 0, sizeof(UniformData));

            gpuCommands->draw(36, 1, 0, 0);

            //imgui->render(*gpuCommands);

            gpuCommands->popMarker();

            gpuProfiler.update(gpu);

            gpu.queueCommandBuffer(gpuCommands);
            gpu.present();
        }
        else
        {
            //ImGui::Render();
        }

        //FrameMark;
    }

    vkDeviceWaitIdle(gpu.vulkanDevice);

    audioSystem.shutdown();
    skyboxImageArray.shutdown();

    gpu.destroyBuffer(positionalBuffer);

    gpu.destroyDescriptorSet(skyboxDescriptorSet);
    gpu.destroyBuffer(skyboxMaterialBuffer);
    //gpu.destroyBuffer(skyboxUniformBuffer);
    gpu.destroyBuffer(debugRendererDataBuffer);
    gpu.destroyBuffer(debugGlobalBuffer);

    scene.shutdownScene(gpu, Physics::instance());

    gpu.destroySampler(skyboxSampler);
    gpu.destroyTexture(skyboxTextureHandle);

    gpu.destroyDescriptorSetLayout(mainDescriptorSetLayout);
    gpu.destroyDescriptorSetLayout(skyboxDescriptorSetLayout);
    gpu.destroyPipeline(cubePipeline);
    gpu.destroyPipeline(skyboxPipeline);
    gpu.destroyPipeline(debugPipeline);

    imgui->shutdown();

    gpuProfiler.shutdown();

    gpu.shutdown();

    inputHandler.shutdown();
    Window::instance()->shutdown();

    MemoryService::instance()->shutdown();

    return 0;
}