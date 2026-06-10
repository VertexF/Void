#ifndef UTILS_HDR
#define UTILS_HDR

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

enum EntityType : uint8_t
{
    PLAYER,
    ROCK,
    DUCK,

    COUNT_TYPE
};

enum EntityModels : uint8_t
{
    ROCK_MODEL,
    DUCK_MODEL,

    MODEL_COUNT
};

enum DebugModels : uint8_t
{
    SPHERE,

    DEBUG_COUNT
};

//Here we are going to attempt full bindless for the debug renderer to make this as painless as possible in the future.
struct EntityData
{
    mat4s position;
    //We need this because the final matrix that comes out the glb after multiplying 
    //all local nodes together needs to be the same as the collision geometry.
    //Meaning that model matrix we get out of the actual geometry needs to be given to the debug geometry if they tied together when creating the buffer.
    mat4s debugModel;
    //Colour will be used as a key for various different objects.
    vec4s colour;
    float padd[4];
};

struct Entity
{
    //If we do this we can have a gaint bindless positionally buffer that has everything in it we just index into the that position array.
    uint32_t entityIndex = UINT32_MAX;
    //We can loop through all the entities and use that model index to fetch the meshDraw to be able to draw all the models regardless of the model.
    EntityModels modelType = MODEL_COUNT;
    EntityType entityType = COUNT_TYPE;
    void* entityData;
    JPH::BodyID bodyID;

    bool isDynamic;
};

static mat4s convertToMat4(JPH::RMat44& jphMat)
{
    JPH::Vec4 col1 = jphMat.mCol[0];
    JPH::Vec4 col2 = jphMat.mCol[1];
    JPH::Vec4 col3 = jphMat.mCol[2];
    JPH::Vec4 col4 = jphMat.mCol[3];

    mat4s positionMatrix;
    positionMatrix.m00 = col1.GetX();
    positionMatrix.m01 = col1.GetY();
    positionMatrix.m02 = col1.GetZ();
    positionMatrix.m03 = col1.GetW();

    positionMatrix.m10 = col2.GetX();
    positionMatrix.m11 = col2.GetY();
    positionMatrix.m12 = col2.GetZ();
    positionMatrix.m13 = col2.GetW();

    positionMatrix.m20 = col3.GetX();
    positionMatrix.m21 = col3.GetY();
    positionMatrix.m22 = col3.GetZ();
    positionMatrix.m23 = col3.GetW();

    positionMatrix.m30 = col4.GetX();
    positionMatrix.m31 = col4.GetY();
    positionMatrix.m32 = col4.GetZ();
    positionMatrix.m33 = col4.GetW();

    return positionMatrix;
}

static vec3s convertToVec3(const JPH::Vec3& jphVec3)
{
    vec3s vector;
    vector.x = jphVec3.GetX();
    vector.y = jphVec3.GetY();
    vector.z = jphVec3.GetZ();
    return vector;
}

static JPH::Vec3 convertToVec3JPH(const vec3s& vec)
{
    JPH::Vec3 vector;
    vector.SetX(vec.x);
    vector.SetY(vec.y);
    vector.SetZ(vec.z);
    return vector;
}

#endif // !UTILS_HDR
