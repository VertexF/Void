#include <stdio.h>
#include <assert.h>
#include <vector>
#include <math.h>
#include <array>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct/vec2.h>
#include <cglm/struct/vec3.h>
#include <cglm/struct/affine.h>
#include <cglm/struct/cam.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define SHADER_ASSETS "Assets/Shaders/"
#define SKELETON_DEBUG
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

namespace
{
    SDL_Window* window;
    VkInstance instance;

    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    std::vector<VkImage> swapchainImages;

    VkDevice device;
    VkSwapchainKHR swapchain;

    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;

    uint32_t maxAnisotropy;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;

    VkFormat swapchainFormat;
    VkRenderPass renderPass;
    VkExtent2D swapchainExtent;

    uint32_t imageCount;

    //Setting this to the max value of a 32uint because it's possible that the first family index that is found is 0, 
    //UINT_MAX should error out some where else and be obvious in code what happened.
    uint32_t mainQueueFamilyIndex = UINT32_MAX;
    uint32_t transferQueueFamilyIndex = UINT32_MAX;

    VkCommandPool commandPool;
    VkCommandPool transferCommandPool;
    VkQueue mainQueue;
    VkQueue transferQueue;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    struct Vertex 
    {
        vec3s position;
        vec3s colour;
        vec2s texCoord;

        static VkVertexInputBindingDescription getBindingDescriptions() 
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() 
        {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, colour);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            return attributeDescriptions;
        }
    };

    struct ModelData
    {
        alignas(16) mat4s model;
        alignas(16) mat4s view;
        alignas(16) mat4s proj;
    };

    const std::vector<Vertex> vertices = 
    {
        {{ -0.5f, -0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f }},
        {{  0.5f, -0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f }},
        {{  0.5f,  0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 0.f, 1.f }},
        {{ -0.5f,  0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f }},

        {{ -0.5f, -0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f }},
        {{  0.5f, -0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f }},
        {{  0.5f,  0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 0.f, 1.f }},
        {{ -0.5f,  0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f }}
    };

    const std::vector<uint16_t> indices = 
    {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };
}

static uint32_t clamp(uint32_t value, uint32_t minimum, uint32_t maximum) 
{
    return MAX(minimum, MIN(value, maximum));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                     void* userData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        const char* type = (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR"
            : (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) || (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) ?
            "WARNING" : "INFO";

        static constexpr uint32_t bufferSize = 4096;
        char message[bufferSize];
        snprintf(message, bufferSize, "%s : %s\n", type, callbackData->pMessage);

        printf("%s", message);
#ifdef _WIN32
        OutputDebugStringA(message);
#endif // _WIN32

        __debugbreak();
    }

    return VK_FALSE;
}

static long getFileSize(FILE* file)
{
    long fileSizeSigned;

    fseek(file, 0, SEEK_END);
    fileSizeSigned = ftell(file);
    fseek(file, 0, SEEK_SET);

    return fileSizeSigned;
}

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) 
{
    // Find memory type
    uint32_t memoryType = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    if (memoryType == UINT32_MAX)
    {
        printf("Failed to find suitable memory type.");
        assert(false);
    }
}

static VkCommandPool createCommandPool(uint32_t queueFamilyIndex) 
{
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool pool;
    if (vkCreateCommandPool(device, &poolCreateInfo, nullptr, &pool))
    {
        assert(false);
        printf("Failed to create a command pool.");
    }

    return pool;
}

static VkCommandBuffer beginOneTimeCommandBuffer(const VkCommandPool& pool) 
{
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandPool = pool;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void endOneTimeCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandPool& cmdPool, const VkQueue& queue)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
}

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) 
{
    //Vertex buffer creation.
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        printf("Failed to create vertex buffer.");
        assert(false);
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    VkMemoryAllocateInfo bufferAllocationInfo{};
    bufferAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferAllocationInfo.allocationSize = memoryRequirements.size;
    bufferAllocationInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &bufferAllocationInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        printf("Failed to allocate the vertex buffer object");
        assert(false);
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
{
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer(transferCommandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endOneTimeCommandBuffer(commandBuffer, transferCommandPool, transferQueue);
}

static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) 
{
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer(transferCommandPool);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endOneTimeCommandBuffer(commandBuffer, transferCommandPool, transferQueue);
}

static bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer(commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencilComponent(format))
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else 
    {
        printf("Not support memory image layout transition");
        assert(false);
    }
    
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endOneTimeCommandBuffer(commandBuffer, commandPool, mainQueue);
}

static void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        printf("Failed to create image.");
        assert(false);
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        printf("Failed to allocate image memory");
        assert(false);
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

//"Assets/Textures/frobert.png"
static void createTextureImage(const char* path, VkImage& textureImage, VkDeviceMemory& textureImageMemory)
{
    int texWidth;
    int texHeight;
    int channles;

    //Here load the raw pixel data to an unsigned char* 
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &channles, STBI_rgb_alpha);
    //We also need the device size for the staging buffers.
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    //Next we check if it's loaded correctly
    if (!pixels)
    {
        printf("Failed to load texture image.");
        assert(false);
    }
    //Then we start with the staging buffers.
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;

    //We are making the staging buffer for copying to an image so we use VK_BUFFER_USAGE_TRANSFER_SRC_BIT to transfer
    //and it needs to he host visible so we can load data in.
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, imageStagingBuffer, imageStagingBufferMemory);

    //Next we copy the data to the staging buffer using the imageSize we calculated eariler when we loaded the image from disk.
    void* imageData;
    vkMapMemory(device, imageStagingBufferMemory, 0, imageSize, 0, &imageData);
    memcpy(imageData, pixels, size_t(imageSize));
    vkUnmapMemory(device, imageStagingBufferMemory);

    //After that we free the pixels as they are in the staging buffer.
    stbi_image_free(pixels);

    //Next we create the image we are going to copy the buffer into. 
    //We aren't doing anything special but it needs to be created with VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT as it will live on the GPU
    //and be used to sampled on VK_IMAGE_USAGE_SAMPLED_BIT and it needs to be the destination 
    //for our copy so we need the VK_IMAGE_USAGE_TRANSFER_DST_BIT to be ORed with it.
    createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    //The we transition image ready to receive data.  So the image needs to be have the same format and type as the usage type.
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, 
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    //Next we copy the entire image from the buffer to the image.
    copyBufferToImage(imageStagingBuffer, textureImage, uint32_t(texWidth), uint32_t(texHeight));
    //After that we transition the image memory layout to be read by the shader. 
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, 
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //Finally we can destroy the staging buffers.
    vkDestroyBuffer(device, imageStagingBuffer, nullptr);
    vkFreeMemory(device, imageStagingBufferMemory, nullptr);
}

static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) 
{
    VkImageViewCreateInfo createImageViewInfo{};
    createImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createImageViewInfo.image = image;
    createImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createImageViewInfo.format = format;
    createImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createImageViewInfo.subresourceRange.aspectMask = aspectFlags;
    createImageViewInfo.subresourceRange.baseMipLevel = 0;
    createImageViewInfo.subresourceRange.levelCount = 1;
    createImageViewInfo.subresourceRange.baseArrayLayer = 0;
    createImageViewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &createImageViewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        printf("Failed to create texture image view");
        assert(false);
    }

    return imageView;
}

static VkImageView createTextureImageView(const VkImage& image, VkFormat format) 
{
    return createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

static VkSampler createSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = maxAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.f;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = 0.f;

    VkSampler sampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) 
    {
        printf("Could not create sampler");
        assert(false);
    }
    return sampler;
}

static void createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(ModelData);

    uniformBuffers.resize(imageCount);
    uniformBuffersMemory.resize(imageCount);
    uniformBuffersMapped.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

static VkFormat findSupportedFormat(const std::vector<VkFormat>& candidtes, VkImageTiling tiling, VkFormatFeatureFlags features) 
{
    for (VkFormat format : candidtes) 
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) 
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    assert(!"No supported format based on the feature enum %d", features);
}

static VkFormat findDepthFormat() 
{
    return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static void createDepthResources()
{
    VkFormat depthFormat = findDepthFormat();

    createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

//Read file and allocate memory from. User is responsible for freeing the memory.
static char* fileReadBinary(const char* filename, size_t* size = nullptr)
{
    char* outData = 0;

    FILE* file = fopen(filename, "rb");
    if (file)
    {
        size_t filesize = getFileSize(file);

        outData = (char*)malloc(filesize + 1);

        if (outData != nullptr) 
        {
            fread(outData, filesize, 1, file);
            outData[filesize] = 0;
        }
        else 
        {
            printf("Couldn't allocate memory for file %s", filename);
        }

        if (size)
        {
            *size = filesize;
        }

        fclose(file);
    }
    else 
    {
        printf("Couldn't open file %s", filename);
    }

    return outData;
}

static void cleanupSwapchain() 
{
    for (auto framebuffer : swapchainFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapchainImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

static void createSwapchain() 
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    if (formatCount != 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());
    }

    VkSurfaceFormatKHR swapchainSurface{};
    for (const auto& availableSurface : surfaceFormats)
    {
        if (availableSurface.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            swapchainSurface = availableSurface;
        }
    }
    swapchainSurface = surfaceFormats[0];

    uint32_t presentationModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentationModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentationModes(presentationModeCount);
    if (presentationModeCount != 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentationModeCount, presentationModes.data());
    }

    VkPresentModeKHR presentationMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
    for (uint32_t i = 0; i < presentationModes.size(); ++i)
    {
        if (presentationModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentationMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }

        presentationMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    if (swapchainExtent.width != UINT32_MAX)
    {
        swapchainExtent.width = clamp(swapchainExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        swapchainExtent.height = clamp(swapchainExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    imageCount = surfaceCapabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR createSwapchain{};
    createSwapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createSwapchain.surface = surface;
    createSwapchain.minImageCount = imageCount;
    createSwapchain.imageFormat = swapchainSurface.format;
    createSwapchain.imageColorSpace = swapchainSurface.colorSpace;
    createSwapchain.imageExtent = swapchainExtent;
    createSwapchain.imageArrayLayers = 1;
    createSwapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    //We always assume the presentation queue family and the graphics queue family are the same. 
    createSwapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createSwapchain.queueFamilyIndexCount = 0;
    createSwapchain.pQueueFamilyIndices = nullptr;
    createSwapchain.preTransform = surfaceCapabilities.currentTransform;
    createSwapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createSwapchain.presentMode = presentationMode;
    createSwapchain.clipped = VK_TRUE;

    vkCreateSwapchainKHR(device, &createSwapchain, nullptr, &swapchain);

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainFormat = swapchainSurface.format;
}

static void createImageViews() 
{
    swapchainImageViews.resize(swapchainImages.size());

    for (uint32_t i = 0; i < swapchainImages.size(); ++i)
    {
        swapchainImageViews[i] = createImageView(swapchainImages[i], swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

static void createFramebuffers() 
{
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (uint32_t i = 0; i < swapchainImageViews.size(); ++i)
    {
        std::array<VkImageView, 2> attachments =
        {
            swapchainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = uint32_t(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
        {
            assert(false);
            printf("Failed to create framebuffer %i", i);
        }
    }
}

static void recreateSwapchain()
{
    vkDeviceWaitIdle(device);

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    swapchainExtent = surfaceCapabilities.currentExtent;

    if(swapchainExtent.width == 0 || swapchainExtent.height == 0) 
    {
        return;
    }

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
}

int main()
{
    const char* REQUESTED_EXTENSIONS[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#if defined(SKELETON_DEBUG)
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#else
#endif //SKELETON_DEBUG
    };

    const char* REQUESTED_LAYERS[] =
    {
#if defined(SKELETON_DEBUG)
        "VK_LAYER_KHRONOS_validation"
#else
        ""
#endif
    };

    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        assert(false);
        return 1;
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow("Skeleton", 1080, 720, flags);
    if (!window)
    {
        printf("Failed to create window.\n");
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);

#ifdef SKELETON_DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugUtilInfo = {};
    debugUtilInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilInfo.pNext = nullptr;
    debugUtilInfo.pfnUserCallback = debugCallback;
    debugUtilInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    debugUtilInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugUtilInfo.pUserData = nullptr;
#endif // SKELETON_DEBUG

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Skeleton";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
    appInfo.pEngineName = "Skeleton";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
#ifdef SKELETON_DEBUG
    instanceInfo.pNext = &debugUtilInfo;
    instanceInfo.enabledLayerCount = sizeof(REQUESTED_LAYERS) / sizeof(REQUESTED_LAYERS[0]);
    instanceInfo.ppEnabledLayerNames = REQUESTED_LAYERS;
#endif // SKELETON_DEBUG
    instanceInfo.enabledExtensionCount = sizeof(REQUESTED_EXTENSIONS) / sizeof(REQUESTED_EXTENSIONS[0]);
    instanceInfo.ppEnabledExtensionNames = REQUESTED_EXTENSIONS;

    vkCreateInstance(&instanceInfo, nullptr, &instance);

    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == false) 
    {
        assert(false);
        printf("Failed to create SDL/Vulkan Surface.\n");
    }

#ifdef SKELETON_DEBUG
    VkDebugUtilsMessengerEXT vulkanDebugUtilsMessenger;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    vkCreateDebugUtilsMessengerEXT(instance, &debugUtilInfo, nullptr, &vulkanDebugUtilsMessenger);
#endif // SKELETON_DEBUG

    //Selecting physical device
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, gpus.data());

    uint32_t highestScore = 0;
    uint32_t bestGPUIndex = 0;
    for (uint32_t i = 0; i < gpus.size(); ++i)
    {
        //Here we are checking the properties of the GPUs and listing them out. 
        //The score is a number which tracks the quality of the GPU we are selecting.
        //NOTE: we are not checking features because the 1070 and the 4070 have the same base features excluding extensions.
        uint32_t score = 0;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(gpus[i], &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 1000;
        }

        score += properties.limits.maxImageDimension2D;
        //Between the 1070 and the 4070 properties the maxDescriptorSetUniformBuffers is one of the few properties that's different.
        score += properties.limits.maxDescriptorSetUniformBuffers;

        if (highestScore < score) 
        {
            highestScore = score;
            bestGPUIndex = i;
            maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        }
        score = 0;
    }

    physicalDevice = gpus[bestGPUIndex];

    //Getting physical device level extensions out of the physical device.
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::vector<const char*> usableDeviceExtensions;
    
    //This is here to check we support demoting the fragment shader to a "helper". When discard is used a new optimisation in 
    //the newest SPRIV compilers to helps with the `discard` key word in the fragment shader. 
    bool demoteShaderToHelperInvocation = false;

    //Assuming you've picked the best GPU we assume that we have the most available.
    //NOTE: I'm looping through all the available extensions here even though we only care about the swapchain one, because we likely want more soon.
    for (uint32_t i = 0; i < availableExtensions.size(); ++i) 
    {
        if (strcmp(availableExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            usableDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            continue;
        }

        if (strcmp(availableExtensions[i].extensionName, VK_KHR_8BIT_STORAGE_EXTENSION_NAME) == 0)
        {
            usableDeviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
            continue;
        }

        if (strcmp(availableExtensions[i].extensionName, VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME) == 0)
        {
            demoteShaderToHelperInvocation = true;
            usableDeviceExtensions.push_back(VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME);
            continue;
        }
    }

    //TODO: This check is bad, there might be GPU that has some random extension and NOT the VK_KHR_SWAPCHAIN_EXTENSION_NAME fix it later.
    if (usableDeviceExtensions.empty())
    {
        assert(false);
        printf("You need at least the %s device level extension working the GPU.\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    //Looking for device features that are aviable.
    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.pNext = nullptr;
    void* currentPNext = &indexingFeatures;

    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &indexingFeatures;
    if (demoteShaderToHelperInvocation)
    {
        vulkan13Features.shaderDemoteToHelperInvocation = VK_TRUE;
        currentPNext = &vulkan13Features;
    }

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures.pNext = currentPNext;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

    //Setting up querying family index
    uint32_t familyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(familyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyPropertyCount, queueFamilyProperties.data());

    VkBool32 surfaceSupported = VK_FALSE;
    for (uint32_t i = 0; i < familyPropertyCount; ++i) 
    {
        if (queueFamilyProperties.size() == 0) 
        {
            continue;
        }

        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &surfaceSupported);
            if (surfaceSupported)
            {
                mainQueueFamilyIndex = i;
                continue;
            }
        }

        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            transferQueueFamilyIndex = i;
            continue;
        }

        if (mainQueueFamilyIndex != UINT32_MAX && transferQueueFamilyIndex != UINT32_MAX)
        {
            break;
        }
    }

    //Logical device and queue creation 
    const float priority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = mainQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priority;

    if (transferQueueFamilyIndex != UINT32_MAX)
    {
        VkDeviceQueueCreateInfo queueTransferCreateInfo{};
        queueTransferCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueTransferCreateInfo.queueFamilyIndex = transferQueueFamilyIndex;
        queueTransferCreateInfo.queueCount = 1;
        queueTransferCreateInfo.pQueuePriorities = &priority;

        VkDeviceQueueCreateInfo localDeviceQueues[] = { queueCreateInfo, queueTransferCreateInfo };

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures;
        deviceCreateInfo.ppEnabledExtensionNames = usableDeviceExtensions.data();
        deviceCreateInfo.enabledExtensionCount = uint32_t(usableDeviceExtensions.size());
        deviceCreateInfo.pQueueCreateInfos = localDeviceQueues;
        deviceCreateInfo.queueCreateInfoCount = sizeof(localDeviceQueues) / sizeof(localDeviceQueues[0]);

        vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    }
    else 
    {
        VkDeviceQueueCreateInfo localDeviceQueues[] = { queueCreateInfo };

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures;
        deviceCreateInfo.ppEnabledExtensionNames = usableDeviceExtensions.data();
        deviceCreateInfo.enabledExtensionCount = uint32_t(usableDeviceExtensions.size());
        deviceCreateInfo.pQueueCreateInfos = localDeviceQueues;
        deviceCreateInfo.queueCreateInfoCount = sizeof(localDeviceQueues) / sizeof(localDeviceQueues[0]);

        //HACK
        transferQueueFamilyIndex = mainQueueFamilyIndex;

        vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    }

    vkGetDeviceQueue(device, mainQueueFamilyIndex, 0, &mainQueue);
    vkGetDeviceQueue(device, transferQueueFamilyIndex, 0, &transferQueue);

    //Swapchain set up.
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    swapchainExtent = surfaceCapabilities.currentExtent;
    createSwapchain();
    createImageViews();

    //Subpass dependencies, subpass attachment references and render pass attachments set up.
    VkAttachmentDescription colourAttachment{};
    colourAttachment.format = swapchainFormat;
    colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colourAttachmentRef{};
    colourAttachmentRef.attachment = 0;
    colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //Descriptor set layout creation
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding sampleLayoutBinding{};
    sampleLayoutBinding.binding = 1;
    sampleLayoutBinding.descriptorCount = 1;
    sampleLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampleLayoutBinding.pImmutableSamplers = nullptr;
    sampleLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    //Pipeline layout creation
    std::array<VkDescriptorSetLayoutBinding, 2> binding = { uboLayoutBinding, sampleLayoutBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = uint32_t(binding.size());
    layoutInfo.pBindings = binding.data();

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        printf("Failed to create descriptor set layout");
        assert(false);
    }

    //Shader modules creation
    size_t sizeOfVertexCode = 0;
    size_t sizeOfFragmentCode = 0;
    char* binVertCode = fileReadBinary(SHADER_ASSETS"mainShader.vert.spv", &sizeOfVertexCode);
    char* binFragCode;
    if (demoteShaderToHelperInvocation)
    {
        binFragCode = fileReadBinary(SHADER_ASSETS"mainShader.frag.spv", &sizeOfFragmentCode);
    }
    else 
    {
        binFragCode = fileReadBinary(SHADER_ASSETS"mainShaderFallback.frag.spv", &sizeOfFragmentCode);
    }
    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;

    VkShaderModuleCreateInfo vertexCreateInfo{};
    vertexCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexCreateInfo.codeSize = sizeOfVertexCode;
    vertexCreateInfo.pCode = reinterpret_cast<const uint32_t*>(binVertCode);

    VkShaderModuleCreateInfo fragCreateInfo{};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = sizeOfFragmentCode;
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(binFragCode);

    if (vkCreateShaderModule(device, &vertexCreateInfo, nullptr, &vertexShaderModule) != VK_SUCCESS)
    {
        printf("Failed to create the vertex shader module");
        assert(false);
    }

    if (vkCreateShaderModule(device, &fragCreateInfo, nullptr, &fragmentShaderModule) != VK_SUCCESS)
    {
        printf("Failed to create the fragement shader module");
        assert(false);
    }

    free((void*)binVertCode);
    binVertCode = nullptr;
    free((void*)binFragCode);
    binVertCode = nullptr;

    VkPipelineShaderStageCreateInfo vertShaderModuleInfo{};
    vertShaderModuleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderModuleInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderModuleInfo.module = vertexShaderModule;
    vertShaderModuleInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderModuleInfo{};
    fragShaderModuleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderModuleInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderModuleInfo.module = fragmentShaderModule;
    fragShaderModuleInfo.pName = "main";

    //Graphics pipeline creation
    VkPipelineShaderStageCreateInfo  shaderStages[] = { vertShaderModuleInfo, fragShaderModuleInfo };

    std::vector<VkDynamicState> dynamicStates = 
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = uint32_t(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    auto bindingDescription = Vertex::getBindingDescriptions();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &bindingDescription;
    vertexInputState.vertexAttributeDescriptionCount = uint32_t(attributeDescriptions.size());
    vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;	
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportCreateInfo{};
    viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportCreateInfo.viewportCount = 1;
    viewportCreateInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasteriser{};
    rasteriser.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasteriser.depthClampEnable = VK_FALSE;
    rasteriser.rasterizerDiscardEnable = VK_FALSE;
    rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
    rasteriser.lineWidth = 1.f;
    rasteriser.cullMode = VK_CULL_MODE_BACK_BIT;
    rasteriser.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasteriser.depthBiasEnable = VK_FALSE;
    rasteriser.depthBiasConstantFactor = 0.f;
    rasteriser.depthBiasClamp = 0.f;
    rasteriser.depthBiasSlopeFactor = 0.f;

    VkPipelineMultisampleStateCreateInfo multisamplingState{};
    multisamplingState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingState.sampleShadingEnable = VK_FALSE;
    multisamplingState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisamplingState.minSampleShading = 1.f; //optional
    multisamplingState.pSampleMask = nullptr; //optional
    multisamplingState.alphaToCoverageEnable = VK_FALSE; //optional
    multisamplingState.alphaToOneEnable = VK_FALSE; //optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.f;
    depthStencil.maxDepthBounds = 1.f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    VkPipelineColorBlendAttachmentState colourBlendAttachment{};
    colourBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colourBlendAttachment.blendEnable = VK_TRUE;
    colourBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colourBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colourBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colourBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colourBlending{};
    colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlending.logicOpEnable = VK_FALSE;
    colourBlending.logicOp = VK_LOGIC_OP_COPY;
    colourBlending.attachmentCount = 1;
    colourBlending.pAttachments = &colourBlendAttachment;
    colourBlending.blendConstants[0] = 0.f;
    colourBlending.blendConstants[1] = 0.f;
    colourBlending.blendConstants[2] = 0.f;
    colourBlending.blendConstants[3] = 0.f;

    std::array<VkAttachmentDescription, 2> atts = { colourAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = uint32_t(atts.size());
    renderPassCreateInfo.pAttachments = atts.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) 
    {
        assert(false);
        printf("Failed to create a render pass.");
    }

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) 
    {
        assert(false);
        printf("Failed to create the layout pipeline.\n");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportCreateInfo;
    pipelineInfo.pRasterizationState = &rasteriser;
    pipelineInfo.pMultisampleState = &multisamplingState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colourBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline mainPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mainPipeline) != VK_SUCCESS) 
    {
        assert(false);
        printf("Failed to create pipeline.");
    }

    vkDestroyShaderModule(device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

    //Command pool creation
    commandPool = createCommandPool(mainQueueFamilyIndex);
    transferCommandPool = createCommandPool(transferQueueFamilyIndex);

    //Create depth resources
    createDepthResources();

    //Framebuffer creation
    createFramebuffers();

    //Create texture and samplers
    VkImage image;
    VkDeviceMemory imageMemory;
    createTextureImage("Assets/Textures/frobert.png", image, imageMemory);
    VkImageView textureImageView = createTextureImageView(image, VK_FORMAT_R8G8B8A8_SRGB);
    VkSampler textureSampler = createSampler();

    //Index and vertex buffer creation.
    //Transfering of buffers works because the usage e.g VK_BUFFER_USAGE_TRANSFER_SRC_BIT and synced up. 
    //Meaning that when we create a vertex staging buffer with TRANSFER_SRC_BIT and our vertex buffer with TRANSFER_DST_BIT the copy from one to another is valid.
    //You shouldn't make the buffer sharingMode VK_SHARING_MODE_EXCLUSIVE because we aren't sharing data between then we are copying it from on to another.
    VkDeviceSize vertexbufferSize = sizeof(vertices[0]) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
    VkDeviceSize totalBufferSize = vertexbufferSize + indexBufferSize;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(totalBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, vertexbufferSize, 0, &data);
    memcpy(data, vertices.data(), size_t(vertexbufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    void* indexData;
    vkMapMemory(device, stagingBufferMemory, vertexbufferSize, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), size_t(indexBufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    VkBuffer modelBuffer;
    VkDeviceMemory modelBufferMemory;

    createBuffer(totalBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, modelBuffer, modelBufferMemory);

    copyBuffer(stagingBuffer, modelBuffer, totalBufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    //Uniform buffer creation
    createUniformBuffers();

    //Descriptor pool creation
    std::array<VkDescriptorPoolSize, 2> poolSize{};
    poolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize[0].descriptorCount = imageCount;
    poolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize[1].descriptorCount = imageCount;

    VkDescriptorPoolCreateInfo poolDescriptorCreateInfo{};
    poolDescriptorCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolDescriptorCreateInfo.poolSizeCount = uint32_t(poolSize.size());
    poolDescriptorCreateInfo.pPoolSizes = poolSize.data();
    poolDescriptorCreateInfo.maxSets = imageCount;

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(device, &poolDescriptorCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        printf("Failed to create descriptor pool");
        assert(false);
    }

    //Descriptor sets creation
    std::vector<VkDescriptorSetLayout> layouts(imageCount, descriptorSetLayout);
    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = descriptorPool;
    descriptorAllocateInfo.descriptorSetCount = imageCount;
    descriptorAllocateInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptorSets(imageCount);
    if (vkAllocateDescriptorSets(device, &descriptorAllocateInfo, descriptorSets.data()) != VK_SUCCESS) 
    {
        printf("Failed to allocate descriptors sets");
        assert(false);
    }

    for (uint32_t i = 0; i < imageCount; ++i) 
    {
        VkDescriptorBufferInfo desBufferInfo{};
        desBufferInfo.buffer = uniformBuffers[i];
        desBufferInfo.offset = 0;
        desBufferInfo.range = sizeof(ModelData);

        VkDescriptorImageInfo desImageInfo{};
        desImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desImageInfo.imageView = textureImageView;
        desImageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &desBufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = &desImageInfo;
        descriptorWrites[1].pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(device, uint32_t(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    //Command buffer creation
    const uint32_t maxFramesInFlight = imageCount;
    std::vector<VkCommandBuffer> commandBuffers(maxFramesInFlight);
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = uint32_t(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data())) 
    {
        assert(false);
        printf("Failed to create a command buffer.");
    }

    //Synchronise object creation
    std::vector<VkSemaphore> imageAvailableSemaphore(maxFramesInFlight);
    std::vector<VkSemaphore> renderFinishSemaphore(maxFramesInFlight);
    std::vector<VkFence> framesInFlight(maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore[i]) != VK_SUCCESS)
        {
            printf("Failed to create image available semaphore");
            assert(false);
        }

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishSemaphore[i]) != VK_SUCCESS)
        {
            printf("Failed to create render finished semaphore");
            assert(false);
        }

        if (vkCreateFence(device, &fenceInfo, nullptr, &framesInFlight[i]) != VK_SUCCESS)
        {
            printf("Failed to create the fence.");
            assert(false);
        }
    }

    bool running = true;
    SDL_Event event;
    bool framebufferResize = false;

    auto startTime = std::chrono::high_resolution_clock::now();
    uint32_t currentFrame = 0;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            {
                running = false;
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            {
                if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_Q)
                {
                    running = false;
                }
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED:
            {
                framebufferResize = true;
                break;
            }
            }
        }

        vkWaitForFences(device, 1, &framesInFlight[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            continue;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            printf("Failed to acquire swapchain image at image index %d", imageIndex);
            assert(false);
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        ModelData modelData{};
        modelData.model = glms_rotate(glms_mat4_identity(), time * glm_rad(90.f), { 0.f, 0.f, 1.f });
        modelData.view = glms_lookat({ 2.f, 2.f, 2.f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 1.f });
        modelData.proj = glms_perspective(glm_rad(45.f), swapchainExtent.width / (float)swapchainExtent.height, 100.f, 1.f);
        modelData.proj.m11 *= -1;

        memcpy(uniformBuffersMapped[currentFrame], &modelData, sizeof(modelData));

        vkResetFences(device, 1, &framesInFlight[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = 0;

        if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS)
        {
            assert(false);
            printf("Failed to begin recording command buffer.");
        }

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
        renderPassBeginInfo.renderArea.offset = { 0, 0 };
        renderPassBeginInfo.renderArea.extent = swapchainExtent;
        
        // 0.218f, 0.f, 0.265f, 1.f
        std::array<VkClearValue, 2> clearColour{};
        clearColour[0] = { { 0.f, 0.535f, 1.f, 1.f } };
        clearColour[1] = { { 0.f, 0 } };
        renderPassBeginInfo.clearValueCount = uint32_t(clearColour.size());
        renderPassBeginInfo.pClearValues = clearColour.data();

        vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline);

        VkViewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = float(swapchainExtent.width);
        viewport.height = float(swapchainExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapchainExtent;
        vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { modelBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers[currentFrame], modelBuffer, vertexbufferSize, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

        vkCmdDrawIndexed(commandBuffers[currentFrame], uint32_t(indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffers[currentFrame]);

        if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS)
        {
            printf("Failed to record a command buffer");
            assert(false);
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphore[currentFrame]};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        
        VkSemaphore signalSemaphores[] = { renderFinishSemaphore[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(mainQueue, 1, &submitInfo, framesInFlight[currentFrame]) != VK_SUCCESS)
        {
            printf("Failed to submit the command buffer to draw with the current queue.");
            assert(false);
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = { swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults;

        result = vkQueuePresentKHR(mainQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResize) 
        {
            framebufferResize = false;
            recreateSwapchain();

            currentFrame = (currentFrame + 1) % maxFramesInFlight;
            continue;
        }
        else if(result != VK_SUCCESS)
        {
            printf("Failed or present swapchain image!");
            assert(false);
        }

        currentFrame = (currentFrame + 1) % maxFramesInFlight;
    }

    vkDeviceWaitIdle(device);

#ifdef SKELETON_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    vkDestroyDebugUtilsMessengerEXT(instance, vulkanDebugUtilsMessenger, nullptr);
#endif // SKELETON_DEBUG
    
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    cleanupSwapchain();

    vkDestroySampler(device, textureSampler, nullptr);
    vkDestroyImageView(device, textureImageView, nullptr);

    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, imageMemory, nullptr);

    vkDestroyPipeline(device, mainPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    for (uint32_t i = 0; i < maxFramesInFlight; ++i) 
    {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    vkDestroyBuffer(device, modelBuffer, nullptr);
    vkFreeMemory(device, modelBufferMemory, nullptr);

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        vkDestroySemaphore(device, imageAvailableSemaphore[i], nullptr);
        vkDestroySemaphore(device, renderFinishSemaphore[i], nullptr);
        vkDestroyFence(device, framesInFlight[i], nullptr);
    }

    vkDestroyCommandPool(device, transferCommandPool, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyDevice(device, nullptr);

    SDL_Vulkan_DestroySurface(instance, surface, nullptr);

    vkDestroyInstance(instance, nullptr);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}