#ifndef GPU_ENUM_HDR
#define GPU_ENUM_HDR

#include "Foundation/Platform.hpp"

namespace ResourceType
{
    enum Type : uint8_t
    {
        IMMUTABLE,
        DYNAMIC,
        STREAM,

        COUNT
    };

    enum Mask : uint8_t
    {
        IMMUTABLE_MASK = 1 << 0,
        DYNAMIC_MASK = 1 << 1,
        STREAM_MASK = 1 << 2,

        COUNT_MASK = 1 << 3
    };

    static const char* RESOURCE_NAMES[] =
    {
        "IMMUTABLE", "DYNAMIC", "STREAM"
    };

    static const char* toString(Type type)
    {
        return (static_cast<uint32_t>(type) < Type::COUNT ? RESOURCE_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Resource

namespace TextureFlags 
{
    enum Flags : uint8_t
    {
        DEFAULT,
        RENDER_TARGET,
        COMPUTE,
        COUNT
    };

    enum Mask : uint8_t
    {
        DEFAULT_MASK = 1 << 0,
        RENDER_TARGET_MASK = 1 << 1,
        COMPUTE_MASK = 1 << 2
    };

    static const char* TEXTURE_FLAG_NAMES[] =
    {
        "DEFAULT", "RENDER_TARGET", "COMPUTE", "COUNT"
    };

    static const char* toString(Flags type)
    {
        return (static_cast<uint32_t>(type) < Flags::COUNT ? TEXTURE_FLAG_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//TextureFlags

namespace RenderPassType
{
    enum Types : uint8_t
    {
        GEOMETRY,
        SWAPCHAIN,
        COMPUTE
    };

    enum Operations : uint8_t
    {
        DONT_CARE,
        LOAD,
        CLEAR,
        COUNT
    };

}//RenderPass

namespace ResourceDeletion 
{
    enum Types : uint8_t
    {
        BUFFER,
        TEXTURE,
        PIPELINE,
        SAMPLER,
        DESCRIPTOR_SET_LAYOUT,
        DESCRIPTOR_SET,
        RENDER_PASS,
        SHADER_STATE,
        COUNT
    };
}//ResourceDeletion

namespace PresentMode 
{
    enum Types : uint8_t
    {
        IMMEDIATE,
        VSYNC,
        VSYNC_FAST,
        VSYNC_RELAXED,
        COUNT
    };
}//PresentMode

typedef enum ResourceState : uint8_t
{
    RESOURCE_STATE_UNDEFINED = 0,
    RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    RESOURCE_STATE_INDEX_BUFFER = 0x2,
    RESOURCE_STATE_RENDER_TARGET = 0x4,
    RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
    RESOURCE_STATE_DEPTH_WRITE = 0x10,
    RESOURCE_STATE_DEPTH_READ = 0x20,
    RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
    RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
    RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
    RESOURCE_STATE_STREAM_OUT = 0x100,
    RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
    RESOURCE_STATE_COPY_DEST = 0x400,
    RESOURCE_STATE_COPY_SOURCE = 0x800,
    RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
    RESOURCE_STATE_PRESENT = 0x1000,
    RESOURCE_STATE_COMMON = 0x2000,
    RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x4000,
    RESOURCE_STATE_SHADING_RATE_SOURCE = 0x8000,
} ResourceState;

#endif // !GPU_ENUM_HDR
