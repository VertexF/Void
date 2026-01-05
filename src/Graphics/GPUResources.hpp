#ifndef GPU_RESOURCE_HDR
#define GPU_RESOURCE_HDR

#include "Foundation/Platform.hpp"
#include "Graphics/GPUEnum.hpp"

#include <vulkan/vulkan.h>

#include "vender/vk_mem_alloc.h"

namespace 
{
    constexpr uint32_t INVALID_INDEX = UINT32_MAX;

    //Maximum number of images/render_targets/fbo attachments.
    constexpr uint8_t MAX_IMAGE_OUTPUT = 8;

    //Maximum number of layouts in the pipeline.
    constexpr uint8_t MAX_DESCRIPTOR_SET_LAYOUTS = 8;

    //Maximum simultaneous shader stages. Applicable to all different type of pipelines. 
    constexpr uint8_t MAX_SHADER_STAGES = 5;

    //Maximum list elements for both descriptor set layout and descriptor sets.
    constexpr uint8_t MAX_DESCRIPTOR_PER_SET = 16;

    constexpr uint8_t MAX_VERTEX_STREAMS = 16;
    constexpr uint8_t MAX_VERTEX_ATTRIBUTES = 16;

    constexpr uint32_t SUBMIT_HEADER_SENTINEL = 0xFEFEB7BA;
    constexpr uint32_t MAX_RESOURCE_DELETIONS = 64;

    //constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 3;
}

struct Allocator;
struct DeviceStateVulkan;

struct BufferHandle 
{
    uint32_t index;
};

struct TextureHandle 
{
    uint32_t index;
};

struct ShaderStateHandle 
{
    uint32_t index;
};

struct SamplerHandle 
{
    uint32_t index;
};

struct DescriptorSetLayoutHandle 
{
    uint32_t index;
};

struct DescriptorSetHandle 
{
    uint32_t index;
};

struct PipelineHandle 
{
    uint32_t index;
};

struct RenderPassHandle 
{
    uint32_t index;
};

static BufferHandle INVALID_BUFFER { INVALID_INDEX };
static TextureHandle INVALID_TEXTURE { INVALID_INDEX };
static ShaderStateHandle INVALID_SHADER { INVALID_INDEX };
static SamplerHandle INVALID_SAMPLER { INVALID_INDEX };
static DescriptorSetLayoutHandle INVALID_LAYOUT { INVALID_INDEX };
static DescriptorSetHandle INVALID_SET { INVALID_INDEX };
static PipelineHandle INVALID_PIPELINE { INVALID_INDEX };
static RenderPassHandle INVALID_PASS { INVALID_INDEX };

struct Rect2D 
{
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
};

struct Rect2DInt 
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
};

struct Viewport 
{
    Rect2DInt rect;
    float minDepth = 0.f;
    float maxDepth = 0.f;
};

struct ViewportState 
{
    uint32_t numViewports = 0;
    uint32_t numScissors = 0;

    Viewport* viewport = nullptr;
    Rect2DInt* scissors = nullptr;
};

struct StencilOperationState 
{
    VkStencilOp fail = VK_STENCIL_OP_KEEP;
    VkStencilOp pass = VK_STENCIL_OP_KEEP;
    VkStencilOp depthFail = VK_STENCIL_OP_KEEP;
    VkCompareOp compare = VK_COMPARE_OP_ALWAYS;

    uint32_t compareMask = 0xFF;
    uint32_t writeMask = 0xFF;
    uint32_t reference = 0xFF;
};

struct DepthStencilCreation 
{
    StencilOperationState front;
    StencilOperationState back;
    VkCompareOp depthComparison = VK_COMPARE_OP_ALWAYS;

    bool depthEnable = false;
    bool depthWriteEnable = false;
    bool stencilEnable = false;

    DepthStencilCreation() : depthEnable(false), depthWriteEnable(false), stencilEnable(false)
    {
    }

    DepthStencilCreation& setDepth(bool write, VkCompareOp comparisonTest);
};

struct BlendState 
{
    VkBlendFactor sourceColour = VK_BLEND_FACTOR_ONE;
    VkBlendFactor destinationColour = VK_BLEND_FACTOR_ONE;
    VkBlendOp colourOperation = VK_BLEND_OP_ADD;

    VkBlendFactor sourceAlpha = VK_BLEND_FACTOR_ONE;
    VkBlendFactor destinationAlpha = VK_BLEND_FACTOR_ONE;
    VkBlendOp alphaOperation = VK_BLEND_OP_ADD;

    VkColorComponentFlags colourWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    bool blendEnabled = false;
    bool separateBlend = false;

    BlendState() : blendEnabled(false), separateBlend(false) 
    {
    }

    BlendState& setColour(VkBlendFactor sourceCol, VkBlendFactor destinationCol, VkBlendOp colourOp);
    BlendState& setAlpha(VkBlendFactor sourceCol, VkBlendFactor destinationCol, VkBlendOp colourOp);
    BlendState& setColourWriteMask(VkColorComponentFlags mask);
};

struct BlendStateCreation 
{
    BlendState blendStates[MAX_IMAGE_OUTPUT];
    uint32_t activeStates = 0;

    BlendStateCreation reset();
    BlendState& addBlendState();
};

struct RasterisationCreation 
{
    VkCullModeFlagBits cullMode = VK_CULL_MODE_NONE;
    VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode fill = VK_POLYGON_MODE_FILL;
};

struct BufferCreation 
{
    VkBufferUsageFlags typeFlags = 0;
    ResourceType::Type usage = ResourceType::Type::IMMUTABLE;
    uint32_t size = 0;

    void* initialData = nullptr;
    const char* name = nullptr;

    BufferCreation& reset();
    BufferCreation& set(VkBufferUsageFlags flags, ResourceType::Type resourceUsage, uint32_t bufferSize);
    BufferCreation& setData(void *data);
    BufferCreation& setName(const char* inName);
};

struct TextureCreation 
{
    void* initialData = nullptr;
    uint16_t width = 1;
    uint16_t height = 1;
    uint16_t depth = 1;
    uint8_t mipmaps = 1;
    uint8_t flags = 0;

    VkFormat format = VK_FORMAT_UNDEFINED;

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;

    const char* name = nullptr;

    TextureCreation& setSize(uint16_t newWidth, uint16_t newHeight, uint16_t newDepth);
    TextureCreation& setFlags(uint8_t newMipmaps, uint8_t newFlags);
    TextureCreation& setFormatType(VkFormat newFormat, VkImageType newImageType, VkImageViewType newImageViewType);
    TextureCreation& setName(const char* inName);
    TextureCreation& setData(void* data);
};

struct SamplerCreation 
{
    VkFilter minFilter = VK_FILTER_NEAREST;
    VkFilter magFilter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    const char* name = nullptr;

    SamplerCreation& setMinMagMip(VkFilter min, VkFilter mag, VkSamplerMipmapMode mip);
    SamplerCreation& setAddressModeU(VkSamplerAddressMode modeU);
    SamplerCreation& setAddressModeUV(VkSamplerAddressMode modeU, VkSamplerAddressMode modeV);
    SamplerCreation& setAddressModeUVW(VkSamplerAddressMode modeU, VkSamplerAddressMode modeV, VkSamplerAddressMode modeW);
    SamplerCreation& setName(const char* inName);
};

struct ShaderStage 
{
    const char* code = nullptr;
    uint32_t codeSize = 0;
    uint32_t* data = nullptr;
    VkShaderStageFlagBits type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
};

struct ShaderStateCreation 
{
    ShaderStage stages[MAX_SHADER_STAGES];
    const char* name = nullptr;

    uint32_t stagesCount = 0;
    uint32_t spvInput = 0;

    ShaderStateCreation& reset();
    ShaderStateCreation& setName(const char* inName);
    ShaderStateCreation& addStage(const char* code, uint32_t codeSize, VkShaderStageFlagBits type);
    ShaderStateCreation& setSPVInput(bool value);
};

struct DescriptorSetLayoutCreation 
{
    //A single binding. It can be relative to one or more resource of the same type.
    struct Binding 
    {
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        uint16_t start = 0;
        uint16_t count = 0;
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        const char* name = nullptr;
    };

    Binding bindings[MAX_DESCRIPTOR_PER_SET];
    uint32_t numBindings = 0;
    uint32_t setIndex = 0;

    const char* name = nullptr;

    DescriptorSetLayoutCreation& reset();
    DescriptorSetLayoutCreation& addBinding(const Binding& binding);
    DescriptorSetLayoutCreation& setName(const char* inName);
    DescriptorSetLayoutCreation& setSetIndex(uint32_t index);
};

struct DescriptorSetCreation 
{
    uint32_t resources[MAX_DESCRIPTOR_PER_SET];
    SamplerHandle samplers[MAX_DESCRIPTOR_PER_SET];
    uint16_t bindings[MAX_DESCRIPTOR_PER_SET];

    DescriptorSetLayoutHandle layout;
    uint32_t numResources = 0;

    const char* name = nullptr;

    DescriptorSetCreation& reset();
    DescriptorSetCreation& setLayout(DescriptorSetLayoutHandle newLayout);
    DescriptorSetCreation& texture(TextureHandle texture, uint16_t binding);
    DescriptorSetCreation& buffer(BufferHandle buffer, uint16_t binding);
    //TODO: Seperate samplers from textures.
    DescriptorSetCreation& textureSampler(TextureHandle texture, SamplerHandle sampler, uint16_t binding);
    DescriptorSetCreation& setName(const char* inName);
};

struct DescriptorSetUpdate 
{
    DescriptorSetHandle descriptorSet;
    uint32_t frameIssued = 0;
};

struct VertexAttribute 
{
    uint16_t location = 0;
    uint16_t binding = 0;
    uint32_t offset = 0;

    VkFormat format = VK_FORMAT_MAX_ENUM;
};

struct VertexStream 
{
    uint16_t binding = 0;
    uint16_t stride = 0;
    VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_MAX_ENUM;
};

struct VertexInputCreation 
{
    uint32_t numVertexStreams = 0;
    uint32_t numVertexAttributes = 0;

    VertexStream vertexStreams[MAX_VERTEX_STREAMS];
    VertexAttribute vertexAttributes[MAX_VERTEX_ATTRIBUTES];

    VertexInputCreation& reset();
    VertexInputCreation& addVertexStream(const VertexStream& stream);
    VertexInputCreation& addVertexAttribute(const VertexAttribute& attribute);
};

struct RenderPassOutput 
{
    VkFormat colourFormats[MAX_IMAGE_OUTPUT];
    VkFormat depthStencilFormat;
    uint32_t numColourFormats;

    RenderPassType::Operations colourOperations = RenderPassType::Operations::DONT_CARE;
    RenderPassType::Operations depthOperations = RenderPassType::Operations::DONT_CARE;
    RenderPassType::Operations stencilOperations = RenderPassType::Operations::DONT_CARE;

    RenderPassOutput& reset();
    RenderPassOutput& colour(VkFormat format);
    RenderPassOutput& depth(VkFormat format);
    RenderPassOutput& setOperations(RenderPassType::Operations colour,
                                    RenderPassType::Operations depth,
                                    RenderPassType::Operations stencil);
};

struct RenderPassCreation 
{
    uint16_t numRenderTargets = 0;
    RenderPassType::Types type = RenderPassType::Types::GEOMETRY;

    TextureHandle outputTextures[MAX_IMAGE_OUTPUT];
    TextureHandle depthStencilTexture;

    float scaleX = 1.f;
    float scaleY = 1.f;
    uint8_t resize = 1;

    RenderPassType::Operations colourOperations = RenderPassType::Operations::DONT_CARE;
    RenderPassType::Operations depthOperations = RenderPassType::Operations::DONT_CARE;
    RenderPassType::Operations stencilOperations = RenderPassType::Operations::DONT_CARE;

    const char* name = nullptr;

    RenderPassCreation& reset();
    RenderPassCreation& addRenderTexture(TextureHandle texture);
    RenderPassCreation& setScaling(float newScaleX, float newScaleY, uint8_t newResize);
    RenderPassCreation& setDepthStencilTexture(TextureHandle texture);
    RenderPassCreation& setName(const char* inName);
    RenderPassCreation& setType(RenderPassType::Types renderPassType);
    RenderPassCreation& setOperations(RenderPassType::Operations colour,
                                        RenderPassType::Operations depth,
                                        RenderPassType::Operations stencil);
};

struct PipelineCreation 
{
    RasterisationCreation rasterisation;
    DepthStencilCreation depthStencil;
    BlendStateCreation blendState;
    VertexInputCreation vertexInput;
    ShaderStateCreation shaders;

    RenderPassOutput renderPass;
    DescriptorSetLayoutHandle descriptorSetLayout[MAX_DESCRIPTOR_SET_LAYOUTS];
    const ViewportState* viewport = nullptr;

    uint32_t numActiveLayouts = 0;

    const char* name = nullptr;

    PipelineCreation& addDescriptorSetLayout(DescriptorSetLayoutHandle handle);
    RenderPassOutput& renderPassOutput();
};

namespace TextureFormat 
{
    static bool isDepthStencil(VkFormat value) 
    {
        return value == VK_FORMAT_D16_UNORM_S8_UINT || value == VK_FORMAT_D24_UNORM_S8_UINT || value == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    static bool isDepthOnly(VkFormat value)
    {
        return value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_D32_SFLOAT;
    }

    static bool isStencilOnly(VkFormat value) 
    {
        return value == VK_FORMAT_S8_UINT;
    }

    static bool hasDepth(VkFormat value) 
    {
        return (value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_S8_UINT) || 
                (value >- VK_FORMAT_D16_UNORM_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT );
    }

    static bool hasStencil(VkFormat value) 
    {
        return value >= VK_FORMAT_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    static bool hasDepthOrStencil(VkFormat value) 
    {
        return value >= VK_FORMAT_D16_UNORM && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

}//TextureFormat

struct ResourceData 
{
    void* data = nullptr;
};

struct ResourceBinding 
{
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint16_t start = 0;
    uint16_t count = 0;
    uint16_t set = 0;

    const char* name = nullptr;
};

struct ShaderStateDescription 
{
    void* nativeHandle = nullptr;
    const char* name = nullptr;
};

struct BufferDescription 
{
    void* nativeHandle = nullptr;
    const char* name = nullptr;

    VkBufferUsageFlags typeFlags = 0;
    ResourceType::Type usage = ResourceType::Type::IMMUTABLE;
    uint32_t size = 0;
    BufferHandle parentHandle;
};

struct TextureDescription 
{
    void* nativeHandle = nullptr;
    const char* name = nullptr;

    uint16_t width = 1;
    uint16_t height = 1;
    uint16_t depth = 1;
    uint8_t mipmaps = 1;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;
};

struct SamplerDescription 
{
    const char* name = nullptr;

    VkFilter minFilter = VK_FILTER_NEAREST;
    VkFilter magFilter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
};

struct DescriptorSetLayoutDescription 
{
    ResourceBinding bindings[MAX_DESCRIPTOR_PER_SET];
    uint32_t numActiveBindings = 0;
};

struct DescriptorSetDescription 
{
    ResourceData resources[MAX_DESCRIPTOR_PER_SET];
    uint32_t numActiveResources = 0;
};

struct PipelineDescription 
{
    ShaderStateHandle shader;
};

struct MapBufferParameters 
{
    BufferHandle buffer;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct ImageBarrier 
{
    TextureHandle texture;
};

struct MemoryBarrierHandle
{
    BufferHandle buffer;
};

struct ExecutionBarrier 
{
    ImageBarrier imageBarriers[8];
    MemoryBarrierHandle memoryBarriers[8];

    VkPipelineStageFlagBits sourcePipelineStage;
    VkPipelineStageFlagBits destinationPipelineStage;

    uint32_t newBarrierExperimental = UINT32_MAX;
    uint32_t loadOperation = 0;
    uint32_t numImageBarriers;
    uint32_t numMemoryBarriers;

    ExecutionBarrier& reset();
    ExecutionBarrier& set(VkPipelineStageFlagBits source, VkPipelineStageFlagBits destination);
    ExecutionBarrier& addImageBarrier(const ImageBarrier& imageBarrier);
    ExecutionBarrier& addMemoryBarrier(const MemoryBarrierHandle& memoryBarrier);
};

struct ResourceUpdate 
{
    ResourceDeletion::Types type;
    uint32_t handle;
    uint32_t currentFrame;
};

struct Buffer 
{
    VkBuffer vkBuffer;
    VmaAllocation vmaAllocation;
    VkDeviceMemory vkDeviceMemory;
    VkDeviceSize vkDeviceSize;

    VkBufferUsageFlags typeFlags = 0;
    ResourceType::Type usage = ResourceType::Type::IMMUTABLE;
    uint32_t size = 0;
    uint32_t globalOffset = 0;

    BufferHandle handle;
    BufferHandle parentBuffer;

    const char* name = nullptr;
};

struct Sampler 
{
    VkSampler vkSampler;

    VkFilter minFilter = VK_FILTER_NEAREST;
    VkFilter magFilter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    const char* name = nullptr;
};

struct Texture 
{
    VkImage vkImage;
    VkImageView vkImageView;
    VmaAllocation vmaAllocation;

    Sampler* sampler = nullptr;
    const char* name = nullptr;

    VkImageLayout vkImageLayout;
    VkFormat vkFormat;

    TextureHandle handle;
    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;

    uint16_t width = 1;
    uint16_t height = 1;
    uint16_t depth = 1;
    uint8_t mipmaps = 1;
    uint8_t flags = 0;
};

struct ShaderState 
{
    VkPipelineShaderStageCreateInfo shaderStateInfo[MAX_SHADER_STAGES];
        
    const char* name = nullptr;

    uint32_t activeShaders = 0;
    bool graphicsPipeline = false;
};

struct DescriptorBinding 
{
    VkDescriptorType type;

    uint16_t start = 0;
    uint16_t count = 0;
    uint16_t set = 0;

    const char* name = nullptr;
};

struct DescriptorSetLayout 
{
    VkDescriptorSetLayout vkDescriptorSetLayout;

    VkDescriptorSetLayoutBinding* vkBinding = nullptr;
    DescriptorBinding* bindings = nullptr;
    uint16_t numBindings = 0;
    uint16_t setIndex = 0;

    DescriptorSetLayoutHandle handle;
};

struct DescriptorSet 
{
    VkDescriptorSet vkDescriptorSet;

    uint32_t* resources = nullptr;
    SamplerHandle* samplers = nullptr;
    uint16_t* bindings = nullptr;

    const DescriptorSetLayout* layout = nullptr;
    uint32_t numResources = 0;
};

struct Pipeline 
{
    VkPipeline vkPipeline;
    VkPipelineLayout vkPipelineLayout;

    VkPipelineBindPoint vkBindPoint;

    ShaderStateHandle shaderState;

    const DescriptorSetLayout* descriptorSetLayout[MAX_DESCRIPTOR_SET_LAYOUTS];
    DescriptorSetLayoutHandle descriptorSetLayoutHandle[MAX_DESCRIPTOR_SET_LAYOUTS];
    uint32_t numActiveLayouts = 0;

    DepthStencilCreation depthStencil;
    BlendStateCreation blendState;
    RasterisationCreation rasterisation;

    PipelineHandle handle;
    bool graphicsPipeline = true;
};

struct RenderPass 
{
    VkRenderPass vkRenderPass;
    VkFramebuffer vkFrameBuffer;

    RenderPassOutput output;

    TextureHandle outputTextures[MAX_IMAGE_OUTPUT];
    TextureHandle outputDepth;

    RenderPassType::Types type;

    float scaleX = 1.f;
    float scaleY = 1.f;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t dispatchX = 0;
    uint16_t dispatchY = 0;
    uint16_t dispatchZ = 0;

    uint8_t resize = 0;
    uint8_t numRenderTargets = 0;

    const char* name = nullptr;
};

static const char* toCompilerExtension(VkShaderStageFlagBits value) 
{
    switch (value) 
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return "vert";
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return "frag";
    case VK_SHADER_STAGE_COMPUTE_BIT:
        return "comp";
    default:
        return "";
    }
}

static const char* toStageDefines(VkShaderStageFlagBits value) 
{
    switch (value) 
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return "VERTEX";
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return "FRAGMENT";
    case VK_SHADER_STAGE_COMPUTE_BIT:
        return "COMPUTE";
    default:
        return "";
    }
}

static VkAccessFlags toVKPipelineStage(ResourceState state) 
{
    VkAccessFlags returnVal = 0;
    if (state & RESOURCE_STATE_COPY_SOURCE) 
    {
        returnVal |= VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (state & RESOURCE_STATE_COPY_DEST) 
    {
        returnVal |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) 
    {
        returnVal |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }

    if (state & RESOURCE_STATE_INDEX_BUFFER) 
    {
        returnVal |= VK_ACCESS_INDEX_READ_BIT;
    }

    if (state & RESOURCE_STATE_UNORDERED_ACCESS) 
    {
        returnVal |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }

    if (state & RESOURCE_STATE_INDIRECT_ARGUMENT) 
    {
        returnVal |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }

    if (state & RESOURCE_STATE_RENDER_TARGET) 
    {
        returnVal |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (state & RESOURCE_STATE_DEPTH_WRITE) 
    {
        returnVal |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    if (state & RESOURCE_STATE_SHADER_RESOURCE) 
    {
        returnVal |= VK_ACCESS_SHADER_READ_BIT;
    }

    if (state & RESOURCE_STATE_PRESENT) 
    {
        returnVal |= VK_ACCESS_MEMORY_READ_BIT;
    }

#ifdef ENABLE_RAYTRACING
    if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) 
    {
        returnVal |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
    }
#endif // ENABLE_RAYTRACING

    return returnVal;
}

static VkImageLayout utilToVKImageLayout(ResourceState usage) 
{
    if (usage & RESOURCE_STATE_COPY_SOURCE) 
    {
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    if (usage & RESOURCE_STATE_COPY_DEST) 
    {
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    if (usage & RESOURCE_STATE_RENDER_TARGET) 
    {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (usage & RESOURCE_STATE_DEPTH_WRITE) 
    {
        return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
    }

    if (usage & RESOURCE_STATE_DEPTH_READ) 
    {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    if (usage & RESOURCE_STATE_UNORDERED_ACCESS) 
    {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    if (usage & RESOURCE_STATE_SHADER_RESOURCE) 
    {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if (usage & RESOURCE_STATE_PRESENT) 
    {
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    if (usage == RESOURCE_STATE_COMMON) 
    {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

//Determines pipeline stages involved for given accesses.
static VkPipelineStageFlags utilDeterminePipelineStageFlags(VkAccessFlags accessFlags, VkQueueFlagBits queueFamily)
{
    VkPipelineStageFlags flags = 0;

    switch (queueFamily)
    {
    case VK_QUEUE_GRAPHICS_BIT:
        if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0) 
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }

        if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0) 
        {
            flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            //TODO: Maybe added additional shaders when needed.
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
#ifdef ENABLE_RAYTRACING
            if (renderer->vulkan.raytracingExtension) 
            {
                flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
            }
#endif // ENABLE_RAYTRACING

            if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0) 
            {
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }

            if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
            {
                flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }
        }

        break;
    case VK_QUEUE_COMPUTE_BIT:
        if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
            (accessFlags &  VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
            (accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
            (accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0) 
        {
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0) 
        {
            flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        break;
    case VK_QUEUE_TRANSFER_BIT:
    default:
        break;
    }

    //Compatible with both compute and graphics queues.
    if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0) 
    {
        flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }

    if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0) 
    {
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0) 
    {
        flags |= VK_PIPELINE_STAGE_HOST_BIT;
    }

    if (accessFlags == 0) 
    {
        flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    return flags;
}

#endif // !GPU_RESOURCE_HDR
