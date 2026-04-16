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

#include <stdlib.h>
#include <SDL3/SDL.h>
#include <vender/stb_image.h>

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
    PipelineHandle debugPipeline;
    BufferHandle sceneBuffer;
    BufferHandle skyboxUniformBuffer;
    BufferHandle skyboxMaterialBuffer;
    DescriptorSetLayoutHandle mainDescriptorSetLayout;
    DescriptorSetLayoutHandle skyboxDescriptorSetLayout;
    DescriptorSetHandle skyboxDescriptorSet;

    BufferHandle positionalBuffer;
    BufferHandle debugDataBuffer;

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

    //Here we are going to attempt full bindless for the debug renderer to make this as painless as possible in the future.
    struct DebugData
    {
        mat4s pos;
        //We also need the global scale to be the same as the regular geometry other wise things will be too small.
        mat4s globalModel;
        //We need this because the final matrix that comes out the glb after multiplying 
        //all local nodes together needs to be the same as the collision geometry.
        //Meaning that model matrix we get out of the actual geometry needs to be given to the debug geometry if they tied together when creating the buffer.
        mat4s model;
        mat4s viewPerspective;
        //Colour will be used as a key for various different objects.
        vec4s colour;
        float pad[12];
    };

    struct EntityData
    {
        mat4s pos;
        vec4s colour;
        float padd[12];
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

    inline mat4s& convertToMat4(JPH::RMat44& jphMat) { return *reinterpret_cast<mat4s*>(&jphMat); }
    inline vec3s& convertToVec3(JPH::Vec3& jphVec3) { return *reinterpret_cast<vec3s*>(&jphVec3); }
}

struct Entity
{
    //If we do this we can have a gaint bindless positionally buffer that has everything in it we just index into the that position array.
    uint32_t positionIndex;
    //We can loop through all the entities and use that model index to fetch the meshDraw to be able to draw all the models regardless of the model.
    uint32_t modelIndex;

    JPH::BodyID bodyID;
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

    GPUProfiler gpuProfiler;
    gpuProfiler.init(allocator, 100);

    ImguiService* imgui = ImguiService::instance();
    ImguiServiceConfiguration imguiConfig = { &gpu, Window::instance()->platformHandle };
    imgui->init(&imguiConfig);

    Physics physics;
    physics.initPhysics();

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

    //Debug renderer
    PipelineCreation debugPipelineCreation{};
    debugPipelineCreation.depthStencil.depthEnable = false;

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

    srand(42);

    uint32_t totalDucks = 1111;
    Array<EntityData> entityData;
    entityData.init(allocator, totalDucks, totalDucks);

    uint32_t totalColliders = 2;
    Array<DebugData> debugData;
    debugData.init(allocator, totalColliders, totalColliders);

    Array<Entity> entities;
    entities.init(allocator, totalDucks, totalDucks);
    constexpr uint32_t rockModelIndex = 0;
    constexpr uint32_t duckModelIndex = 1;
    constexpr uint32_t debugSphereIndex = 2;
    Array<Model> models;
    models.init(allocator, 3, 3);
    models[rockModelIndex].loadModel(DEFAULT_3D_MODEL, gpu, sceneBuffer, mainDescriptorSetLayout);
    models[duckModelIndex].loadModel("Assets/Models/out/Duck.glb", gpu, sceneBuffer, mainDescriptorSetLayout);
    models[debugSphereIndex].loadCollider("Assets/Models/Debug/debugSphere.glb", gpu);

    float sceneRadius = 5000.f;
    for (uint32_t i = 2; i < totalDucks; ++i)
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

        entityData[i].pos = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(postion));
        entityData[i].colour = { rotx, roty, rotz, 1.f };

        entities[i].positionIndex = i;
        entities[i].modelIndex = rockModelIndex;
    }

    vec3s postion{};

    postion.x = 0.f;
    postion.y = 0.f;
    postion.z = 0.f;

    float rotx = 0.f;
    float roty = 0.f;
    float rotz = 0.f;

    vec3s axis = glms_normalize({ rotx, roty, rotz });
    float angle = 0.f;

    vec3s scaledVector = glms_vec3_scale(axis, sinf(angle * 0.5f));

    entityData[0].pos = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(postion));
    entityData[0].colour = { 1.f, 0.f, 1.f, 1.f };
    entities[0].positionIndex = 0;
    entities[0].modelIndex = rockModelIndex;

    postion.x = 5999.f;
    postion.y = 0.f;
    postion.z = 0.f;

    rotx = 0.f;
    roty = 0.f;
    rotz = 0.f;

    axis = glms_normalize({ rotx, roty, rotz });
    angle = 0.f;

    scaledVector = glms_vec3_scale(axis, sinf(angle * 0.5f));

    entityData[1].pos = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(postion));
    entityData[1].colour = { 0.f, 1.f, 1.f, 1.f };
    entities[1].positionIndex = 1;
    entities[1].modelIndex = duckModelIndex;

    // Create the shape
    JPH::SphereShape sphereShape{ 850.f };
    JPH::RVec3Arg position{ 0.f, 0.f, 0.f };
    //Note that this uses the shorthand version of creating and adding a body to the world
    JPH::BodyCreationSettings sphereSettings{ &sphereShape, position, JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::MOVING };
    entities[0].bodyID = physics.bodyInterface->CreateAndAddBody(sphereSettings, JPH::EActivation::Activate);

    // Create the shape
    JPH::SphereShape sphereShape1{ 50.f };
    JPH::RVec3Arg position1{ 5999.f, 0.f, 0.f };
    // Note that this uses the shorthand version of creating and adding a body to the world
    JPH::BodyCreationSettings sphereSettings1{ &sphereShape1, position1, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING };
    sphereSettings1.mLinearVelocity = { 0.f, 0.f, 0.f };
    entities[1].bodyID = physics.bodyInterface->CreateAndAddBody(sphereSettings1, JPH::EActivation::Activate);

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
    physics.bodyInterface->SetLinearVelocity(entities[1].bodyID, JPH::Vec3(-400.f, 0.0f, 0.0f));

    debugData[0].colour = { 1.f, 0.f, 1.f, 1.f };
    debugData[0].pos = glms_mat4_identity();//glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make({ 0.f, 0.f, 0.f }));
    debugData[0].model = glms_mat4_identity();
    debugData[0].viewPerspective = glms_mat4_identity();
    debugData[0].globalModel = glms_mat4_identity();

    debugData[1].colour = { 1.f, 0.f, 1.f, 1.f };
    debugData[1].pos = glms_mat4_identity();//glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make({ 10.f, 0.f, 0.f }));
    debugData[1].model = glms_mat4_identity();
    debugData[1].viewPerspective = glms_mat4_identity();
    debugData[1].globalModel = glms_mat4_identity();

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
    // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
    physics.physicsSystem.OptimizeBroadPhase();

    PushConstants pushConstants{};

    BufferCreation bufferCreation{};
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(EntityData) * entityData.size)
        .setName("othername")
        .setData(entityData.data);
    positionalBuffer = gpu.createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(DebugData) * totalColliders)
        .setName("debugRenderer")
        .setData(debugData.data);
    debugDataBuffer = gpu.createBindlessBuffer(bufferCreation);

    uint32_t positionalMatrixSize = entityData.size;

    entityData.shutdown();

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
    SkyboxData* skyboxMaterialBufferData = reinterpret_cast<SkyboxData*>(gpu.mapBuffer(skyboxMaterialMap));

    MapBufferParameters positionMap = { .buffer = positionalBuffer, .offset = 0, .size = 0 };
    EntityData* positionBufferData = reinterpret_cast<EntityData*>(gpu.mapBuffer(positionMap));

    MapBufferParameters debugDataMap = { .buffer = debugDataBuffer, .offset = 0, .size = 0 };
    DebugData* debugDataGPU = reinterpret_cast<DebugData*>(gpu.mapBuffer(debugDataMap));

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

            physics.updatePhysics();

            inputHandler.newFrame();
            inputHandler.update();
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
            //mat4s globalModel;
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

            //newPosition.z += deltaTime;
            Buffer* positionBuf = gpu.accessBuffer(positionalBuffer);

            for (uint32_t i = 0; i < positionalMatrixSize; ++i)
            {
                if (i == 1)
                {
                    JPH::RMat44 newPos = physics.bodyInterface->GetWorldTransform(entities[i].bodyID);
                    positionBufferData[i].pos = convertToMat4(newPos);
                }
            }

            pushConstants.modelPositionAddress = positionBuf->bufferAddress;
            for (uint32_t entityIndex = 0; entityIndex < entities.size; ++entityIndex)
            {
                const Entity& entity = entities[entityIndex];
                pushConstants.index = entityIndex;
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

            //Debug
            gpuCommands->bindPipeline(debugPipeline);
            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            Buffer* debugDataBufferf = gpu.accessBuffer(debugDataBuffer);
            for (uint32_t i = 0; i < positionalMatrixSize; ++i)
            {
                if (i == 1 || i == 0)
                {
                    globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

                    JPH::RMat44 newPos = physics.bodyInterface->GetWorldTransform(entities[i].bodyID);
                    debugDataGPU[i].pos = convertToMat4(newPos);
                    debugDataGPU[i].globalModel = globalModel;
                    debugDataGPU[i].viewPerspective = gameCamera.internal3DCamera.viewProjection;
                }
            }

            pushConstants.modelPositionAddress = debugDataBufferf->bufferAddress;
            for (uint32_t colliderIndex = 0; colliderIndex < totalColliders; ++colliderIndex)
            {
                const Entity& entity = entities[colliderIndex];
                pushConstants.index = entity.positionIndex;
                for (uint32_t meshIndex = 0; meshIndex < models[entity.modelIndex].meshDraws.size; ++meshIndex)
                {
                    MeshDraw meshDraw = models[debugSphereIndex].meshDraws[meshIndex];

                    Buffer* vertexDataBuf = gpu.accessBuffer(meshDraw.vertexBuffer);
                    pushConstants.vertexDataAddress = vertexDataBuf->bufferAddress;

                    vkCmdPushConstants(gpuCommands->vkCommandBuffer, gpuCommands->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

                    gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.componentType);

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
    gpu.unmapBuffer(positionMap);

    gpu.destroyBuffer(positionalBuffer);

    gpu.destroyDescriptorSet(skyboxDescriptorSet);
    gpu.destroyBuffer(skyboxMaterialBuffer);
    gpu.destroyBuffer(skyboxUniformBuffer);

    for (uint32_t i = 0; i < models.size; ++i)
    {
        models[i].shutdownModel(gpu);
    }
    models.shutdown();

    for (uint32_t i = 0; i < entities.size; ++i)
    {
        physics.bodyInterface->RemoveBody(entities[i].bodyID);
        physics.bodyInterface->DestroyBody(entities[i].bodyID);
    }

    entities.shutdown();

    gpu.destroySampler(skyboxSampler);
    gpu.destroyTexture(skyboxTextureHandle);

    gpu.destroyBuffer(sceneBuffer);
    gpu.destroyDescriptorSetLayout(mainDescriptorSetLayout);
    gpu.destroyDescriptorSetLayout(skyboxDescriptorSetLayout);
    gpu.destroyPipeline(cubePipeline);
    gpu.destroyPipeline(skyboxPipeline);

    imgui->shutdown();

    gpuProfiler.shutdown();

    gpu.shutdown();

    inputHandler.shutdown();
    Window::instance()->shutdown();

    MemoryService::instance()->shutdown();

    return 0;
}