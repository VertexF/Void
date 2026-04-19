#ifndef SCENE_HDR
#define SCENE_HDR

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include "Graphics/LoadGLTF.hpp"

#include "Physics/Physics.hpp"

struct HeapAllocator;

namespace
{
    //Here we are going to attempt full bindless for the debug renderer to make this as painless as possible in the future.
    struct DebugRendererData
    {
        mat4s position;
        //We also need the global scale to be the same as the regular geometry other wise things will be too small.
        mat4s globalModel;
        //We need this because the final matrix that comes out the glb after multiplying 
        //all local nodes together needs to be the same as the collision geometry.
        //Meaning that model matrix we get out of the actual geometry needs to be given to the debug geometry if they tied together when creating the buffer.
        mat4s model;
        mat4s viewPerspective;
        //Colour will be used as a key for various different objects.
        vec4s colour;
        float pad[4];
    };

    struct EntityData
    {
        mat4s pos;
        vec4s colour;
        float padd[4];
    };

    struct Entity
    {
        Entity() : positionIndex(UINT32_MAX), modelIndex(UINT32_MAX), debugRendererIndex(UINT32_MAX), shape(nullptr)
        {
        }

        //If we do this we can have a gaint bindless positionally buffer that has everything in it we just index into the that position array.
        uint32_t positionIndex;
        //We can loop through all the entities and use that model index to fetch the meshDraw to be able to draw all the models regardless of the model.
        uint32_t modelIndex;

        uint32_t debugRendererIndex;

        JPH::BodyID bodyID;
        //This only exists if debugRendererIndex != UINT32_MAX. 
        JPH::Shape* shape;
    };

    inline mat4s& convertToMat4(JPH::RMat44& jphMat) { return *reinterpret_cast<mat4s*>(&jphMat); }
    inline vec3s& convertToVec3(JPH::Vec3& jphVec3) { return *reinterpret_cast<vec3s*>(&jphVec3); }
    inline JPH::Vec3& convertToJPHVec3(vec3s& jphVec3) { return *reinterpret_cast<JPH::Vec3*>(&jphVec3); }
}

struct Scene
{
    void initScene(HeapAllocator* inAllocator, GPUDevice& gpu, BufferHandle sceneBuffer, DescriptorSetLayoutHandle descriptorSetLayout);
    void buildScene(Physics& physics);
    void buildRigidBodyEntity(const Physics& physics, uint32_t modelIndex, const vec3s& position, vec3s axis,
                              float angle, const JPH::BodyCreationSettings& shapeSetting);
    void buildNoneSoildEntity(uint32_t modelIndex, vec3s& position, vec3s axis, float angle);
    void shutdownScene(GPUDevice& gpu, Physics& physics);

    JPH::BodyCreationSettings sphereSettings;
    JPH::SphereShape sphereShape{ 850.f };
    JPH::BodyCreationSettings sphereSettings1;
    JPH::SphereShape sphereShape1{ 50.f };

    uint32_t totalEntities = 1111;
    uint32_t totalColliders = 2;
    uint32_t currentLastEntity;
    uint32_t currentDebugRendererIndex;

    static constexpr uint32_t rockModelIndex = 0;
    static constexpr uint32_t duckModelIndex = 1;
    static constexpr uint32_t debugSphereIndex = 2;

    Array<Entity> entities;
    Array<EntityData> entityData;
    Array<DebugRendererData> debugRendererData;
    Array<Model> models;

    HeapAllocator* allocator;
};
#endif // !SCENE_HDR
