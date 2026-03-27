#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/Keys.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
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
#include "Foundation/ResourceManager.hpp"
#include "Foundation/Time.hpp"

#include <stdlib.h>
#include <SDL3/SDL.h>

//static const char* DEFAULT_3D_MODEL = "Assets/Models/2.0/Sponza/glTF/Sponza.gltf";
//static const char* DEFAULT_3D_MODEL = "Assets/Models/out/Sponza5.glb";
//static const char* DEFAULT_3D_MODEL = "Assets/Models/out/Duck.glb";
static const char* DEFAULT_3D_MODEL = "Assets/Models/out/rock.glb";
//static const char* DEFAULT_3D_MODEL = "Assets/Models/out/riggedModel.glb";

//I might try to remove this later.
#define InjectDefault3DModel() \
if (fileExists(DEFAULT_3D_MODEL)) \
{\
    argc = 2;\
    argv[1] = const_cast<char*>(DEFAULT_3D_MODEL);\
}\
else \
{\
    vprint("Could not find file.");\
    exit(-1);\
}\

namespace
{
    //TODO: Figure out if you need this stuff.
    PipelineHandle cubePipeline;
    PipelineHandle skyboxPipeline;
    BufferHandle sceneBuffer;
    BufferHandle skyboxUniformBuffer;
    BufferHandle skyboxMaterialBuffer;
    DescriptorSetLayoutHandle mainDescriptorSetLayout;
    DescriptorSetLayoutHandle skyboxDescriptorSetLayout;
    DescriptorSetHandle skyboxDescriptorSet;

    BufferHandle positionalBuffer;

    struct SkyboxData
    {
        vec3s testColour;
        uint32_t skyboxTextureIndex;
    };

    struct UniformData
    {
        mat4s globalModel;
        mat4s viewPerspective;
        vec4s eye;
        vec4s light;
    };

    struct PushConstants
    {
        VkDeviceAddress modelPositionAddress;
        VkDeviceAddress vertexDataAddress;
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
}

struct Entity 
{
    //If we do this we can have a gaint bindless positionally buffer that has everything in it we just index into the that position array.
    uint32_t positionIndex;
    //We can loop through all the entities and use that model index to fetch the meshDraw to be able to draw all the models regardless of the model.
    uint32_t modelIndex;
};

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        vprint("Setting the Sponza GLTF model.\n");
        InjectDefault3DModel();
    }

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

    ResourceManager resourceManager;
    resourceManager.init(allocator);

    GPUProfiler gpuProfiler;
    gpuProfiler.init(allocator, 100);

    Renderer renderer;
    renderer.init({ &gpu, allocator });
    renderer.setLoaders(&resourceManager);

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
    PipelineCreation pipelineCreation2{};
    pipelineCreation2.depthStencil.depthEnable = false;

    //Shader state
    FileReadResult vertSkybox = fileReadBinary("Assets/Shaders/skybox.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragSkybox = fileReadBinary("Assets/Shaders/skybox.frag.spv", &MemoryService::instance()->scratchAllocator);

    pipelineCreation2.shaders.setName("skybox")
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
    pipelineCreation2.addDescriptorSetLayout(skyboxDescriptorSetLayout)
        .addDescriptorSetLayout(gpu.bindlessDescriptorSetLayoutHandle);

    skyboxPipeline = gpu.createPipeline(pipelineCreation2);

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

    TextureResource* skyboxTextureResource = renderer.createSkybox(cubemapsImage, "SpaceCubeMap");
    SamplerCreation skyboxSamplerCreation{};
    skyboxSamplerCreation.minFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.magFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerHandle skyboxSampler = gpu.createSampler(skyboxSamplerCreation);
    gpu.linkTextureSampler(skyboxTextureResource->handle, skyboxSampler);

    cubemapsImage.shutdown();

    srand(42);

    uint32_t totalDucks = 1111;
    Array<mat4s> drawMatrices;
    drawMatrices.init(allocator, totalDucks, totalDucks);
    Array<Entity> entities;
    entities.init(allocator, totalDucks, totalDucks);

    constexpr uint32_t rockModelIndex = 0;
    constexpr uint32_t duckModelIndex = 1;
    Array<Model> models;
    models.init(allocator, 2, 2);
    models[rockModelIndex].loadModel(DEFAULT_3D_MODEL, gpu, renderer, sceneBuffer, mainDescriptorSetLayout);
    models[duckModelIndex].loadModel("Assets/Models/out/Duck.glb", gpu, renderer, sceneBuffer, mainDescriptorSetLayout);

    float sceneRadius = 5000.f;
    for (uint32_t i = 0; i < totalDucks; ++i)
    {
        vec3s postion{};

        postion.x = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        postion.y = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        postion.z = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;

        float rotx = ((float(rand()) / RAND_MAX) * 2 - 1);
        float roty = ((float(rand()) / RAND_MAX) * 2 - 1);
        float rotz = ((float(rand()) / RAND_MAX) * 2 - 1);

        vec3s axis = glms_normalize({ rotx, roty, rotz });
        float angle = (float(rand()) / RAND_MAX) * M_PI_4;

        vec3s scaledVector = glms_vec3_scale(axis, sinf(angle * 0.5f));

        drawMatrices[i] = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(postion));

        entities[i].positionIndex = i;
        if (i % 2 == 0)
        {
            entities[i].modelIndex = rockModelIndex;
        }
        else 
        {
            entities[i].modelIndex = duckModelIndex;
        }
    }

    PushConstants pushConstants{};

    BufferCreation bufferCreation{};
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(mat4s) * drawMatrices.size)
        .setName("othername")
        .setData(drawMatrices.data);
    positionalBuffer = gpu.createBindlessBuffer(bufferCreation);

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
    gameCamera.internal3DCamera.initPerspective(0.01f, 1000.f, 60.f, (float)Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(7.f, 3.0f, 0.1f);

    float modelScale = 0.1f;
    bool fullscreen = false;

    MapBufferParameters skyboxCBMap = { skyboxUniformBuffer, 0, 0 };
    void* skyboxCBData = gpu.mapBuffer(skyboxCBMap);

    MapBufferParameters cbMap = { sceneBuffer, 0, 0 };
    void* cbData = gpu.mapBuffer(cbMap);

    MapBufferParameters skyboxMaterialMap = { skyboxMaterialBuffer, 0, 0 };
    SkyboxData* skyboxMaterialBufferData = reinterpret_cast<SkyboxData*>(gpu.mapBuffer(skyboxMaterialMap));

    vec3s newPosition{ 0 };

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

            inputHandler.newFrame();
            inputHandler.update();
            gameCamera.update(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, deltaTime);
            Window::instance()->centerMouse(inputHandler.isMouseDragging(MouseButtons::MOUSE_BUTTON_RIGHT));

            CommandBuffer* gpuCommands = gpu.getCommandBuffer(VK_QUEUE_GRAPHICS_BIT, true);
            gpuCommands->pushMarker("Frame");

            gpu.beginRenderingTransition(gpuCommands);

            //gpuCommands->clear(0.7f, 0.9f, 1.f, 1.f);
            gpuCommands->clear(1.f, 0.f, 1.f, 1.f);
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
                skyboxData.skyboxTextureIndex = skyboxTextureResource->handle.index;
                skyboxData.testColour = vec3s{ 0.f, 1.f, 0.f };
                memcpy(skyboxMaterialBufferData, &skyboxData, sizeof(SkyboxData));
            }

            gpuCommands->bindDescriptorSet(&skyboxDescriptorSet, 1, nullptr, 0, 0);
            //Only do this once.
            gpuCommands->bindlessDescriptorSet(1);

            gpuCommands->draw(36, 1, 0, 0);

            //Scene
            gpuCommands->bindPipeline(cubePipeline);
            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            //Only do this once.
            gpuCommands->bindlessDescriptorSet(1);

            mat4s globalModel{};
            //Update rotating cube data.
            if (cbData)
            {
                globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

                //TODO: Match these name with what's in the shader.
                UniformData uniformData{};
                uniformData.viewPerspective = gameCamera.internal3DCamera.viewProjection;
                uniformData.globalModel = globalModel;
                //eye not used in shader.

                uniformData.eye = vec4s{ eye.x, eye.y, eye.z, 1.f };
                uniformData.light = vec4s{ gameCamera.internal3DCamera.position.x, gameCamera.internal3DCamera.position.y, gameCamera.internal3DCamera.position.z, 1.f };

                memcpy(cbData, &uniformData, sizeof(UniformData));
            }

            newPosition.z += 10 * deltaTime;

            Buffer* positionBuf = gpu.accessBuffer(positionalBuffer);
            pushConstants.modelPositionAddress = positionBuf->bufferAddress;
            for (uint32_t entityIndex = 0; entityIndex < entities.size; ++entityIndex)
            {
                const Entity& entity = entities[entityIndex];
                mat4s newPositionMat = drawMatrices[entity.positionIndex];

                newPositionMat = glms_translate(newPositionMat, newPosition);
                memcpy(((mat4s*)positionBuf->mappedMemory + entity.positionIndex), &newPositionMat, sizeof(mat4s));

                pushConstants.index = entity.positionIndex;
                for (uint32_t meshIndex = 0; meshIndex < models[entity.modelIndex].meshDraws.size; ++meshIndex)
                {
                    MeshDraw meshDraw = models[entity.modelIndex].meshDraws[meshIndex];

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

    gpu.unmapBuffer(cbMap);
    gpu.unmapBuffer(skyboxMaterialMap);
    gpu.unmapBuffer(skyboxCBMap);

    gpu.destroyBuffer(positionalBuffer);

    gpu.destroyDescriptorSet(skyboxDescriptorSet);
    gpu.destroyBuffer(skyboxMaterialBuffer);
    gpu.destroyBuffer(skyboxUniformBuffer);

    for (uint32_t i = 0; i < models.size; ++i)
    {
        models[i].shutdownModel(gpu, renderer);
    }
    models.shutdown();
    drawMatrices.shutdown();
    entities.shutdown();

    gpu.destroySampler(skyboxSampler);

    gpu.destroyBuffer(sceneBuffer);
    gpu.destroyDescriptorSetLayout(mainDescriptorSetLayout);
    gpu.destroyDescriptorSetLayout(skyboxDescriptorSetLayout);
    gpu.destroyPipeline(cubePipeline);
    gpu.destroyPipeline(skyboxPipeline);

    imgui->shutdown();

    gpuProfiler.shutdown();
    resourceManager.shutdown();

    renderer.shutdown();

    inputHandler.shutdown();
    Window::instance()->shutdown();

    MemoryService::instance()->shutdown();

    return 0;
}