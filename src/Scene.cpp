#include "Entity.hpp"

#include "Foundation/Memory.hpp"

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

void Scene::initScene(HeapAllocator *inAllocator, GPUDevice & gpu, BufferHandle sceneBuffer, DescriptorSetLayoutHandle descriptorSetLayout)
{
    allocator = inAllocator;

    entities.init(allocator, totalEntities, totalEntities);
    entityData.init(allocator, totalEntities, totalEntities);
    debugRendererData.init(allocator, totalColliders, totalColliders);
    models.init(allocator, 3, 3);

    models[rockModelIndex].loadModel(DEFAULT_3D_MODEL, gpu, sceneBuffer, descriptorSetLayout);
    models[duckModelIndex].loadModel("Assets/Models/out/Duck.glb", gpu, sceneBuffer, descriptorSetLayout);
    models[debugSphereIndex].loadCollider("Assets/Models/Debug/debugSphere.glb", gpu);

    currentLastEntity = 0;
    currentDebugRendererIndex = 0;
}

void Scene::buildScene(Physics& physics)
{
    vec3s position{ 0.f, 0.f, 0.f };
    sphereSettings.SetShape(&sphereShape);
    sphereSettings.mPosition = convertToJPHVec3(position);
    sphereSettings.mRotation = JPH::Quat::sIdentity();
    sphereSettings.mMotionType = JPH::EMotionType::Static;
    sphereSettings.mObjectLayer = Layers::MOVING;
    buildRigidBodyEntity(physics, rockModelIndex, position, { 0.f, 0.f, 0.f }, 0.f, sphereSettings);

    vec3s position1{ 5999.f, 0.f, 0.f };
    sphereSettings1.SetShape(&sphereShape1);
    sphereSettings1.mPosition = convertToJPHVec3(position1);
    sphereSettings1.mRotation = JPH::Quat::sIdentity();
    sphereSettings1.mMotionType = JPH::EMotionType::Dynamic;
    sphereSettings1.mObjectLayer = Layers::MOVING;
    buildRigidBodyEntity(physics, duckModelIndex, position1, { 0.f, 0.f, 0.f }, 0.f, sphereSettings1);

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
    physics.bodyInterface->SetLinearVelocity(entities[1].bodyID, JPH::Vec3(-400.f, 0.0f, 0.0f));

    srand(42);

    float sceneRadius = 5000.f;
    for (uint32_t i = 2; i < totalEntities; ++i)
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

        buildNoneSoildEntity(rockModelIndex, postion, axis, angle);
    }
}

void Scene::buildRigidBodyEntity(const Physics& physics, uint32_t modelIndex, const vec3s& position, vec3s axis, float angle, const JPH::BodyCreationSettings& shapeSetting)
{
    //Note that this uses the shorthand version of creating and adding a body to the world
    entities[currentLastEntity].bodyID = physics.bodyInterface->CreateAndAddBody(shapeSetting, JPH::EActivation::Activate);

    //TODO: Switch over the shapes that it might be.
    JPH::RMat44 sphereModel = JPH::RMat44::sTranslation(physics.bodyInterface->GetPosition(entities[currentLastEntity].bodyID));
    JPH::RMat44 spherePosition = physics.bodyInterface->GetWorldTransform(entities[currentLastEntity].bodyID);

    debugRendererData[currentDebugRendererIndex].colour = { 1.f, 0.f, 1.f, 1.f };
    debugRendererData[currentDebugRendererIndex].position = convertToMat4(spherePosition);
    debugRendererData[currentDebugRendererIndex].model = glms_mat4_identity();//convertToMat4(sphereModel);
    debugRendererData[currentDebugRendererIndex].viewPerspective = glms_mat4_identity();
    debugRendererData[currentDebugRendererIndex].globalModel = glms_mat4_identity();

    entities[currentLastEntity].debugRendererIndex = currentDebugRendererIndex;

    vec3s scaledVector = glms_vec3_scale(axis, sinf(angle * 0.5f));

    entityData[currentLastEntity].pos = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(position));
    entityData[currentLastEntity].colour = { 1.f, 0.f, 1.f, 1.f };
    entities[currentLastEntity].positionIndex = currentLastEntity;
    entities[currentLastEntity].modelIndex = modelIndex;

    currentLastEntity++;
    currentDebugRendererIndex++;
}

void Scene::buildNoneSoildEntity(uint32_t modelIndex, vec3s& position, vec3s axis, float angle)
{
    vec3s scaledVector = glms_vec3_scale(axis, sinf(angle * 0.5f));

    entityData[currentLastEntity].pos = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(position));
    entityData[currentLastEntity].colour = { 1.f, 0.f, 1.f, 1.f };
    entities[currentLastEntity].positionIndex = currentLastEntity;
    entities[currentLastEntity].modelIndex = modelIndex;
    entities[currentLastEntity].debugRendererIndex = UINT32_MAX;

    currentLastEntity++;
}

void Scene::shutdownScene(GPUDevice& gpu, Physics& physics)
{
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
}