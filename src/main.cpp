#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/Keys.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/CommandBuffer.hpp"
#include "Graphics/VoidImgui.hpp"
#include "Graphics/GPUProfiler.hpp"
#include "Graphics/LoadGLTF.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include "vender/imgui/imgui.h"
//#include "vender/tracy/tracy/Tracy.hpp"

#include "Foundation/File.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/Time.hpp"
#include "Foundation/Array.hpp"

#include "Physics/Physics.hpp"

#include "Scene.hpp"

#include <stdlib.h>
#include <SDL3/SDL.h>
#include <vender/stb_image.h>

namespace
{
    //TODO: Figure out if you need this stuff.
    PipelineHandle cubePipeline;
    PipelineHandle skyboxPipeline;
    PipelineHandle debugPipeline;
    BufferHandle sceneBuffer;
    BufferHandle skyboxUniformBuffer;
    BufferHandle skyboxMaterialBuffer;
    DescriptorSetLayoutHandle mainDescriptorSetLayout;
    DescriptorSetLayoutHandle skyboxDescriptorSetLayout;
    DescriptorSetHandle skyboxDescriptorSet;

    BufferHandle positionalBuffer;
    BufferHandle debugRendererDataBuffer;

    struct SkyboxData
    {
        vec3s testColour;
        uint32_t skyboxTextureIndex;
    };

    struct UniformData
    {
        mat4s viewPerspective;
        mat4s globalModel;
        vec4s eye;
        vec4s light;
    };

    struct PushConstants
    {
        VkDeviceAddress vertexDataAddress;
        VkDeviceAddress modelPositionAddress;
        uint32_t index;
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
    TextureHandle createACubemap(GPUDevice& gpu, const Array<const char*>& images, const char* name)
    {
        Array<uint8_t*> skyboxImageArray;
        skyboxImageArray.init(&MemoryService::instance()->systemAllocator, 6);
        int comp;
        int width;
        int height;

        for (uint32_t i = 0; i < images.size; ++i)
        {
            if (images[i])
            {
                //Load 6 images.
                uint8_t* imageData = stbi_load(images[i], &width, &height, &comp, 4);
                if (imageData == nullptr)
                {
                    VOID_ERROR("Error loading texture %s", images[i]);
                    return INVALID_TEXTURE;
                }

                skyboxImageArray.push(imageData);
                free(imageData);
            }
        }

        //Create the single texture.
        TextureCreation creation{};
        creation.setData(skyboxImageArray.data)
            .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE)
            .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(width), 1)
            .setName(name);
        creation.layerCount = 6;
        TextureHandle newTexture = gpu.createTexture(creation);

        skyboxImageArray.shutdown();
        return newTexture;
    }
}

int main(int argc, char** argv)
{
    //Init services
    MemoryService::instance()->init(void_giga(1ull), void_mega(8));
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
    cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, 1, VK_SHADER_STAGE_ALL, "LocalConstants" })
        .addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, 1, VK_SHADER_STAGE_ALL, "MaterialConstant" })
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
    skyboxPipelineCreation.depthStencil.depthEnable = false;

    //Shader state
    FileReadResult vertSkybox = fileReadBinary("Assets/Shaders/skybox.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragSkybox = fileReadBinary("Assets/Shaders/skybox.frag.spv", &MemoryService::instance()->scratchAllocator);

    skyboxPipelineCreation.shaders.setName("skybox")
        .addStage(vertSkybox.data, uint32_t(vertSkybox.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragSkybox.data, uint32_t(fragSkybox.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    //Descriptor set layout.
    DescriptorSetLayoutCreation skyboxSetLayout{};
    skyboxSetLayout.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, 1, VK_SHADER_STAGE_ALL, "LocalConstants" })
        .addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, 1, VK_SHADER_STAGE_ALL, "SkyboxMaterial" })
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

    debugPipeline = gpu.createPipeline(debugPipelineCreation, /*debugRendering=*/ false);

    //Constant buffer
    BufferCreation uniformBufferCreation;
    uniformBufferCreation.reset()
        .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UniformData))
        .setName("sceneBuffer");
    sceneBuffer = gpu.createBuffer(uniformBufferCreation);

    Array<const char*> cubemapsImage;
    cubemapsImage.init(allocator, 6);
    cubemapsImage.push("Assets/Textures/1.png");
    cubemapsImage.push("Assets/Textures/2.png");
    cubemapsImage.push("Assets/Textures/3.png");
    cubemapsImage.push("Assets/Textures/4.png");
    cubemapsImage.push("Assets/Textures/5.png");
    cubemapsImage.push("Assets/Textures/6.png");

    TextureHandle skyboxTextureHandle = createACubemap(gpu, cubemapsImage, "SpaceCubeMap");
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

    Physics physics;
    physics.initPhysics();

    Scene scene;
    scene.initScene(allocator, gpu, sceneBuffer, mainDescriptorSetLayout);
    scene.buildScene(physics);

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    physics.physicsSystem.OptimizeBroadPhase();

    PushConstants pushConstants{};

    BufferCreation bufferCreation{};
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(EntityData) * scene.entityData.size)
        .setName("othername")
        .setData(scene.entityData.data);
    positionalBuffer = gpu.createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(DebugRendererData) * scene.totalColliders)
        .setName("debugRenderer")
        .setData(scene.debugRendererData.data);
    debugRendererDataBuffer = gpu.createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(SkyboxData))
        .setName("SkyboxData");
    skyboxMaterialBuffer = gpu.createBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UniformData))
        .setName("skyboxUniformDescriptor");
    skyboxUniformBuffer = gpu.createBuffer(bufferCreation);

    DescriptorSetCreation dsCreation{};
    dsCreation.buffer(skyboxUniformBuffer, 0);
    dsCreation.buffer(skyboxMaterialBuffer, 1);
    dsCreation.setLayout(skyboxDescriptorSetLayout);

    skyboxDescriptorSet = gpu.createDescriptorSet(dsCreation);

    int64_t beginFrameTick = timeNow();

    vec3s eye = vec3s{ 0.f, 2.5f, 2.f };

    GameCamera gameCamera;
    gameCamera.internal3DCamera.initPerspective(0.01f, 5000.f, 60.f, (float)Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(7.f, 3.0f, 0.1f);

    float modelScale = 0.1f;
    bool fullscreen = false;

    MapBufferParameters skyboxCBMap = { .buffer = skyboxUniformBuffer, .offset = 0, .size = 0 };
    void* skyboxCBData = gpu.mapBuffer(skyboxCBMap);

    MapBufferParameters cbMap = { .buffer = sceneBuffer, .offset = 0, .size = 0 };
    void* cbData = gpu.mapBuffer(cbMap);

    MapBufferParameters skyboxMaterialMap = { .buffer = skyboxMaterialBuffer, .offset = 0, .size = 0 };
    SkyboxData* skyboxMaterialBufferData = static_cast<SkyboxData*>(gpu.mapBuffer(skyboxMaterialMap));

    bool debugRenderer = true;
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
            //On Windows with an Nvidia graphics card and using SDL3 the synchronisation drifts when a window events when I'm using synchronisation1. 
            //This seem to be happening because the presentation engine or something internal driver thing is causing the frame to continue to push frames to the screen.
            //Meaning that when we restart the event loop for rendering after an Window event the currentFrame mis-matches with what the actual currentFrame is.
            //This happens when submitting the main queue for some work. The only fix I could find is idling the main queue at the beginning of every frame.
            //For some reason this causes everything to remain in sync when an Window even happens.
            vkQueueWaitIdle(gpu.vulkanQueue);

            //This is only false when we can't recreate the swapchain because of 0 height due to VK_ERROR_OUT_OF_DATE_KHR constantly being hit.
            //We still need to acquire an image to re-check if can now correctly fetch a swapchain image. 
            if (gpu.newFrame() == false)
            {
                continue;
            }

            if (Window::instance()->resizeRequested)
            {
                Window::instance()->resizeRequested = false;

                gpu.resize(Window::instance()->width, Window::instance()->height);
                gameCamera.internal3DCamera.setAspectRatio(Window::instance()->width * 1.f / Window::instance()->height);
            }

            if (inputHandler.isKeyJustReleased(Keys::KEY_1))
            {
                debugRenderer = !debugRenderer;
            }

            //NOTE: This must be after the OS messages.
            imgui->newFrame();

            if (ImGui::Begin("Void ImGui"))
            {
                ImGui::InputFloat("Model Scale", &modelScale, 0.001f);
            }
            ImGui::End();

            if (ImGui::Begin("GPU"))
            {
                gpuProfiler.imguiDraw();
            }
            ImGui::End();

            //Moves key pressed events stores then in a key-pressed array. This allows us to know if a key is being held down, rather than just pressed. 
            inputHandler.newFrame();
            //Saves the mouse position in screen coordinates and handles events that are for re-mapped key bindings 
            inputHandler.update();

            //I want the physics delta outside of the loop for now.
            const int64_t currentTick = timeNow();
            float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
            beginFrameTick = currentTick;

            physics.updatePhysics();
            
            gameCamera.update(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, deltaTime);
            Window::instance()->centerMouse(inputHandler.isMouseDragging(MouseButtons::MOUSE_BUTTON_RIGHT));

            CommandBuffer* gpuCommands = gpu.getCommandBuffer(VK_QUEUE_GRAPHICS_BIT, true);
            gpuCommands->pushMarker("Frame");

            gpu.beginRenderingTransition(gpuCommands);

            //gpuCommands->clear(0.7f, 0.9f, 1.f, 1.f);
            gpuCommands->clear(0.f, 0.f, 0.f, 1.f);
            gpuCommands->clearDepthStencil(0.f, 0);
            gpuCommands->beginRendering();

            //Skybox!
            gpuCommands->bindPipeline(skyboxPipeline);
            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            //Update the perspective matrix for the skybox.
            if (skyboxCBData)
            {
                //TODO: Match these name with what's in the shader.
                UniformData uniformData{};
                uniformData.viewPerspective = gameCamera.internal3DCamera.viewProjection;
                //It needs to have no translation.
                uniformData.viewPerspective.m30 = 0;
                uniformData.viewPerspective.m31 = 0;
                uniformData.viewPerspective.m32 = 0;
                uniformData.viewPerspective.m33 = 1;
                memcpy(skyboxCBData, &uniformData, sizeof(UniformData));
            }

            //Maybe we can make this non-dymanic after things are working?
            if (skyboxMaterialBufferData)
            {
                SkyboxData skyboxData{};
                skyboxData.skyboxTextureIndex = skyboxTextureHandle.index;
                skyboxData.testColour = vec3s{ 0.f, 1.f, 0.f };
                memcpy(skyboxMaterialBufferData, &skyboxData, sizeof(SkyboxData));
            }

            gpuCommands->bindDescriptorSet(&skyboxDescriptorSet, 1, nullptr, 0, 0);
            gpuCommands->bindlessDescriptorSet(1);

            gpuCommands->draw(36, 1, 0, 0);

            //Scene
            gpuCommands->bindPipeline(cubePipeline);
            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            gpuCommands->bindlessDescriptorSet(1);

            //Update rotating cube data.
            mat4s globalModel{};
            if (cbData)
            {
                globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

                //TODO: Match these name with what's in the shader.
                UniformData uniformData{};
                uniformData.viewPerspective = gameCamera.internal3DCamera.viewProjection;
                uniformData.globalModel = globalModel;
                uniformData.eye = vec4s{ eye.x, eye.y, eye.z, 1.f };
                uniformData.light = vec4s{ gameCamera.internal3DCamera.position.x, gameCamera.internal3DCamera.position.y, gameCamera.internal3DCamera.position.z, 1.f };

                memcpy(cbData, &uniformData, sizeof(UniformData));
            }

            Buffer* positionBuff = gpu.accessBuffer(positionalBuffer);
            pushConstants.modelPositionAddress = positionBuff->bufferAddress;

            uint32_t physicsMarker = scratchAllocator.getMarker();
            Array<EntityData> physicsUpdateDataArray;
            physicsUpdateDataArray.init(&scratchAllocator, 4);

            for (uint32_t entityIndex = 0; entityIndex < scene.totalEntities; ++entityIndex)
            {
                const Entity& entity = scene.entities[entityIndex];
                pushConstants.index = entityIndex;

                if (entity.debugRendererIndex != UINT32_MAX)
                {
                    JPH::RMat44 newPos = physics.bodyInterface->GetWorldTransform(scene.entities[entity.debugRendererIndex].bodyID);

                    EntityData physicsPosition{};
                    physicsPosition.pos = convertToMat4(newPos);
                    physicsPosition.colour = scene.entityData[entityIndex].colour;

                    physicsUpdateDataArray.push(physicsPosition);
                }

                for (uint32_t meshIndex = 0; meshIndex < scene.models[entity.modelIndex].meshDraws.size; ++meshIndex)
                {
                    MeshDraw meshDraw = scene.models[entity.modelIndex].meshDraws[meshIndex];

                    MapBufferParameters materialMap = { meshDraw.materialBuffer, 0, 0 };
                    MaterialData* materialBufferData = reinterpret_cast<MaterialData*>(gpu.mapBuffer(materialMap));

                    uploadMaterial(*materialBufferData, meshDraw);
                    gpu.unmapBuffer(materialMap);

                    Buffer* vertexDataBuf = gpu.accessBuffer(meshDraw.vertexBuffer);
                    pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                    vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

                    gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);
                    gpuCommands->bindDescriptorSet(&meshDraw.descriptorSet, 1, nullptr, 0, 0);

                    gpuCommands->drawIndexed(meshDraw.count, 1, 0, 0, 0);
                }
            }
            
            vmaCopyMemoryToAllocation(gpu.VMAAllocator, physicsUpdateDataArray.data, positionBuff->vmaAllocation, 0, sizeof(EntityData) * physicsUpdateDataArray.size);
            scratchAllocator.freeMarker(physicsMarker);

            if (debugRenderer)
            {
                //Debug
                gpuCommands->bindPipeline(debugPipeline);
                gpuCommands->setScissor(nullptr);
                gpuCommands->setViewport(nullptr);

                uint32_t debugRendererMarker = scratchAllocator.getMarker();

                Array<DebugRendererData> debugRenderingDataArray;
                debugRenderingDataArray.init(&scratchAllocator, 4);

                Buffer* debugBufferRendererData = gpu.accessBuffer(debugRendererDataBuffer);
                pushConstants.modelPositionAddress = debugBufferRendererData->bufferAddress;
                for (uint32_t entityIndex = 0; entityIndex < scene.totalEntities; ++entityIndex)
                {
                    const Entity& entity = scene.entities[entityIndex];
                    if (entity.debugRendererIndex != UINT32_MAX)
                    {
                        globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

                        JPH::RMat44 newPos = physics.bodyInterface->GetWorldTransform(scene.entities[entityIndex].bodyID);
                        
                        DebugRendererData debugRenderData{};
                        debugRenderData.position = convertToMat4(newPos);
                        debugRenderData.globalModel = globalModel;
                        debugRenderData.viewPerspective = gameCamera.internal3DCamera.viewProjection;
                        debugRenderData.model = scene.debugRendererData[entityIndex].model;
                        debugRenderData.colour = scene.debugRendererData[entityIndex].colour;

                        pushConstants.index = entity.positionIndex;
                        VOID_ASSERTM(scene.models[entity.modelIndex].meshDraws.size == 1, "Collider geometry have have one draw call.\n");

                        MeshDraw meshDraw = scene.models[scene.debugSphereIndex].meshDraws[0];

                        Buffer* vertexDataBuf = gpu.accessBuffer(meshDraw.vertexBuffer);
                        pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                        vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

                        gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);

                        gpuCommands->drawIndexed(meshDraw.count, 1, 0, 0, 0);

                        debugRenderingDataArray.push(debugRenderData);
                    }
                }

                vmaCopyMemoryToAllocation(gpu.VMAAllocator, debugRenderingDataArray.data, debugBufferRendererData->vmaAllocation, 0, sizeof(DebugRendererData) * debugRenderingDataArray.size);
                scratchAllocator.freeMarker(debugRendererMarker);
            }
            imgui->render(*gpuCommands);

            gpuCommands->popMarker();

            gpuProfiler.update(gpu);

            gpu.queueCommandBuffer(gpuCommands);
            gpu.present();
        }
        else
        {
            ImGui::Render();
        }

        //FrameMark;
    }

    vkDeviceWaitIdle(gpu.vulkanDevice);

    //gpu.unmapBuffer(debugRendererDataMap);
    //gpu.unmapBuffer(positionMap);
    gpu.unmapBuffer(cbMap);
    gpu.unmapBuffer(skyboxMaterialMap);
    gpu.unmapBuffer(skyboxCBMap);

    gpu.destroyBuffer(positionalBuffer);

    gpu.destroyDescriptorSet(skyboxDescriptorSet);
    gpu.destroyBuffer(skyboxMaterialBuffer);
    gpu.destroyBuffer(skyboxUniformBuffer);
    gpu.destroyBuffer(debugRendererDataBuffer);

    scene.shutdownScene(gpu, physics);

    gpu.destroySampler(skyboxSampler);
    gpu.destroyTexture(skyboxTextureHandle);

    gpu.destroyBuffer(sceneBuffer);
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