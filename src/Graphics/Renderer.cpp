#include "Renderer.hpp"

#include "CommandBuffer.hpp"
#include "Foundation/Memory.hpp"
#include "Foundation/File.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "vender/stb_image.h"

namespace 
{
    TextureHandle createTextureFromFile(GPUDevice& gpu, const char* filename, const char* name) 
    {
        if (filename) 
        {
            int comp;
            int width;
            int height;

            uint8_t* imageData = stbi_load(filename, &width, &height, &comp, 4);
            if (imageData == nullptr) 
            {
                vprint("Error loading texture %s", filename);
                return INVALID_TEXTURE;
            }

            TextureCreation creation;
            creation.setData(imageData)
                    .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
                    .setFlags(1, 0)
                    .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1)
                    .setName(name);

            TextureHandle newTexture = gpu.createTexture(creation);

            free(imageData);

            return newTexture;
        }

        return INVALID_TEXTURE;
    }
}//Anon

//Resource Loading
struct TextureLoader : public ResourceLoader 
{
    virtual ~TextureLoader() = default;

    virtual Resource* get(const char* name) override;
    virtual Resource* get(uint64_t hashedName) override;

    virtual Resource* unload(const char* name) override;
    virtual Resource* createFromFile(const char* name, const char* filename, ResourceManager* resourceManager) override;

    Renderer* renderer;
};

struct BufferLoader : public ResourceLoader
{
    virtual ~BufferLoader() = default;

    virtual Resource* get(const char* name) override;
    virtual Resource* get(uint64_t hashedName) override;

    virtual Resource* unload(const char* name) override;

    Renderer* renderer;
};

struct SamplerLoader : public ResourceLoader
{
    virtual ~SamplerLoader() = default;

    virtual Resource* get(const char* name) override;
    virtual Resource* get(uint64_t hashedName) override;

    virtual Resource* unload(const char* name) override;

    Renderer* renderer;
};

Resource* TextureLoader::get(const char* name) 
{
    const uint64_t hashedName = hashCalculate(name);
    return renderer->resourceCache.textures.get(hashedName);
}

Resource* TextureLoader::get(uint64_t hashedName) 
{
    return renderer->resourceCache.textures.get(hashedName);
}

Resource* TextureLoader::unload(const char* name) 
{
    const uint64_t hashedName = hashCalculate(name);
    TextureResource* texture = renderer->resourceCache.textures.get(hashedName);
    if (texture) 
    {
        renderer->destroyTexture(texture);
    }

    return nullptr;
}

Resource* TextureLoader::createFromFile(const char* name, const char* filename, ResourceManager* /*resourceManager*/) 
{
    return renderer->createTexture(name, filename);
}

Resource* BufferLoader::get(const char* name) 
{
    const uint64_t hashedName = hashCalculate(name);
    return renderer->resourceCache.buffers.get(hashedName);
}

Resource* BufferLoader::get(uint64_t hashedName) 
{
    return renderer->resourceCache.buffers.get(hashedName);
}

Resource* BufferLoader::unload(const char* name) 
{
    const uint64_t hashedName = hashCalculate(name);
    BufferResource* buffer = renderer->resourceCache.buffers.get(hashedName);
    if (buffer)
    {
        renderer->destroyBuffer(buffer);
    }

    return nullptr;
}

Resource* SamplerLoader::get(const char* name)
{
    const uint64_t hashedName = hashCalculate(name);
    return renderer->resourceCache.samplers.get(hashedName);
}

Resource* SamplerLoader::get(uint64_t hashedName)
{
    return renderer->resourceCache.samplers.get(hashedName);
}

Resource* SamplerLoader::unload(const char* name)
{
    const uint64_t hashedName = hashCalculate(name);
    SamplerResource* sampler = renderer->resourceCache.samplers.get(hashedName);
    if (sampler)
    {
        renderer->destroySampler(sampler);
    }

    return nullptr;
}

uint64_t TextureResource::TYPE_HASH = 0;
uint64_t BufferResource::TYPE_HASH = 0;
uint64_t SamplerResource::TYPE_HASH = 0;

static TextureLoader TEXTURE_LOADER;
static BufferLoader BUFFER_LOADER;
static SamplerLoader SAMPLER_LOADER;

Renderer* Renderer::instance() 
{
    static Renderer renderer;
    return &renderer;
}

void ResourceCache::init(Allocator* allocator) 
{
    //Init resources caching
    textures.init(allocator, 16);
    buffers.init(allocator, 16);
    samplers.init(allocator, 16);
}

void ResourceCache::shutdown(Renderer* renderer) 
{
    FlatHashMapIterator it = textures.iteratorBegin();

    while (it.isValid()) 
    {
        TextureResource* texture = textures.get(it);
        renderer->destroyTexture(texture);

        textures.iteratorAdvance(it);
    }

    it = buffers.iteratorBegin();

    while (it.isValid())
    {
        BufferResource* buffer = buffers.get(it);
        renderer->destroyBuffer(buffer);

        buffers.iteratorAdvance(it);
    }

    it = samplers.iteratorBegin();

    while (it.isValid())
    {
        SamplerResource* resource = samplers.get(it);
        renderer->destroySampler(resource);

        samplers.iteratorAdvance(it);
    }

    textures.shutdown();
    buffers.shutdown();
    samplers.shutdown();
}

void Renderer::init(const RenderCreation& creation) 
{
    vprint("Renderer init.\n");

    gpu = creation.gpu;

    width = gpu->swapchainWidth;
    height = gpu->swapchainHeight;

    textures.init(creation.allocator, 512);
    buffers.init(creation.allocator, 4096);
    samplers.init(creation.allocator, 128);

    resourceCache.init(creation.allocator);

    //Init resource hash
    TextureResource::TYPE_HASH = hashCalculate(TextureResource::TYPE);
    BufferResource::TYPE_HASH = hashCalculate(BufferResource::TYPE);
    SamplerResource::TYPE_HASH = hashCalculate(SamplerResource::TYPE);

    TEXTURE_LOADER.renderer = this;
    BUFFER_LOADER.renderer = this;
    SAMPLER_LOADER.renderer = this;
}

void Renderer::shutdown() 
{
    resourceCache.shutdown(this);

    textures.shutdown();
    buffers.shutdown();
    samplers.shutdown();

    vprint("Renderer shutdown.\n");

    gpu->shutdown();
}

void Renderer::setLoaders(ResourceManager* manager) 
{
    manager->setLoader(TextureResource::TYPE, &TEXTURE_LOADER);
    manager->setLoader(BufferResource::TYPE, &BUFFER_LOADER);
    manager->setLoader(SamplerResource::TYPE, &SAMPLER_LOADER);
}

void Renderer::beginFrame() 
{
    gpu->newFrame();
}

void Renderer::endFrame() 
{
    gpu->present();
}

void Renderer::resizeSwapchain(uint32_t newWidth, uint32_t newHeight)
{
    gpu->resize(static_cast<uint16_t>(newWidth), static_cast<uint16_t>(newHeight));

    width = gpu->swapchainWidth;
    height = gpu->swapchainHeight;
}

float Renderer::aspectRatio() const 
{
    return gpu->swapchainWidth * 1.f / gpu->swapchainHeight;
}

//Creation/destruction
BufferResource* Renderer::createBuffer(const BufferCreation& creation) 
{
    BufferResource* buffer = buffers.obtain();
    if (buffer) 
    {
        BufferHandle handle = gpu->createBuffer(creation);
        buffer->handle = handle;
        buffer->name = creation.name;
        gpu->queryBuffer(handle, buffer->description);

        if (creation.name != nullptr) 
        {
            resourceCache.buffers.insert(hashCalculate(creation.name), buffer);
        }

        buffer->references = 1;

        return buffer;
    }

    return nullptr;
}

BufferResource* Renderer::createBuffer(VkBufferUsageFlags type, ResourceType::Type usage,
                                        uint32_t size, void* data, const char* name) 
{
    BufferCreation creation = { type, usage, size, data, name };
    return createBuffer(creation);
}

TextureResource* Renderer::createTexture(const TextureCreation& creation) 
{
    TextureResource* texture = textures.obtain();

    if (texture) 
    {
        TextureHandle handle = gpu->createTexture(creation);
        texture->handle = handle;
        texture->name = creation.name;
        gpu->queryTexture(handle, texture->textureDescription);

        if (creation.name != nullptr) 
        {
            resourceCache.textures.insert(hashCalculate(creation.name), texture);
        }

        texture->references = 1;

        return texture;
    }
    return nullptr;
}

TextureResource* Renderer::createTexture(const char* name, const char* filename) 
{
    TextureResource* texture = textures.obtain();

    if (texture) 
    {
        TextureHandle handle = createTextureFromFile(*gpu, filename, name);
        texture->handle = handle;
        gpu->queryTexture(handle, texture->textureDescription);
        texture->references = 1;
        texture->name = name;

        resourceCache.textures.insert(hashCalculate(name), texture);

        return texture;
    }
    return nullptr;
}

SamplerResource* Renderer::createSampler(const SamplerCreation& creation) 
{
    SamplerResource* sampler = samplers.obtain();
    if (sampler) 
    {
        SamplerHandle handle = gpu->createSampler(creation);
        sampler->handle = handle;
        sampler->name = creation.name;
        gpu->querySampler(handle, sampler->samplerDescription);

        if (creation.name != nullptr)
        {
            resourceCache.samplers.insert(hashCalculate(creation.name), sampler);
        }

        sampler->references = 1;

        return sampler;
    }
    return nullptr;
}

void Renderer::destroyBuffer(BufferResource* buffer) 
{
    if (buffer == nullptr) 
    {
        return;
    }

    buffer->removeReference();
    if (buffer->references) 
    {
        return;
    }

    resourceCache.buffers.remove(hashCalculate(buffer->description.name));
    gpu->destroyBuffer(buffer->handle);
    buffers.release(buffer);
}

void Renderer::destroyTexture(TextureResource* texture) 
{
    if (texture == nullptr)
    {
        return;
    }

    texture->removeReference();
    if (texture->references)
    {
        return;
    }

    resourceCache.textures.remove(hashCalculate(texture->textureDescription.name));
    gpu->destroyTexture(texture->handle);
    textures.release(texture);
}

void Renderer::destroySampler(SamplerResource* sampler) 
{
    if (sampler == nullptr)
    {
        return;
    }

    sampler->removeReference();
    if (sampler->references)
    {
        return;
    }

    resourceCache.samplers.remove(hashCalculate(sampler->samplerDescription.name));
    gpu->destroySampler(sampler->handle);
    samplers.release(sampler);
}

//Update resource
void* Renderer::mapBuffer(BufferResource* buffer, uint32_t offset/* = 0*/, uint32_t size/* = 0*/) 
{
    MapBufferParameters cbMap = { buffer->handle, offset, size };
    return gpu->mapBuffer(cbMap);
}

void Renderer::upmapBuffer(BufferResource* buffer) 
{
    if (buffer->description.parentHandle.index == INVALID_INDEX) 
    {
        MapBufferParameters cbMap = { buffer->handle, 0, 0 };
        gpu->unmapBuffer(cbMap);
    }
}

CommandBuffer* Renderer::getCommandBuffer(VkQueueFlagBits type, bool begin)
{
    return gpu->getCommandBuffer(type, begin);
}

void Renderer::queueCommandBuffer(CommandBuffer* commands)
{
    gpu->queueCommandBuffer(commands);
}