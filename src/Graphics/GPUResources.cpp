#include "GPUResources.hpp"

DepthStencilCreation& DepthStencilCreation::setDepth(bool write, VkCompareOp comparisonTest) 
{
    depthWriteEnable = write;
    depthComparison = comparisonTest;
    depthEnable = true;

    return *this;
}

BlendState& BlendState::setColour(VkBlendFactor sourceCol, VkBlendFactor destinationCol, VkBlendOp colourOp) 
{
    sourceColour = sourceCol;
    destinationColour = destinationCol;
    colourOperation = colourOp;
    blendEnabled = true;

    return *this;
}

BlendState& BlendState::setAlpha(VkBlendFactor sourceCol, VkBlendFactor destinationCol, VkBlendOp colourOp)
{
    sourceColour = sourceCol;
    destinationColour = destinationCol;
    colourOperation = colourOp;
    separateBlend = true;

    return *this;
}

BlendState& BlendState::setColourWriteMask(VkColorComponentFlags mask)
{
    colourWriteMask = mask;

    return *this;
}

BlendStateCreation BlendStateCreation::reset() 
{
    activeStates = 0;

    return *this;
}

BlendState& BlendStateCreation::addBlendState() 
{
    return blendStates[activeStates++];
}

BufferCreation& BufferCreation::reset() 
{
    size = 0;
    initialData = nullptr;

    return *this;
}

BufferCreation& BufferCreation::set(VkBufferUsageFlags flags, ResourceType::Type resourceUsage, uint32_t bufferSize) 
{
    typeFlags = flags;
    usage = resourceUsage;
    size = bufferSize;

    return *this;
}

BufferCreation& BufferCreation::setData(void* data) 
{
    initialData = data;

    return *this;
}

BufferCreation& BufferCreation::setName(const char* inName) 
{
    name = inName;

    return *this;
}

TextureCreation& TextureCreation::setSize(uint16_t newWidth, uint16_t newHeight, uint16_t newDepth)
{
    width = newWidth;
    height = newHeight;
    depth = newDepth;

    return *this;
}

TextureCreation& TextureCreation::setFlags(uint8_t newMipmaps, uint8_t newFlags)
{
    mipmaps = newMipmaps;
    flags = newFlags;

    return *this;
}

TextureCreation& TextureCreation::setFormatType(VkFormat newFormat, VkImageType newImageType, VkImageViewType newImageViewType)
{
    format = newFormat;
    imageType = newImageType;
    imageViewType = newImageViewType;

    return *this;
}

TextureCreation& TextureCreation::setName(const char* inName) 
{
    name = inName;
    return *this;
}

TextureCreation& TextureCreation::setData(void* data) 
{
    initialData = data;
    return *this;
}

SamplerCreation& SamplerCreation::setMinMagMip(VkFilter min, VkFilter mag, VkSamplerMipmapMode mip) 
{
    minFilter = min;
    magFilter = mag;
    mipFilter = mip;

    return *this;
}

SamplerCreation& SamplerCreation::setAddressModeU(VkSamplerAddressMode modeU) 
{
    addressModeU = modeU;

    return *this;
}

SamplerCreation& SamplerCreation::setAddressModeUV(VkSamplerAddressMode modeU, VkSamplerAddressMode modeV) 
{
    addressModeU = modeU;
    addressModeV = modeV;

    return *this;
}

SamplerCreation& SamplerCreation::setAddressModeUVW(VkSamplerAddressMode modeU, VkSamplerAddressMode modeV, VkSamplerAddressMode modeW) 
{
    addressModeU = modeU;
    addressModeV = modeV;
    addressModeW = modeW;

    return *this;
}

SamplerCreation& SamplerCreation::setName(const char* inName) 
{
    name = inName;
    return *this;
}

ShaderStateCreation& ShaderStateCreation::reset() 
{
    stagesCount = 0;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::setName(const char* inName) 
{
    name = inName;
    return *this;
}

ShaderStateCreation& ShaderStateCreation::addStage(const char* code, uint32_t codeSize, VkShaderStageFlagBits type) 
{
    stages[stagesCount].code = code;
    stages[stagesCount].codeSize = codeSize;
    stages[stagesCount].type = type;
    ++stagesCount;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::setSPVInput(bool value) 
{
    spvInput = value;
    return *this;
}

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::reset() 
{
    numBindings = 0;
    setIndex = 0;

    return *this;
}

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::addBinding(const Binding& binding) 
{
    bindings[numBindings++] = binding;
    return *this;
}

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::setName(const char* inName) 
{
    name = inName;
    return *this;
}

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::setSetIndex(uint32_t index) 
{
    setIndex = index;
    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::reset() 
{
    numResources = 0;
    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::setLayout(DescriptorSetLayoutHandle newLayout)
{
    layout = newLayout;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::texture(TextureHandle texture, uint16_t binding) 
{
    samplers[numResources] = INVALID_SAMPLER;
    bindings[numResources] = binding;
    resources[numResources++] = texture.index;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::buffer(BufferHandle buffer, uint16_t binding) 
{
    samplers[numResources] = INVALID_SAMPLER;
    bindings[numResources] = binding;
    resources[numResources++] = buffer.index;

    return *this;
}

//TODO: Seperate samplers from textures.
DescriptorSetCreation& DescriptorSetCreation::textureSampler(TextureHandle texture, SamplerHandle sampler, uint16_t binding) 
{
    bindings[numResources] = binding;
    resources[numResources] = texture.index;
    samplers[numResources++] = sampler;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::setName(const char* inName) 
{
    name = inName;
    return *this;
}

VertexInputCreation& VertexInputCreation::reset() 
{
    numVertexAttributes = 0;
    numVertexStreams = 0;

    return *this;
}

VertexInputCreation& VertexInputCreation::addVertexStream(const VertexStream& stream) 
{
    vertexStreams[numVertexStreams++] = stream;
    return *this;
}

VertexInputCreation& VertexInputCreation::addVertexAttribute(const VertexAttribute& attribute) 
{
    vertexAttributes[numVertexAttributes++] = attribute;
    return *this;
}

RenderPassOutput& RenderPassOutput::reset() 
{
    numColourFormats = 0;
    for (uint32_t i = 0; i < MAX_IMAGE_OUTPUT; ++i) 
    {
        colourFormats[i] = VK_FORMAT_UNDEFINED;
    }
    depthStencilFormat = VK_FORMAT_UNDEFINED;
    colourOperations = RenderPassType::Operations::DONT_CARE;
    depthOperations = RenderPassType::Operations::DONT_CARE;
    stencilOperations = RenderPassType::Operations::DONT_CARE;

    return *this;
}

RenderPassOutput& RenderPassOutput::colour(VkFormat format) 
{
    colourFormats[numColourFormats++] = format;
    return *this;
}

RenderPassOutput& RenderPassOutput::depth(VkFormat format) 
{
    depthStencilFormat = format;
    return *this;
}

RenderPassOutput& RenderPassOutput::setOperations(RenderPassType::Operations colour,
                                                    RenderPassType::Operations depth,
                                                    RenderPassType::Operations stencil) 
{
    colourOperations = colour;
    depthOperations = depth;
    stencilOperations = stencil;

    return *this;
}

RenderPassCreation& RenderPassCreation::reset() 
{
    numRenderTargets = 0;
    depthStencilTexture = INVALID_TEXTURE;
    resize = 0;
    scaleX = 1.f;
    scaleY = 1.f;

    colourOperations = RenderPassType::Operations::DONT_CARE;
    depthOperations = RenderPassType::Operations::DONT_CARE;
    stencilOperations = RenderPassType::Operations::DONT_CARE;

    return *this;
}

RenderPassCreation& RenderPassCreation::addRenderTexture(TextureHandle texture) 
{
    outputTextures[numRenderTargets++] = texture;

    return *this;
}

RenderPassCreation& RenderPassCreation::setScaling(float newScaleX, float newScaleY, uint8_t newResize)
{
    scaleX = newScaleX;
    scaleY = newScaleY;
    resize = newResize;

    return *this;
}

RenderPassCreation& RenderPassCreation::setDepthStencilTexture(TextureHandle texture) 
{
    depthStencilTexture = texture;

    return *this;
}

RenderPassCreation& RenderPassCreation::setName(const char* inName) 
{
    name = inName;
    return *this;
}

RenderPassCreation& RenderPassCreation::setType(RenderPassType::Types renderPassType) 
{
    type = renderPassType;
    return *this;
}

RenderPassCreation& RenderPassCreation::setOperations(RenderPassType::Operations colour,
                                                        RenderPassType::Operations depth,
                                                        RenderPassType::Operations stencil) 
{
    colourOperations = colour;
    depthOperations = depth;
    stencilOperations = stencil;

    return *this;
}

PipelineCreation& PipelineCreation::addDescriptorSetLayout(DescriptorSetLayoutHandle handle) 
{
    descriptorSetLayout[numActiveLayouts++] = handle;

    return *this;
}

RenderPassOutput& PipelineCreation::renderPassOutput() 
{
    return renderPass;
}

ExecutionBarrier& ExecutionBarrier::reset() 
{
    numImageBarriers = 0;
    numMemoryBarriers = 0;
    sourcePipelineStage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    destinationPipelineStage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

    return *this;
}
    
ExecutionBarrier& ExecutionBarrier::set(VkPipelineStageFlagBits source, VkPipelineStageFlagBits destination)
{
    sourcePipelineStage = source;
    destinationPipelineStage = destination;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::addImageBarrier(const ImageBarrier& imageBarrier) 
{
    imageBarriers[numImageBarriers++] = imageBarrier;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::addMemoryBarrier(const MemoryBarrierHandle& memoryBarrier)
{
    memoryBarriers[numMemoryBarriers++] = memoryBarrier;

    return *this;
}
