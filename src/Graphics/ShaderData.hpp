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

struct PushConstants
{
    VkDeviceAddress vertexDataAddress;
    VkDeviceAddress modelPositionAddress;
    VkDeviceAddress sceneAddress;
};

#endif // !SHADER_DATA_HDR
