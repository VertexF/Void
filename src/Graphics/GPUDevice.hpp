#ifndef GPU_DEVICE_HDR
#define GPU_DEVICE_HDR

#include <vulkan/vulkan.h>
#include "vender/vk_mem_alloc.h"

#include "Application/GameCamera.hpp"

#include "Foundation/Platform.hpp"

#include "Graphics/GPUResources.hpp"

#include "Foundation/ResourcePool.hpp"
#include "Foundation/String.hpp"
#include "Foundation/Array.hpp"

#include <vector>

struct Allocator;
struct CommandBuffer;
struct DeviceRenderFrame;
struct GPUTimestampManager;
struct GPUDevice;

struct GPUTimestamp
{
    uint32_t start;
    uint32_t end;

    double elapsedMS;

    uint16_t parentIndex;
    uint16_t depth;

    uint32_t colour;
    uint32_t frameIndex;

    const char* name;
};

struct GPUTimestampManager
{
    void init(Allocator* newAllocator, uint16_t newQueriesPerFrame, uint16_t maxFrame);
    void shutdown();

    bool hasValidQueries() const;
    void reset();
    //Returns the total queries for this frame.
    uint32_t resolve(uint32_t currentFrame, GPUTimestamp* timestampsToFill);

    //Returns the timestamp query index.
    uint32_t push(uint32_t currentFrame, const char* name);
    uint32_t pop(uint32_t currentFrame);

    Allocator* allocator = nullptr;
    GPUTimestamp* timestamps = nullptr;
    uint64_t* timestampsData = nullptr;

    uint32_t queriesPerFrame = 0;
    uint32_t currentQuery = 0;
    uint32_t parentIndex = 0;
    uint32_t depth = 0;

    //Used to query the GPU only once per frame if getGPUTimestamps is called more than once per frame.
    bool currentFrameResolved = false;
};

struct DeviceCreation
{
    Allocator* allocator = nullptr;
    StackAllocator* tempAllocator = nullptr;
    //Pointer ot API-specific window: SDL_Window, GLFWWindow
    void* window = nullptr;
    uint16_t width = 1;
    uint16_t height = 1;

    uint16_t GPUTimeQueriesPerFrame = 32;
    bool enableGPUTimeQueries = false;
    bool debug = false;

    DeviceCreation& setWindow(uint32_t newWidth, uint32_t newHeight, void* handle);
    DeviceCreation& setAllocator(Allocator* newAllocator);
    DeviceCreation& setLinearAllocator(StackAllocator* alloc);
};

struct GPUDevice
{
    virtual ~GPUDevice() = default;
    static GPUDevice instance();

    //Init/shutdown
    virtual void init(const DeviceCreation& creation);
    virtual void shutdown();

    //Creation/Destruction of resources
    BufferHandle createBuffer(const BufferCreation& creation);
    TextureHandle createTexture(const TextureCreation& creation);
    PipelineHandle createPipeline(const PipelineCreation& creation);
    PipelineHandle createPipeline(const PipelineCreation& creation, DescriptorSetLayoutHandle one, DescriptorSetLayoutHandle two);
    SamplerHandle createSampler(const SamplerCreation& creation);
    DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorSetLayoutCreation& creation);
    DescriptorSetHandle createDescriptorSet(const DescriptorSetCreation& creation);
    RenderPassHandle createRenderPass(const RenderPassCreation& creation);
    ShaderStateHandle createShaderState(const ShaderStateCreation& creation);

    void destroyBuffer(BufferHandle buffer);
    void destroyTexture(TextureHandle texture);
    void destroyPipeline(PipelineHandle pipeline);
    void destroySampler(SamplerHandle sampler);
    void destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout);
    void destroyDescriptorSet(DescriptorSetHandle layout);
    void destroyRenderPass(RenderPassHandle renderPass);
    void destroyShaderState(ShaderStateHandle shader);

    //Query description
    void queryBuffer(BufferHandle buffer, BufferDescription& outDescriptor);
    void queryTexture(TextureHandle texture, TextureDescription& outDescriptor);
    void queryPipeline(PipelineHandle pipeline, PipelineDescription& outDescriptor);
    void querySampler(SamplerHandle sampler, SamplerDescription& outDescriptor);
    void queryDescriptorSetLayout(DescriptorSetLayoutHandle layout, DescriptorSetLayoutDescription& outDescriptor);
    void queryDescriptorSet(DescriptorSetHandle set, DescriptorSetDescription& outDescriptor);
    void queryShaderState(ShaderStateHandle shader, ShaderStateDescription& outDescriptor);

    const RenderPassOutput& getRenderPassOutput(RenderPassHandle renderPass) const;

    //Update/Reload resources
    void resizeOutputTextures(RenderPassHandle renderPass, uint32_t width, uint32_t height);
    void updateDescriptorSet(DescriptorSetHandle set);

    //Misc
    //TODO: For now specify a sampler for a texture or use the default one.
    void linkTextureSampler(TextureHandle texture, SamplerHandle sampler);
    void setPresentMode(PresentMode::Types type);
    void frameCountersAdvanced();
    bool getFamilyQueue(VkPhysicalDevice physicalDevice);

    //Swapchain
    void createSwapchain();
    void destroySwapchain();
    void resizeSwapchain();

    //Map/Unmap
    void* mapBuffer(const MapBufferParameters& parameters);
    void  unmapBuffer(const MapBufferParameters& parameters);

    void* dynamicAllocate(uint32_t size);

    void setBufferGlobalOffset(BufferHandle buffer, uint32_t offset);

    //Command buffers
    CommandBuffer* getCommandBuffer(VkQueueFlagBits type, bool begin);
    CommandBuffer* getInstantCommandBuffer();

    void queueCommandBuffer(CommandBuffer* commandBuffer);

    //Rendering
    void newFrame();
    void present();
    void resize(uint16_t width, uint16_t height);

    void fillBarrier(RenderPassHandle renderPass, ExecutionBarrier& outBarrier);

    //Returns a vertex buffer usable for fullscreen that uses no vertices.
    BufferHandle getFullscreenVertexBuffer() const;
    //Returns what is considered the final pass that writes to the swapchain.
    RenderPassHandle getSwapchainPass() const;

    TextureHandle getDummyTexture() const;
    BufferHandle getDummyConstantBuffer() const;
    const RenderPassOutput& getSwapchainOutput() const;

    VkRenderPass getVulkanRenderPass(const RenderPassOutput& output, const char* name);

    //Names and markers
    void setResourceName(VkObjectType objectType, uint64_t handle, const char* name);
    void pushMarker(VkCommandBuffer commandBuffer, const char* name);
    void popMarker(VkCommandBuffer commandBuffer);

    //GPU timings
    void setGPUTimestampsEnable(bool flag);

    uint32_t getGPUTimestamps(GPUTimestamp* outTimestamps);
    void pushGPUTimestamp(CommandBuffer* commandBuffer, const char* name);
    void popGPUTimestamp(CommandBuffer* commandBuffer);

    //Instant functions
    void destroyBufferInstant(uint32_t buffer);
    void destroyTextureInstant(uint32_t texture);
    void destroyPipelineInstant(uint32_t pipeline);
    void destroySamplerInstant(uint32_t sampler);
    void destroyDescriptorSetLayoutInstant(uint32_t layout);
    void destroyDescriptorSetInstant(uint32_t set);
    void destroyRenderPassInstant(uint32_t renderPass);
    void destroyShaderStateInstant(uint32_t shader);

    void updateDescriptorSetInstant(const DescriptorSetUpdate& update);

    //Accesses
    ShaderState* accessShaderState(ShaderStateHandle shader);
    const ShaderState* accessShaderState(ShaderStateHandle shader) const;

    Texture* accessTexture(TextureHandle texture);
    const Texture* accessTexture(TextureHandle texture) const;

    Buffer* accessBuffer(BufferHandle buffer);
    const Buffer* accessBuffer(BufferHandle buffer) const;

    Pipeline* accessPipeline(PipelineHandle pipeline);
    const Pipeline* accessPipeline(PipelineHandle pipeline) const;

    Sampler* accessSampler(SamplerHandle sampler);
    const Sampler* accessSampler(SamplerHandle sampler) const;

    DescriptorSetLayout* accessDescriptorSetLayout(DescriptorSetLayoutHandle layout);
    const DescriptorSetLayout* accessDescriptorSetLayout(DescriptorSetLayoutHandle layout) const;

    DescriptorSet* accessDescriptorSet(DescriptorSetHandle set);
    const DescriptorSet* accessDescriptorSet(DescriptorSetHandle set) const;

    RenderPass* accessRenderPass(RenderPassHandle renderPass);
    const RenderPass* accessRenderPass(RenderPassHandle renderPass) const;

    ResourcePool buffers;
    ResourcePool textures;
    ResourcePool pipelines;
    ResourcePool samplers;
    ResourcePool descriptorSetLayouts;
    ResourcePool descriptorSets;
    ResourcePool renderPasses;
    ResourcePool commandBuffers;
    ResourcePool shaders;

    //Primitive resources
    BufferHandle fullscreenVertexBuffer;
    RenderPassHandle swapchainPass;
    SamplerHandle defaultSampler;

    //Dummy resources
    TextureHandle dummyTexture;
    BufferHandle dummyConstantBuffer;

    RenderPassOutput swapchainOutput;

    StringBuffer stringBuffer;

    Allocator* allocator;
    StackAllocator* tempAllocator;

    uint32_t dynamicMaxPerFrameSize;
    BufferHandle dynamicBuffer;
    uint8_t* dynamicMappedMemory;
    uint32_t dynamicAllocatedSize;
    uint32_t dynamicPerFrameSize;

    CommandBuffer** queuedCommandBuffers = nullptr;
    uint32_t numAllocatedCommandBuffers = 0;
    uint32_t numQueuedCommandBuffers = 0;

    PresentMode::Types presentMode = PresentMode::Types::VSYNC;
    uint32_t currentFrame;
    uint32_t previousFrame;

    uint32_t absoluteFrame;

    uint16_t swapchainWidth = 1;
    uint16_t swapchainHeight = 1;

    GPUTimestampManager* gpuTimestampManager = nullptr;

    bool bindlessSupported = false;
    bool timestampsEnabled = false;
    bool resized = false;
    bool verticalSync = false;

    static constexpr const char* NAME = "AIR_GPU_SERVICE";

    VkAllocationCallbacks* vulkanAllocationCallbacks;
    VkInstance vulkanInstance;
    VkPhysicalDevice vulkanPhysicalDevice;
    VkPhysicalDeviceProperties vulkanPhysicalProperties;
    VkDevice vulkanDevice;
    VkQueue vulkanQueue;
    uint32_t vulkanQueueFamily;
    VkDescriptorPool vulkanDescriptorPool;

    //Swapchain
    Array<VkImage> vulkanSwapchainImages;
    Array<VkImageView> vulkanSwapchainImageViews;
    Array<VkFramebuffer> vulkanSwapchainFramebuffers;

    VkQueryPool vulkanTimestampQueryPool;
    //Per frame synchronisation
    //Array<VkSemaphore> imageAvailableSemaphore;
    //Array<VkSemaphore> renderFinishSemaphore;
    //Array<VkFence> framesInFlight;
    Array<VkSemaphore> imageAvailableSemaphore;
    Array<VkSemaphore> renderFinishSemaphore;
    Array<VkFence> framesInFlight;

    TextureHandle depthTexture;

    //Window specific
    VkSurfaceKHR vulkanWindowSurface;
    VkSurfaceFormatKHR vulkanSurfaceFormat;
    VkPresentModeKHR vulkanPresentMode;
    VkSwapchainKHR vulkanSwapchain;
    uint32_t swapchainImageCount;

    VkDebugReportCallbackEXT vulkanDebugCallback;
    VkDebugUtilsMessengerEXT vulkanDebugUtilsMessenger;

    uint32_t vulkanImageIndex;

    VmaAllocator VMAAllocator;

    //These are dynamic - so that workload can handled correctly.
    Array<ResourceUpdate> resourceDeletionQueue;
    Array<DescriptorSetUpdate> descriptorSetUpdates;

    float gpuTimestampFrequency;
    bool gpuTimestampReset = true;
    bool debugUtilsExtensionPresent = false;

    char vulkanBinariesPath[512];
};

#endif // !GPU_DEVICE_HDR
