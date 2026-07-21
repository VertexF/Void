#ifndef SHADER_DATA_HDR
#define SHADER_DATA_HDR

#include "cglm/struct/mat4.h"
#include "cglm/struct/vec4.h"

#include <vulkan/vulkan_core.h>

struct UniformData
{
    mat4s view;
    mat4s project;
    mat4s globalModel;
    vec4s eye;
    vec4s light;
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

struct SceneData2D
{
    mat4s view;
    mat4s project;
};

struct PushConstants
{
    VkDeviceAddress vertexDataAddress;
    VkDeviceAddress modelPositionAddress;
    VkDeviceAddress sceneAddress;
};

struct ParticleDrawCallPushConstant 
{
    VkDeviceAddress indirectAddress;
    VkDeviceAddress indirectCountAddress;
    VkDeviceAddress activeBufferData;
};

struct ParticlePushConstant
{
    VkDeviceAddress particleData;
    VkDeviceAddress particleSetData;
    VkDeviceAddress activeBufferData;
    float delta;
    uint32_t particleSet;
    uint32_t offset;
};

#endif // !SHADER_DATA_HDR
