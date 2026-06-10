#ifndef SCENE_HDR
#define SCENE_HDR

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include "Graphics/LoadGLTF.hpp"

#include "Application/Audio.hpp"

#include "Utils.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

struct HeapAllocator;

struct Scene
{
    void initScene(HeapAllocator* inAllocator, GPUDevice& gpu, DescriptorSetLayoutHandle descriptorSetLayout);
    void buildScene();
    void buildDebugScene();
    void buildRigidBodyEntity(EntityModels modelType, DebugModels debugModelType, EntityType entityType, const vec3s& position, vec3s axis,
                              float angle, const JPH::BodyCreationSettings& shapeSetting, const vec4s& colour);
    void buildNoneSoildEntity(EntityModels modelType, EntityType entityType, vec3s& position, vec3s axis, float angle);
    void shutdownScene(GPUDevice& gpu);

    JPH::BodyCreationSettings sphereSettings;
    JPH::BodyCreationSettings sphereSettings2;

    uint32_t totalEntities = 4444;
    uint32_t currentLastEntity = 0;

    Array<Entity> entities;
    Array<EntityData> entityData;
    Array<DebugEntityData> debugEntityData;
    Array<Model> models;
    Array<Model> debugModels;
    Array<JPH::BodyID> bodiesToBeAdded;

    HeapAllocator* allocator;
};
#endif // !SCENE_HDR
