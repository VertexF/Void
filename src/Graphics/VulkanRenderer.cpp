#include "VulkanRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#if defined(_MSC_VER)
#include <windows.h>
#elif defined(__linux__)
#include <csignal>
#endif

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct/affine.h>
#include <cglm/struct/cam.h>

#define SHADER_ASSETS "Assets/Shaders/"
#define SKELETON_DEBUG

#include "Foundation/Log.hpp"
#include "Foundation/Assert.hpp"
#include "Foundation/Time.hpp"
#include "Foundation/Camera.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/File.hpp"

#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/GameCamera.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#if defined(DEBUG_CHECKING)
#define check(result) VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Assert Code %u", result)
#else
#define check(result) (result)
#endif

namespace 
{
    Array<Vertex> vertices;
    Array<uint16_t> indices;
}

static uint32_t clamp(uint32_t value, uint32_t minimum, uint32_t maximum)
{
    return min(minimum, max(value, maximum));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*userData*/)
{
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
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

#if defined(_MSC_VER)
        __debugbreak();
#elif defined(__LINUX__)
        std::raise(SIGINT);
#endif
    }

    return VK_FALSE;
}

static uint32_t findMemoryType(VulkanRenderer* renderer, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    // Find memory type
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(renderer->physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    VOID_ERROR("Failed to find suitable memory type.");
    return UINT32_MAX;
}

static VkCommandPool createCommandPool(VulkanRenderer* renderer, uint32_t queueFamilyIndex)
{
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool pool;
    check(vkCreateCommandPool(renderer->device, &poolCreateInfo, nullptr, &pool));

    return pool;
}

static VkCommandBuffer beginOneTimeCommandBuffer(VulkanRenderer* renderer, const VkCommandPool& pool)
{
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandPool = pool;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(renderer->device, &allocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void endOneTimeCommandBuffer(VulkanRenderer* renderer, VkCommandBuffer commandBuffer, const VkCommandPool& cmdPool, const VkQueue& queue)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(renderer->device, cmdPool, 1, &commandBuffer);
}

static void createBuffer(VulkanRenderer* renderer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    //Vertex buffer creation.
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    //You shouldn't make the buffer sharingMode VK_SHARING_MODE_CONCURRENT because we aren't sharing data between then we are copying it from on to another.
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check(vkCreateBuffer(renderer->device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(renderer->device, buffer, &memoryRequirements);

    VkMemoryAllocateInfo bufferAllocationInfo{};
    bufferAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferAllocationInfo.allocationSize = memoryRequirements.size;
    bufferAllocationInfo.memoryTypeIndex = findMemoryType(renderer, memoryRequirements.memoryTypeBits, properties);

    check(vkAllocateMemory(renderer->device, &bufferAllocationInfo, nullptr, &bufferMemory));

    vkBindBufferMemory(renderer->device, buffer, bufferMemory, 0);
}

static void copyBuffer(VulkanRenderer* renderer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer(renderer, renderer->transferCommandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endOneTimeCommandBuffer(renderer, commandBuffer, renderer->transferCommandPool, renderer->transferQueue);
}

static void copyBufferToImage(VulkanRenderer* renderer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer(renderer, renderer->transferCommandPool);

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

    endOneTimeCommandBuffer(renderer, commandBuffer, renderer->transferCommandPool, renderer->transferQueue);
}

static bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static void transitionImageLayout(VulkanRenderer* renderer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer(renderer, renderer->commandPool);

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
        VOID_ERROR("Not support memory image layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endOneTimeCommandBuffer(renderer, commandBuffer, renderer->commandPool, renderer->mainQueue);
}

static void createImage(VulkanRenderer* renderer, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
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

    check(vkCreateImage(renderer->device, &imageInfo, nullptr, &image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(renderer->device, image, &memRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(renderer, memRequirements.memoryTypeBits, properties);

    check(vkAllocateMemory(renderer->device, &allocateInfo, nullptr, &imageMemory));

    vkBindImageMemory(renderer->device, image, imageMemory, 0);
}

//"Assets/Textures/frobert.png"
static void createTextureImage(VulkanRenderer* renderer, const char* path, VkImage& textureImage, VkDeviceMemory& textureImageMemory)
{
    int texWidth;
    int texHeight;
    int channles;

    //Here load the raw pixel data to an unsigned char* 
    stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &channles, STBI_rgb_alpha);
    //We also need the device size for the staging buffers.
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    //Next we check if it's loaded correctly
    VOID_ASSERTM(pixels != nullptr, "Failed to load texture image.");

    //Then we start with the staging buffers.
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;

    //We are making the staging buffer for copying to an image so we use VK_BUFFER_USAGE_TRANSFER_SRC_BIT to transfer
    //and it needs to he host visible so we can load data in.
    createBuffer(renderer, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, imageStagingBuffer, imageStagingBufferMemory);

    //Next we copy the data to the staging buffer using the imageSize we calculated eariler when we loaded the image from disk.
    void* imageData;
    vkMapMemory(renderer->device, imageStagingBufferMemory, 0, imageSize, 0, &imageData);
    memcpy(imageData, pixels, size_t(imageSize));
    vkUnmapMemory(renderer->device, imageStagingBufferMemory);

    //After that we free the pixels as they are in the staging buffer.
    stbi_image_free(pixels);

    //Next we create the image we are going to copy the buffer into. 
    //We aren't doing anything special but it needs to be created with VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT as it will live on the GPU
    //and be used to sampled on VK_IMAGE_USAGE_SAMPLED_BIT and it needs to be the destination 
    //for our copy so we need the VK_IMAGE_USAGE_TRANSFER_DST_BIT to be ORed with it.
    createImage(renderer, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    //The we transition image ready to receive data.  So the image needs to be have the same format and type as the usage type.
    transitionImageLayout(renderer, textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    //Next we copy the entire image from the buffer to the image.
    copyBufferToImage(renderer, imageStagingBuffer, textureImage, uint32_t(texWidth), uint32_t(texHeight));
    //After that we transition the image memory layout to be read by the shader. 
    transitionImageLayout(renderer, textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //Finally we can destroy the staging buffers.
    vkDestroyBuffer(renderer->device, imageStagingBuffer, nullptr);
    vkFreeMemory(renderer->device, imageStagingBufferMemory, nullptr);
}

static VkImageView createImageView(VulkanRenderer* renderer, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
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
    check(vkCreateImageView(renderer->device, &createImageViewInfo, nullptr, &imageView));

    return imageView;
}

static VkImageView createTextureImageView(VulkanRenderer* renderer, const VkImage& image, VkFormat format)
{
    return createImageView(renderer, image, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

static VkSampler createSampler(VulkanRenderer* renderer)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = renderer->maxAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.f;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = 0.f;

    VkSampler sampler;
    check(vkCreateSampler(renderer->device, &samplerInfo, nullptr, &sampler));
    return sampler;
}

static void createUniformBuffers(VulkanRenderer* renderer)
{
    VkDeviceSize bufferSize = sizeof(ModelData);

    renderer->uniformBuffers.init(renderer->allocator, renderer->imageCount, renderer->imageCount);
    renderer->uniformBuffersMemory.init(renderer->allocator, renderer->imageCount, renderer->imageCount);
    renderer->uniformBuffersMapped.init(renderer->allocator, renderer->imageCount, renderer->imageCount);

    for (uint32_t i = 0; i < renderer->imageCount; ++i)
    {
        createBuffer(renderer, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderer->uniformBuffers[i], renderer->uniformBuffersMemory[i]);

        vkMapMemory(renderer->device, renderer->uniformBuffersMemory[i], 0, bufferSize, 0, &renderer->uniformBuffersMapped[i]);
    }
}

static VkFormat findSupportedFormat(VulkanRenderer* renderer, const Array<VkFormat>& candidtes, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (uint32_t i = 0; i < candidtes.size; ++i)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(renderer->physicalDevice, candidtes[i], &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
        {
            return candidtes[i];
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
        {
            return candidtes[i];
        }
    }

    VOID_ERROR("No supported format based on the feature enum %d", features);
    return VK_FORMAT_MAX_ENUM;
}

static VkFormat findDepthFormat(VulkanRenderer* renderer)
{
    Array<VkFormat> possibleDepthFormat;
    possibleDepthFormat.init(&renderer->stackAllocator, 3);
    possibleDepthFormat.push(VK_FORMAT_D32_SFLOAT);
    possibleDepthFormat.push(VK_FORMAT_D32_SFLOAT_S8_UINT);
    possibleDepthFormat.push(VK_FORMAT_D24_UNORM_S8_UINT);
    return findSupportedFormat(renderer, possibleDepthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static void createDepthResources(VulkanRenderer* renderer)
{
    VkFormat depthFormat = findDepthFormat(renderer);

    createImage(renderer, renderer->swapchainExtent.width, renderer->swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, renderer->depthImage, renderer->depthImageMemory);
    renderer->depthImageView = createImageView(renderer, renderer->depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

static void cleanupSwapchain(VulkanRenderer* renderer)
{
    vkDestroyImageView(renderer->device, renderer->depthImageView, nullptr);
    vkDestroyImage(renderer->device, renderer->depthImage, nullptr);
    vkFreeMemory(renderer->device, renderer->depthImageMemory, nullptr);

    for (uint32_t i = 0; i < renderer->swapchainFramebuffers.size; ++i)
    {
        vkDestroyFramebuffer(renderer->device, renderer->swapchainFramebuffers[i], nullptr);
    }

    for (uint32_t i = 0; i < renderer->swapchainImageViews.size; ++i)
    {
        vkDestroyImageView(renderer->device, renderer->swapchainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(renderer->device, renderer->swapchain, nullptr);
}

static void createSwapchain(VulkanRenderer* renderer)
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice, renderer->surface, &formatCount, nullptr);
    Array<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.init(&renderer->stackAllocator, formatCount, formatCount);
    if (formatCount != 0)
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice, renderer->surface, &formatCount, surfaceFormats.data);
    }

    VkSurfaceFormatKHR swapchainSurface{};
    for (uint32_t i = 0; i < surfaceFormats.size; ++i)
    {
        if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            swapchainSurface = surfaceFormats[i];
        }
    }
    swapchainSurface = surfaceFormats[0];

    uint32_t presentationModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physicalDevice, renderer->surface, &presentationModeCount, nullptr);

    Array<VkPresentModeKHR> presentationModes;
    presentationModes.init(&renderer->stackAllocator, presentationModeCount, presentationModeCount);
    if (presentationModeCount != 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physicalDevice, renderer->surface, &presentationModeCount, presentationModes.data);
    }

    VkPresentModeKHR presentationMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
    for (uint32_t i = 0; i < presentationModes.size; ++i)
    {
        if (presentationModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentationMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }

        presentationMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    if (renderer->swapchainExtent.width != UINT32_MAX)
    {
        renderer->swapchainExtent.width = clamp(renderer->swapchainExtent.width, renderer->surfaceCapabilities.minImageExtent.width, renderer->surfaceCapabilities.maxImageExtent.width);
        renderer->swapchainExtent.height = clamp(renderer->swapchainExtent.height, renderer->surfaceCapabilities.minImageExtent.height, renderer->surfaceCapabilities.maxImageExtent.height);
    }

    renderer->imageCount = renderer->surfaceCapabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR createSwapchain{};
    createSwapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createSwapchain.surface = renderer->surface;
    createSwapchain.minImageCount = renderer->imageCount;
    createSwapchain.imageFormat = swapchainSurface.format;
    createSwapchain.imageColorSpace = swapchainSurface.colorSpace;
    createSwapchain.imageExtent = renderer->swapchainExtent;
    createSwapchain.imageArrayLayers = 1;
    createSwapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    //We always assume the presentation queue family and the graphics queue family are the same. 
    createSwapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createSwapchain.queueFamilyIndexCount = 0;
    createSwapchain.pQueueFamilyIndices = nullptr;
    createSwapchain.preTransform = renderer->surfaceCapabilities.currentTransform;
    createSwapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createSwapchain.presentMode = presentationMode;
    createSwapchain.clipped = VK_TRUE;

    vkCreateSwapchainKHR(renderer->device, &createSwapchain, nullptr, &renderer->swapchain);

    vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain, &renderer->imageCount, nullptr);
    renderer->swapchainImages.setSize(renderer->imageCount);
    vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain, &renderer->imageCount, renderer->swapchainImages.data);

    renderer->swapchainFormat = swapchainSurface.format;
}

static void createImageViews(VulkanRenderer* renderer)
{
    renderer->swapchainImageViews.setSize(renderer->swapchainImages.size);

    for (uint32_t i = 0; i < renderer->swapchainImages.size; ++i)
    {
        renderer->swapchainImageViews[i] = createImageView(renderer, renderer->swapchainImages[i], renderer->swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

static void createFramebuffers(VulkanRenderer* renderer)
{
    renderer->swapchainFramebuffers.setSize(renderer->swapchainImageViews.size);

    for (uint32_t i = 0; i < renderer->swapchainImageViews.size; ++i)
    {
        VkImageView atttas[] =
        {
            renderer->swapchainImageViews[i],
            renderer->depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderer->renderPass;
        framebufferInfo.attachmentCount = ArraySize(atttas);
        framebufferInfo.pAttachments = atttas;
        framebufferInfo.width = renderer->swapchainExtent.width;
        framebufferInfo.height = renderer->swapchainExtent.height;
        framebufferInfo.layers = 1;

        check(vkCreateFramebuffer(renderer->device, &framebufferInfo, nullptr, &renderer->swapchainFramebuffers[i]));
    }
}

static void recreateSwapchain(VulkanRenderer* renderer)
{
    vkDeviceWaitIdle(renderer->device);

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physicalDevice, renderer->surface, &renderer->surfaceCapabilities);
    renderer->swapchainExtent = renderer->surfaceCapabilities.currentExtent;

    if (renderer->swapchainExtent.width == 0 || renderer->swapchainExtent.height == 0)
    {
        return;
    }

    cleanupSwapchain(renderer);

    createSwapchain(renderer);
    createImageViews(renderer);
    createDepthResources(renderer);
    createFramebuffers(renderer);
}

int runGame(VulkanRenderer* renderer)
{
    //Init services
    MemoryServiceConfiguration memoryConfiguration;
    memoryConfiguration.maximumDynamicSize = void_giga(1ull);

    MemoryService::instance()->init(&memoryConfiguration);
    renderer->allocator = &MemoryService::instance()->systemAllocator;
    renderer->stackAllocator.init(void_mega(8));

    Window::instance()->init(1280, 720, "Void");

    renderer->resourceManager.init(renderer->allocator, nullptr);

    InputHandler inputHandler;
    inputHandler.init(renderer->allocator);
    //TODO: Use reverse-Z perspective

    //Timing set up.
    timeServiceInit();

    indices.init(renderer->allocator, 12);
    indices.push(0);
    indices.push(1);
    indices.push(2);
    indices.push(2);
    indices.push(3);
    indices.push(0);
    indices.push(4);
    indices.push(5);
    indices.push(6);
    indices.push(6);
    indices.push(7);
    indices.push(4);

    vertices.init(renderer->allocator, 12);
    vertices.push({ { -0.5f, -0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f } });
    vertices.push({ {  0.5f, -0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f } });
    vertices.push({ {  0.5f,  0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 0.f, 1.f } });
    vertices.push({ { -0.5f,  0.5f, 0.f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f } });

    vertices.push({ { -0.5f, -0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f } });
    vertices.push({ {  0.5f, -0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f } });
    vertices.push({ {  0.5f,  0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 0.f, 1.f } });
    vertices.push({ { -0.5f,  0.5f, -0.5f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f } });

    renderer->swapchainImageViews.init(renderer->allocator, 3, 3);
    renderer->swapchainFramebuffers.init(renderer->allocator, 3, 3);
    renderer->swapchainImages.init(renderer->allocator, 3, 3);

    const char* REQUESTED_EXTENSIONS[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined (__linux__)
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
        //For surface creation we need to have x11 extension available. 
        //We can't include <vulkan/vulkan_xlib.h> because it doesn't compile so we just include the raw const char*
        "VK_KHR_xlib_surface",
#endif
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
    appInfo.apiVersion = VK_API_VERSION_1_3;

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

    check(vkCreateInstance(&instanceInfo, nullptr, &renderer->instance));

    //VOID_ASSERTM(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface), "Failed to create SDL/Vulkan Surface.");

    if (SDL_Vulkan_CreateSurface(Window::instance()->platformHandle, renderer->instance, nullptr, &renderer->surface) == false)
    {
        vprint(SDL_GetError());
    }

#ifdef SKELETON_DEBUG
    VkDebugUtilsMessengerEXT vulkanDebugUtilsMessenger;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer->instance, "vkCreateDebugUtilsMessengerEXT");
    vkCreateDebugUtilsMessengerEXT(renderer->instance, &debugUtilInfo, nullptr, &vulkanDebugUtilsMessenger);
#endif // SKELETON_DEBUG

    //Selecting physical device
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(renderer->instance, &physicalDeviceCount, nullptr);
    Array<VkPhysicalDevice> gpus{};
    gpus.init(&renderer->stackAllocator, physicalDeviceCount, physicalDeviceCount);
    vkEnumeratePhysicalDevices(renderer->instance, &physicalDeviceCount, gpus.data);

    uint32_t highestScore = 0;
    uint32_t bestGPUIndex = 0;
    for (uint32_t i = 0; i < gpus.size; ++i)
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
            renderer->maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        }
        score = 0;
    }

    renderer->physicalDevice = gpus[bestGPUIndex];
    gpus.shutdown();

    //Getting physical device level extensions out of the physical device.
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(renderer->physicalDevice, nullptr, &extensionCount, nullptr);
    Array<VkExtensionProperties> availableExtensions{};
    availableExtensions.init(&renderer->stackAllocator, extensionCount, extensionCount);
    vkEnumerateDeviceExtensionProperties(renderer->physicalDevice, nullptr, &extensionCount, availableExtensions.data);

    Array<const char*> usableDeviceExtensions{};
    usableDeviceExtensions.init(renderer->allocator, 8);
    //Assuming you've picked the best GPU we assume that we have the most available.
    //NOTE: I'm looping through all the available extensions here even though we only care about the swapchain one, because we likely want more soon.
    for (uint32_t i = 0; i < availableExtensions.size; ++i)
    {
        if (strcmp(availableExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            usableDeviceExtensions.push(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            continue;
        }

        if (strcmp(availableExtensions[i].extensionName, VK_KHR_8BIT_STORAGE_EXTENSION_NAME) == 0)
        {
            usableDeviceExtensions.push(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
            continue;
        }

        if (strcmp(availableExtensions[i].extensionName, VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME) == 0)
        {
            usableDeviceExtensions.push(VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME);
            continue;
        }
    }

    availableExtensions.shutdown();

    //TODO: This check is bad, there might be GPU that has some random extension and NOT the VK_KHR_SWAPCHAIN_EXTENSION_NAME fix it later.
    VOID_ASSERTM(usableDeviceExtensions.size != 0, "You need at least the %s device level extension working the GPU.\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    //Looking for device features that are aviable.
    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.pNext = nullptr;
    void* currentPNext = &indexingFeatures;

    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &indexingFeatures;
    vulkan13Features.shaderDemoteToHelperInvocation = VK_TRUE;
    currentPNext = &vulkan13Features;

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures.pNext = currentPNext;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;

    vkGetPhysicalDeviceFeatures2(renderer->physicalDevice, &deviceFeatures);

    //Setting up querying family index
    uint32_t familyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physicalDevice, &familyPropertyCount, nullptr);
    Array<VkQueueFamilyProperties> queueFamilyProperties{};
    queueFamilyProperties.init(&renderer->stackAllocator, familyPropertyCount, familyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physicalDevice, &familyPropertyCount, queueFamilyProperties.data);

    VkBool32 surfaceSupported = VK_FALSE;
    for (uint32_t i = 0; i < familyPropertyCount; ++i)
    {
        if (queueFamilyProperties.size == 0)
        {
            continue;
        }

        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(renderer->physicalDevice, i, renderer->surface, &surfaceSupported);
            if (surfaceSupported)
            {
                renderer->mainQueueFamilyIndex = i;
                continue;
            }
        }

        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            renderer->transferQueueFamilyIndex = i;
            continue;
        }

        if (renderer->mainQueueFamilyIndex != UINT32_MAX && renderer->transferQueueFamilyIndex != UINT32_MAX)
        {
            break;
        }
    }

    queueFamilyProperties.shutdown();

    //Logical device and queue creation 
    const float priority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = renderer->mainQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priority;

    if (renderer->transferQueueFamilyIndex != UINT32_MAX)
    {
        VkDeviceQueueCreateInfo queueTransferCreateInfo{};
        queueTransferCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueTransferCreateInfo.queueFamilyIndex = renderer->transferQueueFamilyIndex;
        queueTransferCreateInfo.queueCount = 1;
        queueTransferCreateInfo.pQueuePriorities = &priority;

        VkDeviceQueueCreateInfo localDeviceQueues[] = { queueCreateInfo, queueTransferCreateInfo };

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures;
        deviceCreateInfo.ppEnabledExtensionNames = usableDeviceExtensions.data;
        deviceCreateInfo.enabledExtensionCount = usableDeviceExtensions.size;
        deviceCreateInfo.pQueueCreateInfos = localDeviceQueues;
        deviceCreateInfo.queueCreateInfoCount = sizeof(localDeviceQueues) / sizeof(localDeviceQueues[0]);

        vkCreateDevice(renderer->physicalDevice, &deviceCreateInfo, nullptr, &renderer->device);
    }
    else
    {
        VkDeviceQueueCreateInfo localDeviceQueues[] = { queueCreateInfo };

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures;
        deviceCreateInfo.ppEnabledExtensionNames = usableDeviceExtensions.data;
        deviceCreateInfo.enabledExtensionCount = usableDeviceExtensions.size;
        deviceCreateInfo.pQueueCreateInfos = localDeviceQueues;
        deviceCreateInfo.queueCreateInfoCount = sizeof(localDeviceQueues) / sizeof(localDeviceQueues[0]);

        //HACK
        renderer->transferQueueFamilyIndex = renderer->mainQueueFamilyIndex;

        vkCreateDevice(renderer->physicalDevice, &deviceCreateInfo, nullptr, &renderer->device);
    }

    usableDeviceExtensions.shutdown();

    vkGetDeviceQueue(renderer->device, renderer->mainQueueFamilyIndex, 0, &renderer->mainQueue);
    vkGetDeviceQueue(renderer->device, renderer->transferQueueFamilyIndex, 0, &renderer->transferQueue);

    //Swapchain set up.
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physicalDevice, renderer->surface, &renderer->surfaceCapabilities);
    renderer->swapchainExtent = renderer->surfaceCapabilities.currentExtent;
    createSwapchain(renderer);
    createImageViews(renderer);

    //Subpass dependencies, subpass attachment references and render pass attachments set up.
    VkAttachmentDescription colourAttachment{};
    colourAttachment.format = renderer->swapchainFormat;
    colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat(renderer);
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
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[] = { uboLayoutBinding, sampleLayoutBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ArraySize(descriptorSetLayoutBinding);
    layoutInfo.pBindings = descriptorSetLayoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    check(vkCreateDescriptorSetLayout(renderer->device, &layoutInfo, nullptr, &descriptorSetLayout));

    //Shader modules creation
    FileReadResult binVertCode = fileReadBinary(SHADER_ASSETS"mainShader.vert.spv", renderer->allocator);
    FileReadResult binFragCode = fileReadBinary(SHADER_ASSETS"mainShader.frag.spv", renderer->allocator);

    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;

    VkShaderModuleCreateInfo vertexCreateInfo{};
    vertexCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexCreateInfo.codeSize = binVertCode.size;
    vertexCreateInfo.pCode = reinterpret_cast<const uint32_t*>(binVertCode.data);

    VkShaderModuleCreateInfo fragCreateInfo{};
    fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragCreateInfo.codeSize = binFragCode.size;
    fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(binFragCode.data);

    check(vkCreateShaderModule(renderer->device, &vertexCreateInfo, nullptr, &vertexShaderModule));
    check(vkCreateShaderModule(renderer->device, &fragCreateInfo, nullptr, &fragmentShaderModule));

    void_free(binVertCode.data, renderer->allocator);
    void_free(binFragCode.data, renderer->allocator);

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
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderModuleInfo, fragShaderModuleInfo };

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = ArraySize(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    auto bindingDescription = Vertex::getBindingDescriptions();
    auto attributeDescriptions = Vertex::getAttributeDescriptions(renderer->stackAllocator);

    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &bindingDescription;
    vertexInputState.vertexAttributeDescriptionCount = attributeDescriptions.size;
    vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data;

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

    VkAttachmentDescription atts[] = { colourAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = ArraySize(atts);
    renderPassCreateInfo.pAttachments = atts;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &dependency;

    check(vkCreateRenderPass(renderer->device, &renderPassCreateInfo, nullptr, &renderer->renderPass));

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    check(vkCreatePipelineLayout(renderer->device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

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
    pipelineInfo.renderPass = renderer->renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline mainPipeline;
    check(vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mainPipeline));

    vkDestroyShaderModule(renderer->device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(renderer->device, fragmentShaderModule, nullptr);

    //Command pool creation
    renderer->commandPool = createCommandPool(renderer, renderer->mainQueueFamilyIndex);
    renderer->transferCommandPool = createCommandPool(renderer, renderer->transferQueueFamilyIndex);

    //Create depth resources
    createDepthResources(renderer);

    //Framebuffer creation
    createFramebuffers(renderer);

    //Create texture and samplers
    VkImage image;
    VkDeviceMemory imageMemory;
    createTextureImage(renderer, "Assets/Textures/frobert.png", image, imageMemory);
    VkImageView textureImageView = createTextureImageView(renderer, image, VK_FORMAT_R8G8B8A8_SRGB);
    VkSampler textureSampler = createSampler(renderer);

    VkDeviceSize vertexbufferSize = sizeof(vertices[0]) * vertices.size;
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size;
    VkDeviceSize totalBufferSize = vertexbufferSize + indexBufferSize;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    //Index and vertex buffer creation.
    //Transfering of buffers works because the usage e.g VK_BUFFER_USAGE_TRANSFER_SRC_BIT and synced up. 
    //Meaning that when we create a vertex staging buffer with TRANSFER_SRC_BIT and our vertex buffer with TRANSFER_DST_BIT the copy from one to another is valid.
    createBuffer(renderer, totalBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(renderer->device, stagingBufferMemory, 0, vertexbufferSize, 0, &data);
    memcpy(data, vertices.data, size_t(vertexbufferSize));
    vkUnmapMemory(renderer->device, stagingBufferMemory);

    void* indexData;
    vkMapMemory(renderer->device, stagingBufferMemory, vertexbufferSize, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data, size_t(indexBufferSize));
    vkUnmapMemory(renderer->device, stagingBufferMemory);

    VkBuffer modelBuffer;
    VkDeviceMemory modelBufferMemory;

    createBuffer(renderer, totalBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, modelBuffer, modelBufferMemory);

    copyBuffer(renderer, stagingBuffer, modelBuffer, totalBufferSize);

    vkDestroyBuffer(renderer->device, stagingBuffer, nullptr);
    vkFreeMemory(renderer->device, stagingBufferMemory, nullptr);

    //Uniform buffer creation
    createUniformBuffers(renderer);

    //Descriptor pool creation
    Array<VkDescriptorPoolSize> poolSize{};
    poolSize.init(&renderer->stackAllocator, 2, 2);
    poolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize[0].descriptorCount = renderer->imageCount;
    poolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize[1].descriptorCount = renderer->imageCount;

    VkDescriptorPoolCreateInfo poolDescriptorCreateInfo{};
    poolDescriptorCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolDescriptorCreateInfo.poolSizeCount = poolSize.size;
    poolDescriptorCreateInfo.pPoolSizes = poolSize.data;
    poolDescriptorCreateInfo.maxSets = renderer->imageCount;

    VkDescriptorPool descriptorPool;
    check(vkCreateDescriptorPool(renderer->device, &poolDescriptorCreateInfo, nullptr, &descriptorPool));

    //Descriptor sets creation
    Array<VkDescriptorSetLayout> layouts{};
    layouts.init(renderer->allocator, renderer->imageCount);
    for (uint32_t i = 0; i < renderer->imageCount; ++i)
    {
        layouts.push(descriptorSetLayout);
    }
    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = descriptorPool;
    descriptorAllocateInfo.descriptorSetCount = renderer->imageCount;
    descriptorAllocateInfo.pSetLayouts = layouts.data;

    Array<VkDescriptorSet> descriptorSets;
    descriptorSets.init(renderer->allocator, renderer->imageCount, renderer->imageCount);
    check(vkAllocateDescriptorSets(renderer->device, &descriptorAllocateInfo, descriptorSets.data));

    Array<VkWriteDescriptorSet> descriptorWrites{};
    descriptorWrites.init(renderer->allocator, 2);
    descriptorWrites.push({});
    descriptorWrites.push({});
    for (uint32_t i = 0; i < renderer->imageCount; ++i)
    {
        VkDescriptorBufferInfo desBufferInfo{};
        desBufferInfo.buffer = renderer->uniformBuffers[i];
        desBufferInfo.offset = 0;
        desBufferInfo.range = sizeof(ModelData);

        VkDescriptorImageInfo desImageInfo{};
        desImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desImageInfo.imageView = textureImageView;
        desImageInfo.sampler = textureSampler;

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

        vkUpdateDescriptorSets(renderer->device, descriptorWrites.size, descriptorWrites.data, 0, nullptr);
    }
    layouts.shutdown();
    descriptorWrites.shutdown();

    //Command buffer creation
    const uint32_t maxFramesInFlight = renderer->imageCount;
    Array<VkCommandBuffer> commandBuffers{};
    commandBuffers.init(renderer->allocator, maxFramesInFlight, maxFramesInFlight);
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = renderer->commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = commandBuffers.size;

    check(vkAllocateCommandBuffers(renderer->device, &allocateInfo, commandBuffers.data));

    //Synchronise object creation
    Array<VkSemaphore> imageAvailableSemaphore{};
    imageAvailableSemaphore.init(renderer->allocator, maxFramesInFlight, maxFramesInFlight);
    Array<VkSemaphore> renderFinishSemaphore{};
    renderFinishSemaphore.init(renderer->allocator, maxFramesInFlight, maxFramesInFlight);
    Array<VkFence> framesInFlight{};
    framesInFlight.init(renderer->allocator, maxFramesInFlight, maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        check(vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &imageAvailableSemaphore[i]));
        check(vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &renderFinishSemaphore[i]));

        check(vkCreateFence(renderer->device, &fenceInfo, nullptr, &framesInFlight[i]));
    }

    Array<VkClearValue> clearColour{};
    clearColour.init(&renderer->stackAllocator, 2, 2);

    GameCamera gameCamera;
    gameCamera.internal3DCamera.initPerspective(0.05f, 1000.f, 45.f, Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(true, 6.f, 2.0f, 0.1f);

    int64_t beginFrameTick = timeNow();
    uint32_t currentFrame = 0;
    float timePassed = 0.f;
    bool fullscreen = false;
    while (Window::instance()->exitRequested == false)
    {
        //Actually does the SDL event pooling
        inputHandler.onEvent();

        if (inputHandler.isKeyDown(Keys::KEY_ESCAPE))
        {
            Window::instance()->exitRequested = true;
        }
        else if (inputHandler.isKeyJustReleased(Keys::KEY_F))
        {
            fullscreen = !fullscreen;
            Window::instance()->setFullscreen(fullscreen);
        }
        else if (inputHandler.isKeyJustReleased(Keys::KEY_R))
        {
            gameCamera.reset();
        }

        if (inputHandler.isButtonJustReleased(GAMEPAD_BUTTON_A))
        {
            Window::instance()->exitRequested = true;
        }

        //Moves key pressed events stores then in a key-pressed array. This allows us to know if a key is being held down, rather than just pressed. 
        inputHandler.newFrame();
        //Saves the mouse position in screen coordinates and handles events that are for re-mapped key bindings 
        inputHandler.update();

        //Begin "physics"
        const int64_t currentTick = timeNow();
        float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
        timePassed += deltaTime;
        beginFrameTick = currentTick;

        gameCamera.update(&inputHandler, Window::instance()->width, Window::instance()->height, deltaTime);
        Window::instance()->centerMouse(inputHandler.isMouseDragging(MouseButtons::MOUSE_BUTTON_RIGHT));

        if (Window::instance()->minimised == false)
        {
            vkWaitForFences(renderer->device, 1, &framesInFlight[currentFrame], VK_TRUE, UINT64_MAX);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(renderer->device, renderer->swapchain, UINT64_MAX, imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex);
            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapchain(renderer);
                continue;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                VOID_ERROR("Failed to acquire swapchain image at image index %d", imageIndex);
            }

            ModelData modelData{};
            modelData.model = glms_rotate(glms_mat4_identity(), timePassed * glm_rad(90.f), { 0.f, 0.f, 1.f });
            modelData.proj = gameCamera.internal3DCamera.viewProjection;
            modelData.proj.m11 *= -1;

            memcpy(renderer->uniformBuffersMapped[currentFrame], &modelData, sizeof(modelData));

            vkResetFences(renderer->device, 1, &framesInFlight[currentFrame]);

            vkResetCommandBuffer(commandBuffers[currentFrame], 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = 0;

            check(vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo));

            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = renderer->renderPass;
            renderPassBeginInfo.framebuffer = renderer->swapchainFramebuffers[imageIndex];
            renderPassBeginInfo.renderArea.offset = { 0, 0 };
            renderPassBeginInfo.renderArea.extent = renderer->swapchainExtent;

            // 0.218f, 0.f, 0.265f, 1.f
            clearColour[0] = { { 0.f, 0.535f, 1.f, 1.f } };
            clearColour[1] = { { 0.f, 0 } };
            renderPassBeginInfo.clearValueCount = clearColour.size;
            renderPassBeginInfo.pClearValues = clearColour.data;

            vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline);

            VkViewport viewport{};
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = float(renderer->swapchainExtent.width);
            viewport.height = float(renderer->swapchainExtent.height);
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = renderer->swapchainExtent;
            vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

            VkBuffer vertexBuffers[] = { modelBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffers[currentFrame], modelBuffer, vertexbufferSize, VK_INDEX_TYPE_UINT16);

            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

            vkCmdDrawIndexed(commandBuffers[currentFrame], uint32_t(indices.size), 1, 0, 0, 0);

            vkCmdEndRenderPass(commandBuffers[currentFrame]);

            check(vkEndCommandBuffer(commandBuffers[currentFrame]));

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { imageAvailableSemaphore[currentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

            VkSemaphore signalSemaphores[] = { renderFinishSemaphore[currentFrame] };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            check(vkQueueSubmit(renderer->mainQueue, 1, &submitInfo, framesInFlight[currentFrame]));

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapchains[] = { renderer->swapchain };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapchains;
            presentInfo.pImageIndices = &imageIndex;

            result = vkQueuePresentKHR(renderer->mainQueue, &presentInfo);
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || Window::instance()->resizeRequested)
            {
                Window::instance()->resizeRequested = false;
                gameCamera.internal3DCamera.setAspectRatio(Window::instance()->width * 1.f / Window::instance()->height);
                recreateSwapchain(renderer);

                currentFrame = (currentFrame + 1) % maxFramesInFlight;
                continue;
            }
            else if (result != VK_SUCCESS)
            {
                VOID_ERROR("Failed or present swapchain image!");
            }

            currentFrame = (currentFrame + 1) % maxFramesInFlight;
        }
    }

    vkDeviceWaitIdle(renderer->device);

#ifdef SKELETON_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer->instance, "vkDestroyDebugUtilsMessengerEXT");
    vkDestroyDebugUtilsMessengerEXT(renderer->instance, vulkanDebugUtilsMessenger, nullptr);
#endif // SKELETON_DEBUG

    cleanupSwapchain(renderer);

    vkDestroySampler(renderer->device, textureSampler, nullptr);
    vkDestroyImageView(renderer->device, textureImageView, nullptr);

    vkDestroyImage(renderer->device, image, nullptr);
    vkFreeMemory(renderer->device, imageMemory, nullptr);

    vkDestroyPipeline(renderer->device, mainPipeline, nullptr);
    vkDestroyPipelineLayout(renderer->device, pipelineLayout, nullptr);
    vkDestroyRenderPass(renderer->device, renderer->renderPass, nullptr);

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        vkDestroyBuffer(renderer->device, renderer->uniformBuffers[i], nullptr);
        vkFreeMemory(renderer->device, renderer->uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(renderer->device, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(renderer->device, descriptorSetLayout, nullptr);

    vkDestroyBuffer(renderer->device, modelBuffer, nullptr);
    vkFreeMemory(renderer->device, modelBufferMemory, nullptr);

    for (uint32_t i = 0; i < maxFramesInFlight; ++i)
    {
        vkDestroySemaphore(renderer->device, imageAvailableSemaphore[i], nullptr);
        vkDestroySemaphore(renderer->device, renderFinishSemaphore[i], nullptr);
        vkDestroyFence(renderer->device, framesInFlight[i], nullptr);
    }

    vkDestroyCommandPool(renderer->device, renderer->transferCommandPool, nullptr);
    vkDestroyCommandPool(renderer->device, renderer->commandPool, nullptr);

    vkDestroyDevice(renderer->device, nullptr);

    SDL_Vulkan_DestroySurface(renderer->instance, renderer->surface, nullptr);

    vkDestroyInstance(renderer->instance, nullptr);

    Window::instance()->shutdown();

    descriptorSets.shutdown();

    renderer->uniformBuffers.shutdown();
    renderer->uniformBuffersMemory.shutdown();
    renderer->uniformBuffersMapped.shutdown();

    renderer->swapchainImageViews.shutdown();
    renderer->swapchainFramebuffers.shutdown();
    renderer->swapchainImages.shutdown();

    vertices.shutdown();
    indices.shutdown();
    layouts.shutdown();

    commandBuffers.shutdown();
    imageAvailableSemaphore.shutdown();
    renderFinishSemaphore.shutdown();
    framesInFlight.shutdown();

    inputHandler.shutdown();
    renderer->resourceManager.shutdown();
    renderer->stackAllocator.shutdown();
    MemoryService::instance()->shutdown();

    return 0;
}