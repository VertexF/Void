#ifndef RENDERER_HDR
#define RENDERER_HDR

#include "GPUDevice.hpp"
#include "GPUResources.hpp"

#include "Foundation/ResourceManager.hpp"

struct Renderer;

struct BufferResource : public Resource 
{
    BufferHandle handle;
    uint32_t poolIndex;
    BufferDescription description;

    static constexpr const char* TYPE = "Void_Buffer_Type";
    static uint64_t TYPE_HASH;
};//BufferResource

struct TextureResource : public Resource 
{
    TextureHandle handle;
    uint32_t poolIndex;
    TextureDescription textureDescription;

    static constexpr const char* TYPE = "Void_Texture_Type";
    static uint64_t TYPE_HASH;
};//TextureResource

struct SamplerResource : public Resource 
{
    SamplerHandle handle;
    uint32_t poolIndex;
    SamplerDescription samplerDescription;

    static constexpr const char* TYPE = "Void_Sampler_Type";
    static uint64_t TYPE_HASH;
};//SamplerResource

struct ResourceCache 
{
    void init(Allocator* allocator);
    void shutdown(Renderer* renderer);

    FlatHashMap<uint64_t, TextureResource*> textures;
    FlatHashMap<uint64_t, BufferResource*> buffers;
    FlatHashMap<uint64_t, SamplerResource*> samplers;
};//ResourceCache

struct RenderCreation 
{
    GPUDevice* gpu;
    Allocator* allocator;
};

//Main struct responsible for handling all high level resources
struct Renderer
{
    Renderer* instance();

    void init(const RenderCreation& creation);
    void shutdown();

    void setLoaders(ResourceManager* manager);
        
    void beginFrame();
    void endFrame();

    void resizeSwapchain(uint32_t newWidth, uint32_t newHeight);

    float aspectRatio() const;

    //Creation/destruction
    BufferResource* createBuffer(const BufferCreation& creation);
    BufferResource* createBuffer(VkBufferUsageFlags type, ResourceType::Type usage, 
                                    uint32_t size, void* data, const char* name);

    TextureResource* createTexture(const TextureCreation& creation);
    TextureResource* createTexture(const char* name, const char* filename);

    SamplerResource* createSampler(const SamplerCreation& creation);

    void destroyBuffer(BufferResource* buffer);
    void destroyTexture(TextureResource* texture);
    void destroySampler(SamplerResource* sampler);

    //Update resource
    void* mapBuffer(BufferResource* buffer, uint32_t offset = 0, uint32_t size = 0);
    void upmapBuffer(BufferResource* buffer);

    CommandBuffer* getCommandBuffer(VkQueueFlagBits type, bool begin);
    void queueCommandBuffer(CommandBuffer* commands);

    ResourcePoolTyped<TextureResource> textures;
    ResourcePoolTyped<BufferResource> buffers;
    ResourcePoolTyped<SamplerResource> samplers;

    ResourceCache resourceCache;

    GPUDevice* gpu;

    uint16_t width;
    uint16_t height;

    static constexpr const char* NAME = "Void_Rendering_Service";
};

#endif // !RENDERER_HDR
