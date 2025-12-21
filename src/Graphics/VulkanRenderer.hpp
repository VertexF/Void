#ifndef VULKAN_RENDERER_HDR
#define VULKAN_RENDERER_HDR

#include <vulkan/vulkan.h>
#if defined(__linux__)
#include <vulkan/vulkan_wayland.h>
#endif //__linux__

#include <cglm/struct/vec2.h>
#include <cglm/struct/vec3.h>

#include "Foundation/Memory.hpp"
#include "Foundation/Array.hpp"
#include "Foundation/ResourceManager.hpp"

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

    static Array<VkVertexInputAttributeDescription> getAttributeDescriptions(StackAllocator& stackAllocator)
    {
        Array<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.init(&stackAllocator, 3, 3);
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
    alignas(16) mat4s proj;
};

struct VulkanRenderer
{
    VkInstance instance;

    HeapAllocator* allocator;
    StackAllocator stackAllocator;

    ResourceManager resourceManager;

    Array<VkImageView> swapchainImageViews;
    Array<VkFramebuffer> swapchainFramebuffers;
    Array<VkImage> swapchainImages;

    VkDevice device;
    VkSwapchainKHR swapchain;

    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;

    float maxAnisotropy;

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

    Array<VkBuffer> uniformBuffers;
    Array<VkDeviceMemory> uniformBuffersMemory;
    Array<void*> uniformBuffersMapped;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
};

static uint32_t findMemoryType(VulkanRenderer* renderer, uint32_t typeFilter, VkMemoryPropertyFlags properties);

static VkCommandPool createCommandPool(VulkanRenderer* renderer, uint32_t queueFamilyIndex);

static VkCommandBuffer beginOneTimeCommandBuffer(VulkanRenderer* renderer, const VkCommandPool& pool);
static void endOneTimeCommandBuffer(VulkanRenderer* renderer, VkCommandBuffer commandBuffer, const VkCommandPool& cmdPool, const VkQueue& queue);

static void createBuffer(VulkanRenderer* renderer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
static void copyBuffer(VulkanRenderer* renderer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
static void copyBufferToImage(VulkanRenderer* renderer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

static bool hasStencilComponent(VkFormat format);

static void transitionImageLayout(VulkanRenderer* renderer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

static void createImage(VulkanRenderer* renderer, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

static void createTextureImage(VulkanRenderer* renderer, const char* path, VkImage& textureImage, VkDeviceMemory& textureImageMemory);
static VkImageView createImageView(VulkanRenderer* renderer, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
static VkImageView createTextureImageView(VulkanRenderer* renderer, const VkImage& image, VkFormat format);

static VkSampler createSampler(VulkanRenderer* renderer);

static void createUniformBuffers(VulkanRenderer* renderer);

static VkFormat findSupportedFormat(VulkanRenderer* renderer, const Array<VkFormat>& candidtes, VkImageTiling tiling, VkFormatFeatureFlags features);

static VkFormat findDepthFormat(VulkanRenderer* renderer);
static void createDepthResources(VulkanRenderer* renderer);

static void cleanupSwapchain(VulkanRenderer* renderer);
static void createSwapchain(VulkanRenderer* renderer);
static void createImageViews(VulkanRenderer* renderer);
static void createFramebuffers(VulkanRenderer* renderer);
static void recreateSwapchain(VulkanRenderer* renderer);

int runGame(VulkanRenderer* renderer);

#endif // !VULKAN_RENDERER_HDR
