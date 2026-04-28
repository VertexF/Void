#include "Scene.hpp"

#include "Foundation/Memory.hpp"
#include "Foundation/File.hpp"

void Scene::initScene(HeapAllocator *inAllocator, GPUDevice & gpu, BufferHandle sceneBuffer, DescriptorSetLayoutHandle descriptorSetLayout)
{
    allocator = inAllocator;

    entities.init(allocator, totalEntities, totalEntities);
    entityData.init(allocator, totalEntities, totalEntities);
    debugRendererData.init(allocator, totalEntities, totalEntities);
    bodiesToBeAdded.init(allocator, totalEntities);
    models.init(allocator, 3, 3);

    models[rockModelIndex].loadModel("Assets/Models/out/rock.glb", gpu, sceneBuffer, descriptorSetLayout);
    models[duckModelIndex].loadModel("Assets/Models/out/Duck.glb", gpu, sceneBuffer, descriptorSetLayout);
    models[debugSphereIndex].loadCollider("Assets/Models/Debug/debugSphere.glb", gpu);

    currentLastEntity = 0;
    currentDebugRendererIndex = 0;
}

void Scene::buildScene(Physics& physics)
{
    JPH::SphereShapeSettings duckSphereSettings{1.5f};
    duckSphereSettings.SetEmbedded();

    JPH::ShapeSettings::ShapeResult duckShapeResult = duckSphereSettings.Create();
    JPH::ShapeRefC duckShapeRef = duckShapeResult.Get();

    //vec3s position{ 0.f, 0.f, 0.f };
    //sphereSettings.SetShape(rockShapeRef);
    //sphereSettings.mPosition = JPH::Vec3Arg{ position.x, position.y, position.z };
    //sphereSettings.mRotation = JPH::Quat::sIdentity();
    //sphereSettings.mMotionType = JPH::EMotionType::Static;
    //sphereSettings.mObjectLayer = Layers::MOVING;
    //buildRigidBodyEntity(physics, rockModelIndex, position, { 0.f, 0.f, 0.f }, 0.f, sphereSettings, { 1.f, 0.f, 1.f, 1.f });

    vec3s position1{ 20.f, 59.f, 0.f };
    sphereSettings2.SetShape(duckShapeRef);
    sphereSettings2.mPosition = JPH::Vec3Arg{ position1.x, position1.y, position1.z };
    sphereSettings2.mRotation = JPH::Quat::sIdentity();
    sphereSettings2.mMotionType = JPH::EMotionType::Dynamic;
    sphereSettings2.mObjectLayer = Layers::MOVING;
    buildRigidBodyEntity(physics, duckModelIndex, position1, { 0.f, 0.f, 0.f }, 0.f, sphereSettings2, { 1.f, 1.f, 0.f, 1.f });

    vec3s position3{ 0.f, -10.f, 120.f };
    sphereSettings2.SetShape(duckShapeRef);
    sphereSettings2.mPosition = JPH::Vec3Arg{ position3.x, position3.y, position3.z };
    sphereSettings2.mRotation = JPH::Quat::sIdentity();
    sphereSettings2.mMotionType = JPH::EMotionType::Dynamic;
    sphereSettings2.mObjectLayer = Layers::MOVING;
    buildRigidBodyEntity(physics, duckModelIndex, position3, { 0.f, 0.f, 0.f }, 0.f, sphereSettings2, { 1.f, 1.f, 0.f, 1.f });

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
    physics.bodyInterface->SetLinearVelocity(entities[0].bodyID, JPH::Vec3(0.f, -64.f, 0.f));
    physics.bodyInterface->SetLinearVelocity(entities[1].bodyID, JPH::Vec3(0.f, 0.f, -64.f));

    uint32_t maxBodies = physics.physicsSystem.GetMaxBodies();
    uint32_t numBodies = physics.physicsSystem.GetNumBodies();

    srand(time(0));

    float sceneRadius = 1000.f;
    for (uint32_t i = 2; i < totalEntities; ++i)
    {
        vec3s position;
        position.x = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        position.y = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        position.z = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;

        float rotx = ((float(rand()) / RAND_MAX) * 2 - 1);
        float roty = ((float(rand()) / RAND_MAX) * 2 - 1);
        float rotz = ((float(rand()) / RAND_MAX) * 2 - 1);

        vec3s axis = glms_normalize({ rotx, roty, rotz });
        float angle = (float(rand()) / RAND_MAX) * M_PI_4;

        float sinHalfAngle = std::sin((angle / 2) * M_PI / 180);
        float cosHalfAngle = std::cos((angle / 2) * M_PI / 180);

        JPH::SphereShapeSettings rockSphereSetting2{ 13.5f };
        rockSphereSetting2.SetEmbedded();

        JPH::ShapeSettings::ShapeResult rockShapeResult2 = rockSphereSetting2.Create();
        JPH::ShapeRefC rockShapeRef2 = rockShapeResult2.Get();

        JPH::BodyCreationSettings rockShapeSettings;
        rockShapeSettings.SetShape(rockShapeRef2);
        rockShapeSettings.mPosition = JPH::Vec3Arg{ position.x, position.y, position.z };
        rockShapeSettings.mRotation = JPH::Quat(rotx, roty, rotz, rotx).Normalized();
        rockShapeSettings.mMotionType = JPH::EMotionType::Static;
        rockShapeSettings.mObjectLayer = Layers::NON_MOVING;
        buildRigidBodyEntity(physics, rockModelIndex, position, axis, angle, rockShapeSettings, { 1.f, 0.f, 0.f, 1.f });
    }

    JPH::BodyInterface::AddState addingState = physics.bodyInterface->AddBodiesPrepare(bodiesToBeAdded.data, bodiesToBeAdded.size);
    physics.bodyInterface->AddBodiesFinalize(bodiesToBeAdded.data, bodiesToBeAdded.size, addingState, JPH::EActivation::Activate);
}

void Scene::buildRigidBodyEntity(const Physics& physics, uint32_t modelIndex, const vec3s& position, vec3s axis, float angle, const JPH::BodyCreationSettings& shapeSetting, const vec4s& colour)
{
    //Note that this uses the shorthand version of creating and adding a body to the world
    JPH::BodyID bodyID = physics.bodyInterface->CreateBody(shapeSetting)->GetID();
    bodiesToBeAdded.push(bodyID);
    entities[currentLastEntity].bodyID = bodyID;
    entities[currentLastEntity].isDynamic = shapeSetting.mMotionType == JPH::EMotionType::Dynamic;

    JPH::EShapeSubType shapeType = shapeSetting.GetShape()->GetSubType();
    JPH::RMat44 shapeModel;
    switch (shapeType) 
    {
    case JPH::EShapeSubType::Sphere:
    {
        shapeModel = JPH::RMat44::sScale(((JPH::SphereShape*)shapeSetting.GetShape())->GetRadius());
    }
        break;
    default:
        VOID_ERROR("Shape type not supported.\n");
    }

    //TODO: Switch over the shapes that it might be.
    JPH::RMat44 shapePosition = physics.bodyInterface->GetWorldTransform(entities[currentLastEntity].bodyID);

    debugRendererData[currentDebugRendererIndex].colour = colour;
    debugRendererData[currentDebugRendererIndex].position = convertToMat4(shapePosition);
    debugRendererData[currentDebugRendererIndex].model = convertToMat4(shapeModel);

    entities[currentLastEntity].debugRendererIndex = currentDebugRendererIndex;

    entityData[currentLastEntity].pos = convertToMat4(shapePosition);
    entityData[currentLastEntity].colour = colour;
    entities[currentLastEntity].positionIndex = currentLastEntity;
    entities[currentLastEntity].modelIndex = modelIndex;
    models[modelIndex].countOfModelType++;

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
    for (uint32_t i = 0; i < totalColliders; ++i)
    {
        physics.bodyInterface->RemoveBody(entities[i].bodyID);
        physics.bodyInterface->DestroyBody(entities[i].bodyID);
    }

    for (uint32_t i = 0; i < models.size; ++i)
    {
        models[i].shutdownModel(gpu);
    }
    models.shutdown();

    entities.shutdown();
    entityData.shutdown();
    debugRendererData.shutdown();
    bodiesToBeAdded.shutdown();
}