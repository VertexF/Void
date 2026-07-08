#include "Scene.hpp"

#include "Foundation/Memory.hpp"
#include "Foundation/File.hpp"
#include "Physics/Physics.hpp"

#include "Player.hpp"

void Scene::initScene(HeapAllocator *inAllocator, GPUDevice & gpu, DescriptorSetLayoutHandle descriptorSetLayout)
{
    allocator = inAllocator;

    entities.init(allocator, totalEntities, totalEntities);

    entityData.init(allocator, totalEntities, totalEntities);
    bodiesToBeAdded.init(allocator, totalEntities);
    models.init(allocator, 2, 2);
    debugModels.init(allocator, 1, 1);

    models[EntityModels::ROCK_MODEL].loadModel("Assets/Models/out/rock.glb", gpu, descriptorSetLayout);
    models[EntityModels::DUCK_MODEL].loadModel("Assets/Models/out/Duck.glb", gpu, descriptorSetLayout);

    debugModels[DebugModelType::SPHERE].loadCollider("Assets/Models/Debug/debugSphere.glb", gpu);
}

void Scene::buildScene()
{
    JPH::SphereShapeSettings playerSphereSettings{ 1.0f };
    playerSphereSettings.SetEmbedded();

    JPH::ShapeSettings::ShapeResult playerShapeResult = playerSphereSettings.Create();
    JPH::ShapeRefC playerShapeRef = playerShapeResult.Get();

    vec3s playerInitPostion{ 0.f };
    sphereSettings2.SetShape(playerShapeRef);
    sphereSettings2.mPosition = JPH::Vec3Arg{ playerInitPostion.x, playerInitPostion.y, playerInitPostion.z };
    sphereSettings2.mRotation = JPH::Quat::sIdentity();
    sphereSettings2.mMotionType = JPH::EMotionType::Dynamic;
    sphereSettings2.mObjectLayer = Layers::MOVING;
    //This is the make player function. Eventually we need to clean all this up so we wont need to call this function to create a player.
    buildRigidBodyEntity(EntityModels::DUCK_MODEL, DebugModelType::SPHERE, EntityType::PLAYER, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, 0.f, sphereSettings2, { 1.f, 1.f, 1.f, 1.f });

    JPH::SphereShapeSettings duckSphereSettings{ 1.5f };
    duckSphereSettings.SetEmbedded();

    JPH::ShapeSettings::ShapeResult duckShapeResult = duckSphereSettings.Create();
    JPH::ShapeRefC duckShapeRef = duckShapeResult.Get();

    vec3s position1{ 20.f, 59.f, 0.f };
    sphereSettings2.SetShape(duckShapeRef);
    sphereSettings2.mPosition = JPH::Vec3Arg{ position1.x, position1.y, position1.z };
    sphereSettings2.mRotation = JPH::Quat::sIdentity();
    sphereSettings2.mMotionType = JPH::EMotionType::Dynamic;
    sphereSettings2.mObjectLayer = Layers::MOVING;
    buildRigidBodyEntity(EntityModels::DUCK_MODEL, DebugModelType::SPHERE, EntityType::DUCK, position1, { 0.f, 0.f, 0.f }, 0.f, sphereSettings2, { 1.f, 1.f, 0.f, 1.f });

    vec3s position3{ 0.f, -10.f, 120.f };
    sphereSettings2.SetShape(duckShapeRef);
    sphereSettings2.mPosition = JPH::Vec3Arg{ position3.x, position3.y, position3.z };
    sphereSettings2.mRotation = JPH::Quat::sIdentity();
    sphereSettings2.mMotionType = JPH::EMotionType::Dynamic;
    sphereSettings2.mObjectLayer = Layers::MOVING;
    buildRigidBodyEntity(EntityModels::DUCK_MODEL, DebugModelType::SPHERE, EntityType::DUCK, position3, { 0.f, 0.f, 0.f }, 0.f, sphereSettings2, { 1.f, 1.f, 0.f, 1.f });

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
    Physics::instance().bodyInterface->SetLinearVelocity(entities[1].bodyID, JPH::Vec3(0.f, -16.f, 0.f));
    Physics::instance().bodyInterface->SetLinearVelocity(entities[2].bodyID, JPH::Vec3(0.f, 0.f, -16.f));

    srand(time(0));

    float sceneRadius = 2000.f;
    for (uint32_t i = 3; i < totalEntities; ++i)
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
        buildRigidBodyEntity(EntityModels::ROCK_MODEL, DebugModelType::SPHERE, EntityType::ROCK, position, axis, angle, rockShapeSettings, { 1.f, 0.f, 0.f, 1.f });
    }

    JPH::BodyInterface::AddState addingState = Physics::instance().bodyInterface->AddBodiesPrepare(bodiesToBeAdded.data, bodiesToBeAdded.size);
    Physics::instance().bodyInterface->AddBodiesFinalize(bodiesToBeAdded.data, bodiesToBeAdded.size, addingState, JPH::EActivation::Activate);
}

void Scene::buildDebugScene() 
{
    srand(time(0));

    float blue = 0.f;
    float radius = 13.5;

    vec3s position{};
    //Ducks
    for (uint32_t duckIndex = 0; duckIndex < 20; ++duckIndex)
    {
        float rotx = ((float(rand()) / RAND_MAX) * 2 - 1);
        float roty = ((float(rand()) / RAND_MAX) * 2 - 1);
        float rotz = ((float(rand()) / RAND_MAX) * 2 - 1);

        JPH::SphereShapeSettings duckSphereSetting2{ radius + (blue * 5.f) };
        duckSphereSetting2.SetEmbedded();
        JPH::ShapeSettings::ShapeResult duckShapeResult2 = duckSphereSetting2.Create();
        JPH::ShapeRefC duckShapeRef2 = duckShapeResult2.Get();

        JPH::BodyCreationSettings duckShapeSettings;
        duckShapeSettings.SetShape(duckShapeRef2);
        duckShapeSettings.mRotation = JPH::Quat(rotx, roty, rotz, rotx).Normalized();

        position.x = duckIndex * 60.f;

        duckShapeSettings.mPosition = JPH::Vec3Arg{ position.x, position.y, position.z };
        duckShapeSettings.mObjectLayer = Layers::MOVING;
        duckShapeSettings.mMotionType = JPH::EMotionType::Dynamic;

        buildRigidBodyEntity(EntityModels::DUCK_MODEL, DebugModelType::SPHERE, EntityType::DUCK, position, { 0.f, 0.f, 0.f }, 0.f, duckShapeSettings, { 1.f - blue, 0.f, blue, 1.f });
        blue += 0.025f;
    }

    //Rocks
    for (uint32_t rockIndex = 0; rockIndex < 20; ++rockIndex)
    {
        float rotx = ((float(rand()) / RAND_MAX) * 2 - 1);
        float roty = ((float(rand()) / RAND_MAX) * 2 - 1);
        float rotz = ((float(rand()) / RAND_MAX) * 2 - 1);

        JPH::SphereShapeSettings rockSphereSetting2{ radius + (blue * 5.f) };
        rockSphereSetting2.SetEmbedded();
        JPH::ShapeSettings::ShapeResult rockShapeResult2 = rockSphereSetting2.Create();
        JPH::ShapeRefC rockShapeRef2 = rockShapeResult2.Get();

        JPH::BodyCreationSettings rockShapeSettings;
        rockShapeSettings.SetShape(rockShapeRef2);
        rockShapeSettings.mRotation = JPH::Quat(rotx, roty, rotz, rotx).Normalized();

        position.x = (rockIndex * 60.f) + 30.f;

        rockShapeSettings.mPosition = JPH::Vec3Arg{ position.x, position.y, position.z };
        rockShapeSettings.mObjectLayer = Layers::NON_MOVING;
        rockShapeSettings.mMotionType = JPH::EMotionType::Static;

        buildRigidBodyEntity(EntityModels::ROCK_MODEL, DebugModelType::SPHERE, EntityType::ROCK, position, { 0.f, 0.f, 0.f }, 0.f, rockShapeSettings, { 1.f - blue, 0.f, blue, 1.f });
        blue += 0.025f;
    }

    JPH::BodyInterface::AddState addingState = Physics::instance().bodyInterface->AddBodiesPrepare(bodiesToBeAdded.data, bodiesToBeAdded.size);
    Physics::instance().bodyInterface->AddBodiesFinalize(bodiesToBeAdded.data, bodiesToBeAdded.size, addingState, JPH::EActivation::Activate);
}

void Scene::buildPerformance()
{
    //totalEntities = 8888;
    srand(time(0));

    //entities.setSize(totalEntities);
    //entityData.setSize(totalEntities);
    //bodiesToBeAdded.setSize(totalEntities);

    float radius = 13.5;

    float sceneRadius = 500.f;
    vec3s position{};
    //Ducks
    for (uint32_t duckIndex = 0; duckIndex < totalEntities / 2; ++duckIndex)
    {
        vec3s position;
        position.x = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        position.y = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        position.z = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;

        float rotx = ((float(rand()) / RAND_MAX) * 2 - 1);
        float roty = ((float(rand()) / RAND_MAX) * 2 - 1);
        float rotz = ((float(rand()) / RAND_MAX) * 2 - 1);

        JPH::SphereShapeSettings duckSphereSetting2{ radius };
        duckSphereSetting2.SetEmbedded();
        JPH::ShapeSettings::ShapeResult duckShapeResult2 = duckSphereSetting2.Create();
        JPH::ShapeRefC duckShapeRef2 = duckShapeResult2.Get();

        JPH::BodyCreationSettings duckShapeSettings;
        duckShapeSettings.SetShape(duckShapeRef2);
        duckShapeSettings.mRotation = JPH::Quat(rotx, roty, rotz, rotx).Normalized();

        duckShapeSettings.mPosition = JPH::Vec3Arg{ position.x, position.y, position.z };
        duckShapeSettings.mObjectLayer = Layers::MOVING;
        duckShapeSettings.mMotionType = JPH::EMotionType::Dynamic;
        duckShapeSettings.mGravityFactor = 0.f;

        buildRigidBodyEntity(EntityModels::DUCK_MODEL, DebugModelType::SPHERE, EntityType::DUCK, position, { 0.f, 0.f, 0.f }, 0.f, duckShapeSettings, { ((float(rand()) / RAND_MAX) * 2), ((float(rand()) / RAND_MAX) * 2), 1.f, 1.f});
    }

    //Rocks
    for (uint32_t rockIndex = 0; rockIndex < totalEntities / 2; ++rockIndex)
    {
        vec3s position;
        position.x = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        position.y = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
        position.z = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;

        float rotx = ((float(rand()) / RAND_MAX) * 2 - 1);
        float roty = ((float(rand()) / RAND_MAX) * 2 - 1);
        float rotz = ((float(rand()) / RAND_MAX) * 2 - 1);

        JPH::SphereShapeSettings rockSphereSetting2{ radius };
        rockSphereSetting2.SetEmbedded();
        JPH::ShapeSettings::ShapeResult rockShapeResult2 = rockSphereSetting2.Create();
        JPH::ShapeRefC rockShapeRef2 = rockShapeResult2.Get();

        JPH::BodyCreationSettings rockShapeSettings;
        rockShapeSettings.SetShape(rockShapeRef2);
        rockShapeSettings.mRotation = JPH::Quat(rotx, roty, rotz, rotx).Normalized();

        rockShapeSettings.mPosition = JPH::Vec3Arg{ position.x, position.y, position.z };
        rockShapeSettings.mObjectLayer = Layers::NON_MOVING;
        rockShapeSettings.mMotionType = JPH::EMotionType::Static;
        rockShapeSettings.mGravityFactor = 0.f;

        buildRigidBodyEntity(EntityModels::ROCK_MODEL, DebugModelType::SPHERE, EntityType::ROCK, position, { 0.f, 0.f, 0.f }, 0.f, rockShapeSettings, { 1.f, 1.f, 0.f, 1.f });
    }

    for (uint32_t i = 0; i < totalEntities / 2; ++i) 
    {
        float speedX = ((float(rand()) / RAND_MAX) * 200 - 100);
        float speedY = ((float(rand()) / RAND_MAX) * 200 - 100);
        float speedZ = ((float(rand()) / RAND_MAX) * 200 - 100);

        Physics::instance().bodyInterface->SetLinearVelocity(entities[i].bodyID, JPH::Vec3(speedX, speedY, speedZ));
    }

    JPH::BodyInterface::AddState addingState = Physics::instance().bodyInterface->AddBodiesPrepare(bodiesToBeAdded.data, bodiesToBeAdded.size);
    Physics::instance().bodyInterface->AddBodiesFinalize(bodiesToBeAdded.data, bodiesToBeAdded.size, addingState, JPH::EActivation::Activate);
}

void Scene::buildRigidBodyEntity(EntityModels modelType, DebugModelType debugModelType, EntityType entityType, const vec3s& position, vec3s axis,
                                 float angle, const JPH::BodyCreationSettings& shapeSetting, const vec4s& colour)
{   
    //Note that this uses the shorthand version of creating and adding a body to the world
    JPH::BodyID bodyID = Physics::instance().bodyInterface->CreateBody(shapeSetting)->GetID();
    bodiesToBeAdded.push(bodyID);
    entities[currentLastEntity].bodyID = bodyID;

    //TODO: Switch over the shapes that it might be.
    JPH::RMat44 shapePosition = Physics::instance().bodyInterface->GetWorldTransform(bodyID);
    JPH::RMat44 shapeModel = JPH::RMat44::sIdentity();

    switch (entityType)
    {
    case EntityType::PLAYER:
    {
        shapeModel = getCollsionShape(JPH::EShapeSubType::Sphere, shapeSetting);
        entities[currentLastEntity].isDynamic = true;

        entities[currentLastEntity].entityType = EntityType::PLAYER;
        entities[currentLastEntity].entityData = void_allocat(Player, &MemoryService::instance()->systemAllocator);
        new (entities[currentLastEntity].entityData) Player;

        static_cast<Player*>(entities[currentLastEntity].entityData)->init(entityData[currentLastEntity], shapeSetting.GetShape(), shapeModel);

        //We need to set up the entity we have just created to the physics so we can access the actual data that the collision detection corrisponds to.
        Physics::instance().bodyInterface->SetUserData(bodyID, (uint64_t)&entities[currentLastEntity]);
    }
        break;
    case EntityType::ROCK:
        {
            shapeModel = getCollsionShape(JPH::EShapeSubType::Sphere, shapeSetting);

            entities[currentLastEntity].isDynamic = false;
            entities[currentLastEntity].entityType = EntityType::ROCK;

            Physics::instance().bodyInterface->SetUserData(bodyID, (uint64_t)&entities[currentLastEntity]);
        }
        break;
    case EntityType::DUCK:
    {
        shapeModel = getCollsionShape(JPH::EShapeSubType::Sphere, shapeSetting);

        entities[currentLastEntity].isDynamic = true;
        entities[currentLastEntity].entityType = EntityType::DUCK;

        Physics::instance().bodyInterface->SetUserData(bodyID, (uint64_t)&entities[currentLastEntity]);
    }
    break;
    default:
        Physics::instance().bodyInterface->SetUserData(bodyID, NULL);
        break;
    }

    entities[currentLastEntity].isDeleted = false;
    entityData[currentLastEntity].position = convertToMat4(shapePosition);
    entityData[currentLastEntity].colour = colour;
    entityData[currentLastEntity].debugModel = convertToMat4(shapeModel);

    entities[currentLastEntity].entityIndex = currentLastEntity;
    entities[currentLastEntity].modelType = modelType;
    models[modelType].instanceCount++;
    debugModels[debugModelType].instanceCount++;

    currentLastEntity++;
}

void Scene::buildNoneSoildEntity(EntityModels modelType, EntityType entityType, vec3s& position, vec3s axis, float angle)
{
    vec3s scaledVector = glms_vec3_scale(axis, sinf(angle * 0.5f));

    entityData[currentLastEntity].position = glms_mat4_mul(glms_rotate_make(cosf(angle * 0.5f), scaledVector), glms_translate_make(position));
    entityData[currentLastEntity].colour = { 1.f, 0.f, 1.f, 1.f };
    entityData[currentLastEntity].debugModel = glms_mat4_identity();

    entities[currentLastEntity].entityIndex = currentLastEntity;
    entities[currentLastEntity].entityType = entityType;
    entities[currentLastEntity].modelType = modelType;
    models[modelType].instanceCount++;

    currentLastEntity++;
}

void Scene::shutdownScene(GPUDevice& gpu)
{
    //for (uint32_t i = 0; i < totalColliders; ++i)
    //{
    //    Physics::instance().bodyInterface->RemoveBody(entities[i].bodyID);
    //    Physics::instance().bodyInterface->DestroyBody(entities[i].bodyID);
    //}

    //Tempory
    if (entities[0].entityData) 
    {
        void_free(static_cast<Player*>(entities[0].entityData), &MemoryService::instance()->systemAllocator);
    }

    //for (uint32_t i = 0; i < entities.size; ++i) 
    //{
    //    if (entities[i].entityData)
    //    {
    //        void_free(static_cast<Player*>(entities[i].entityData), &MemoryService::instance()->systemAllocator);
    //    }
    //}

    for (uint32_t i = 0; i < models.size; ++i)
    {
        models[i].shutdownModel(gpu);
    }
    models.shutdown();

    for (uint32_t i = 0; i < debugModels.size; ++i)
    {
        debugModels[i].shutdownModel(gpu);
    }
    debugModels.shutdown();

    entities.shutdown();
    entityData.shutdown();
    bodiesToBeAdded.shutdown();
}

JPH::RMat44 Scene::getCollsionShape(JPH::EShapeSubType shapeType, const JPH::BodyCreationSettings& shapeSetting)
{
    switch (shapeType)
    {
    case JPH::EShapeSubType::Sphere:
    {
        return JPH::RMat44::sScale(((JPH::SphereShape*)shapeSetting.GetShape())->GetRadius());
    }
    break;
    default:
        VOID_ERROR("Shape type not supported.\n");
    }
}