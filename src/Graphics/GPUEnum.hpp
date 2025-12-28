#ifndef GPU_ENUM_HDR
#define GPU_ENUM_HDR

#include "Foundation/Platform.hpp"

namespace ColourType
{
    enum Type 
    {
        RED,
        GREEN,
        BLUE,
        ALPHA,
        ALL,

        COUNT
    };

    enum Mask
    {
        RED_MASK = 1 << 0,
        GREEN_MASK = 1 << 1,
        BLUE_MASK = 1 << 2,
        ALPHA_MASK = 1 << 3,

        ALL_MASK = RED_MASK | GREEN_MASK | BLUE_MASK | ALPHA_MASK
    };

    static const char* COLOUR_NAMES[] = 
    {
        "RED", "GREEN", "BLUE", "ALPHA", "ALL", "COUNT"
    };

    static const char* toString(Type type)
    {
        return (static_cast<uint32_t>(type) < Type::COUNT ? COLOUR_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Colour

namespace Cull 
{
    enum CullType 
    {
        NONE,
        FRONT,
        BACK,

        COUNT
    };

    enum Mask 
    {
        NONE_MASK = 1 << 0,
        FRONT_MASK = 1 << 1,
        BACK_MASK = 1 << 2,
        COUNT_MASK = 1 << 3
    };

    static const char* CULL_NAMES[] =
    {
        "NONE", "FRONT", "BACK", "COUNT"
    };

    static const char* toString(CullType type) 
    {
        return (static_cast<uint32_t>(type) < CullType::COUNT ? CULL_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Cull

namespace Depth
{
    enum DepthMaskType 
    {
        ZERO,
        ALL,
        COUNT
    };

    enum Mask 
    {
        ZERO_MASK = 1 << 0,
        ALL_MASK = 1 << 1,
        COUNT_MASK = 1 << 2
    };

    static const char* DEPTH_NAMES[] =
    {
        "ZERO", "ALL", "COUNT"
    };

    static const char* toString(DepthMaskType type)
    {
        return (static_cast<uint32_t>(type) < DepthMaskType::COUNT ? DEPTH_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Depth

namespace Fill 
{
    enum FillType 
    {
        WIREFRAME, 
        SOLID, 
        POINT,

        COUNT
    };

    enum Mask 
    {
        WIREFRAME_MASK = 1 << 0,
        SOLID_MASK = 1 << 1,
        POINT_MASK = 1 << 2,
        COUNT_MASK = 1 << 3
    };

    static const char* FILL_NAMES[] =
    {
        "WIREFRAME", "SOLID", "POINT", "COUNT"
    };

    static const char* toString(FillType type)
    {
        return (static_cast<uint32_t>(type) < FillType::COUNT ? FILL_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Fill

namespace Winding 
{
    enum WindingType 
    {
        CLOCKWISE,
        ANTI_CLOCKWISE,

        COUNT
    };

    enum Mask 
    {
        CLOCKWISE_MASK = 1 << 0,
        ANTI_CLOCKWISE_MASK = 1 << 1,
        COUNT_MASK = 1 << 2
    };

    static const char* WINDING_NAMES[] = 
    {
        "CLOCKWISE", "ANTI_CLOCKWISE", "COUNT"
    };

    static const char* toString(WindingType type)
    {
        return (static_cast<uint32_t>(type) < WindingType::COUNT ? WINDING_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Winding

namespace Stencil 
{
    enum StencilType 
    {
        KEEP,
        ZERO,
        REPLACE,
        INCREASE_STA,
        DECREASE_STA,
        INVERT,
        INCREASE,
        DECREASE,

        COUNT
    };

    enum Mask 
    {
        KEEP_MASK = 1 << 0,
        ZERO_MASK = 1 << 1,
        REPLACE_MASK = 1 << 2,
        INCREASE_STA_MASK = 1 << 3,
        DECREASE_STA_MASK = 1 < 4,
        INVERT_MASK = 1 << 5,
        INCREASE_MASK = 1 << 6,
        DECREASE_MASK = 1 << 7,

        COUNT_MASK = 1 << 8
    };

    static const char* STENCIL_NAMES[] =
    {
        "KEEP", "ZERO", "REPLACE", "INCREASE_STA", "DECREASE_STA", "INVERT", "INCREASE", "DECREASE", "COUNT"
    };

    static const char* toString(StencilType type) 
    {
        return (static_cast<uint32_t>(type) < StencilType::COUNT ? STENCIL_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Stencil

namespace Topology 
{
    enum TopologyType
    {
        UNKNOWN, 
        POINT,
        LINE,
        TRIANGLE,
        PATCH,

        COUNT
    };

    enum Mask 
    {
        UNKNOWN_MASK = 1 << 0,
        POINT_MASK = 1 << 1,
        LINE_MASK = 1 << 2,
        TRIANGLE_MASK = 1 << 3,
        PATCH_MASK = 1 << 4,

        COUNT_MASK = 1 << 5
    };

    static const char* TOPOLOGY_NAMES[] =
    {
        "UNKNOWN", "POINT", "LINE", "TRIANGLE", "PATCH"
    };

    static const char* toString(TopologyType type)
    {
        return (static_cast<uint32_t>(type) < TopologyType::COUNT ? TOPOLOGY_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Topology

namespace ResourceType
{
    enum Type 
    {
        IMMUTABLE,
        DYNAMIC,
        STREAM,

        COUNT
    };

    enum Mask 
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

namespace Index 
{
    enum IndexType 
    {
        UINT16,
        UINT32,

        COUNT
    };

    enum Mask 
    {
        UINT16_MASK = 1 << 0,
        UINT32_MASK = 1 << 1,

        COUNT_MASK = 1 << 2,
    };

    static const char* INDEX_NAMES[] =
    {
        "UINT16", "UINT32", "COUNT"
    };

    static const char* toString(IndexType type) 
    {
        return (static_cast<uint32_t>(type) < IndexType::COUNT ? INDEX_NAMES[static_cast<int>(type)] : "Unsupported");
    }

}//Index

namespace TextureType
{
    enum Type 
    {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_3D,
        TEXTURE_1D_ARRAY,
        TEXTURE_2D_ARRAY,
        TEXTURE_CUBE_ARRAY,

        COUNT
    };

    enum Mask
    {
        TEXTURE_1D_MASK = 1 << 0,
        TEXTURE_2D_MASK = 1 << 1,
        TEXTURE_3D_MASK = 1 << 2,
        TEXTURE_1D_ARRAY_MASK = 1 << 3,
        TEXTURE_2D_ARRAY_MASK = 1 << 4,
        TEXTURE_CUBE_ARRAY_MASK = 1 << 5,

        COUNT_MASK = 1 << 6
    };

    static const char* TEXTURE_NAMES[] =
    {
        "TEXTURE_1D", "TEXTURE_2D", "TEXTURE_3D", "TEXTURE_1D_ARRAY", "TEXTURE_2D_ARRAY", "TEXTURE_3D_ARRAY", "COUNT"
    };

    static const char* toString(Type type)
    {
        return (static_cast<uint32_t>(type) < Type::COUNT ? TEXTURE_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Texture

namespace VertexFormat 
{
    enum VertexComponentFormatType
    {
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,

        MAT4,

        BYTE,
        BYTE4N,
        UBYTE,
        UBYTE4N,

        SHORT2,
        SHORT2N,
        SHORT4,
        SHORT4N,

        UINT,
        UINT2, 
        UINT4,

        COUNT
    };

    static const char* VERTEX_FORMAT_NAMES[] =
    {
        "FLOAT", "FLOAT2", "FLOAT3", "FLOAT4", "MAT4", "BYTE", "BYTE4N", "UBYTE", "UBYTEN4",
        "SHORT2", "SHORT2N", "SHORT4", "SHORT4N", "UINT", "UINT2", "UINT4", "COUNT"
    };

    static const char* toString(VertexComponentFormatType type)
    {
        return (static_cast<uint32_t>(type) < VertexComponentFormatType::COUNT ? VERTEX_FORMAT_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//VertexFormat

namespace VertexInput 
{
    enum VertexInputRateType 
    {
        PER_VERTEX,
        PER_INSTANCE,
        COUNT
    };

    enum Mask 
    {
        PER_VERTEX_MASK = 1 << 0,
        PER_INSTANCE_MASK = 1 << 1,
        COUNT_MASK = 1 << 2
    };

    static const char* VERTEX_INPUT_NAMES[] =
    {
        "PER_VERTEX", "PER_INSTANCE", "COUNT"
    };

    static const char* toString(VertexInputRateType type)
    {
        return (static_cast<uint32_t>(type) < VertexInputRateType::COUNT ? VERTEX_INPUT_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//VertexInput

namespace Logic 
{
    enum LogicOperationType 
    {
        CLEAR,
        SET,
        COPY,
        COPY_INVERTED,
        NOOP,
        INVERT,
        AND,
        NAND,
        OR,
        NOR,
        XOR,
        EQUIV,
        AND_REVERSE,
        AND_INVERTED,
        OR_REVERSE,
        OR_INVERTED,
        COUNT
    };

    enum Mask 
    {
        CLEAR_MASK = 1 << 0,
        SET_MASK = 1 << 1,
        COPY_MASK = 1 << 2,
        COPY_INVERTED_MASK = 1 << 3,
        NOOP_MASK = 1 << 4,
        INVERT_MASK = 1 << 5,
        AND_MASK = 1 << 6,
        NAND_MASK = 1 << 7,
        OR_MASK = 1 << 8,
        NOR_MASK = 1 << 9,
        XOR_MASK = 1 << 10,
        EQUIV_MASK = 1 << 11,
        AND_REVERSE_MASK = 1 << 12,
        AND_INVERTED_MASK = 1 << 13,
        OR_REVERSE_MASK = 1 << 14,
        OR_INVERTED_MASK = 1 << 15,
        COUNT_MASK = 1 << 16
    };

    static const char* LOGIC_NAME[] = 
    {
        "CLEAR", "SET", "COPY", "COPY_INVERTED", "NOOP", 
        "INVERT", "AND", "NAND", "OR", "NOR", "XOR", "EQUIV",
        "AND_REVERSE", "AND_INVERTED", "OR_REVERSE", "OR_INVERTED",
        "COUNT"
    };

    static const char* toString(LogicOperationType type)
    {
        return (static_cast<uint32_t>(type) < LogicOperationType::COUNT ? LOGIC_NAME[static_cast<int>(type)] : "Unsupported");
    }
}//Logic
    
namespace Queue 
{
    enum QueueType 
    {
        GRAPHICS,
        COMPUTE,
        COPY_TRANSFER,
        COUNT
    };

    enum Mask 
    {
        GRAPHICS_MASK = 1 << 0,
        COMPUTE_MASK = 1 << 1,
        COPY_TRANSFER_MASK = 1 << 2,
        COUNT_MASK = 1 << 3
    };

    static const char* QUEUE_NAMES[] =
    {
        "GRAPHICS", "COMPUTE", "COPY_TRANSFER", "COUNT"
    };

    static const char* toString(QueueType type)
    {
        return (static_cast<uint32_t>(type) < QueueType::COUNT ? QUEUE_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Queue

namespace Command 
{
    enum CommandTypes 
    {
        BIND_PIPELINE,
        BIND_RESOURCE_TABLE,
        BIND_VERTEX_BUFFER,
        BIND_INDEX_BUFFER,
        BIND_RESOURCE_SET,

        DRAW,
        DRAW_INDEXED,
        DRAW_INSTANCED,
        DRAW_INDEXED_INSTANCED,

        DISPATCH,
        COPY_RESOURCE,
        SET_SCISSOR,
        SET_VIEWPORT,

        CLEAR,
        CLEAR_DEPTH,
        CLEAR_STENCIL,

        BEGIN_PASS,
        END_PASS,

        COUNT
    };

    static const char* COMMAND_TYPE_NAMES[] =
    {
        "BIND_PIPELINE", "BIND_RESOURCE_TABLE", "BIND_VERTEX_BUFFER", "BIND_INDEX_BUFFER", "BIND_RESOURCE_SET",
        "DRAW", "DRAW_INDEXED", "DRAW_INSTANCED", "DRAW_INDEXED_INSTANCED", "DISPATCH", "COPY_RESOURCE", "SET_SCISSOR", "SET_VIEWPORT",
        "CLEAR", "CLEAR_DEPTH", "CLEAR_STENCIL", "BEGIN_PASS", "END_PASS", "COUNT"
    };

    static const char* toString(CommandTypes type)
    {
        return (static_cast<uint32_t>(type) < CommandTypes::COUNT ? COMMAND_TYPE_NAMES[static_cast<int>(type)] : "Unsupported");
    }
}//Command

enum DeviceExtensions
{
    DeviceExtensions_DebugCallback = 1 << 0
};

namespace TextureFlags 
{
    enum Flags 
    {
        DEFAULT,
        RENDER_TARGET,
        COMPUTE,
        COUNT
    };

    enum Mask
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

namespace PipelineStage
{
    enum Stage
    {
        DRAW_INDIRECT,
        VERTEX_INPUT,
        VERTEX_SHADER,
        FRAGEMENT_SHADER,
        RENDER_TARGET,
        COMPUTE_SHADER,
        TRANSFER
    };

    enum Mask 
    {
        DRAW_INDIRECT_MASK = 1 << 0,
        VERTEX_INPUT_MASK = 1 << 1,
        VERTEX_SHADER_MASK = 1 << 2,
        FRAGEMENT_SHADER_MASK = 1 << 3,
        RENDER_TARGET_MASK = 1 << 4,
        COMPUTE_SHADER_MASK = 1 << 5,
        TRANSFER_MASK = 1 << 6
    };
}//PipelineStage

namespace RenderPassType
{
    enum Types 
    {
        GEOMETRY,
        SWAPCHAIN,
        COMPUTE
    };

    enum Operations 
    {
        DONT_CARE,
        LOAD,
        CLEAR,
        COUNT
    };

}//RenderPass

namespace ResourceDeletion 
{
    enum Types 
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
    enum Types 
    {
        IMMEDIATE,
        VSYNC,
        VSYNC_FAST,
        VSYNC_RELAXED,
        COUNT
    };
}//PresentMode

typedef enum ResourceState 
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
