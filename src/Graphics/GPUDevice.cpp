#include "GPUDevice.hpp"

#include "CommandBuffer.hpp"

#include "Foundation/Memory.hpp"
#include "Foundation/HashMap.hpp"
#include "Foundation/Process.hpp"
#include "Foundation/File.hpp"
#include "Foundation/Numerics.hpp"

#define VMA_IMPLEMENTATION
#include "vender/vk_mem_alloc.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cctype>

namespace 
{
#define VULKAN_DEBUG_REPORT
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

#if defined(VULKAN_DEBUG_REPORT)
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif//VULKAN_DEBUG_REPORT
    };

    const char* REQUESTED_LAYERS[] =
    {
#if defined(VULKAN_DEBUG_REPORT)
        "VK_LAYER_KHRONOS_validation"
#else
        ""
#endif
    };

#if defined(VULKAN_DEBUG_REPORT)

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
        //__debugbreak();
#elif defined(__LINUX__)
        std::raise(SIGINT);
#endif
    }

    return VK_FALSE;
}

    VkDebugUtilsMessengerCreateInfoEXT createDebugUtilsMessengerInfo()
    {
        VkDebugUtilsMessengerCreateInfoEXT creationInfo{};
        creationInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        creationInfo.pNext = nullptr;
        creationInfo.pfnUserCallback = debugCallback;
        creationInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        creationInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        return creationInfo;
    }
#endif //VULKAN_DEBUG_REPORT

    void checkResult(VkResult result)
    {
        if (result == VK_SUCCESS)
        {
            return;
        }

        vprint("Vulkan error: code: code(%u)", result);
        if (result < 0)
        {
            VOID_ASSERTM(false, "Vulkan error: aborting.");
        }
    }

#define check(result) VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Asset Code %u", result)

    SDL_Window* SDLWindow;

    PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
    PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;

    FlatHashMap<uint64_t, VkRenderPass> renderPassCache;

    size_t UBO_ALIGNMENT = 256;
    size_t SSBO_ALIGNMENT = 256;

    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, bool isDepth)
    {
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

        if (isDepth)
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (TextureFormat::hasStencil(format))
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

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

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    //Resource creation
    void vulkanCreateTexture(GPUDevice& gpu, const TextureCreation& creation, TextureHandle handle, Texture* texture)
    {
        texture->width = creation.width;
        texture->height = creation.height;
        texture->depth = creation.depth;
        texture->mipmaps = creation.mipmaps;
        texture->imageType = creation.imageType;
        texture->imageViewType = creation.imageViewType;
        texture->name = creation.name;
        texture->vkFormat = creation.format;
        texture->sampler = nullptr;
        texture->flags = creation.flags;
        texture->handle = handle;

        //Create the image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.format = texture->vkFormat;
        imageInfo.flags = 0;
        imageInfo.imageType = texture->imageType;
        imageInfo.extent.width = creation.width;
        imageInfo.extent.height = creation.height;
        imageInfo.extent.depth = creation.depth;
        imageInfo.mipLevels = creation.mipmaps;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        const bool isRenderTarget = (creation.flags & TextureFlags::Mask::RENDER_TARGET_MASK) == TextureFlags::Mask::RENDER_TARGET_MASK;
        const bool isComputeUsed = (creation.flags & TextureFlags::Mask::COMPUTE_MASK) == TextureFlags::Mask::COMPUTE_MASK;

        //Default to always readable from shaders.
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

        if (TextureFormat::hasDepthOrStencil(creation.format))
        {
            //Depth/Stencil textures are normally textures you render into.
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else
        {
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageInfo.usage |= isRenderTarget ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
        }

        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo memoryInfo{};
        memoryInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        check(vmaCreateImage(gpu.VMAAllocator, &imageInfo, &memoryInfo, &texture->vkImage, &texture->vmaAllocation, nullptr));

        gpu.setResourceName(VK_OBJECT_TYPE_IMAGE, (uint64_t)(texture->vkImage), creation.name);

        //Create the image view
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = texture->vkImage;
        info.viewType = texture->imageViewType;
        info.format = imageInfo.format;

        if (TextureFormat::hasDepthOrStencil(creation.format))
        {
            info.subresourceRange.aspectMask = TextureFormat::hasDepth(creation.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
        }
        else
        {
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        check(vkCreateImageView(gpu.vulkanDevice, &info, gpu.vulkanAllocationCallbacks, &texture->vkImageView));

        gpu.setResourceName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(texture->vkImageView), creation.name);
        texture->vkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void vulkanFillWriteDescriptorSets(GPUDevice& gpu, const DescriptorSetLayout* descriptorSetLayout, VkDescriptorSet vkDescriptorSet,
                                       VkWriteDescriptorSet* descriptorWrite, VkDescriptorBufferInfo* bufferInfo, VkDescriptorImageInfo* imageInfo,
                                       VkSampler vkDefaultSampler, uint32_t& numResources, const uint32_t* resources, const SamplerHandle* samplers,
                                       const uint16_t* bindings)
    {
        uint32_t usedResources = 0;
        for (uint32_t res = 0; res < numResources; ++res)
        {
            //Binding arrays contains the index into the resource. 
            //I am layout binding to retrieve the correct binding information.
            uint32_t layoutBindingIndex = bindings[res];

            const VkDescriptorSetLayoutBinding& binding = descriptorSetLayout->vkBinding[layoutBindingIndex];

            uint32_t i = usedResources;
            ++usedResources;

            descriptorWrite[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            descriptorWrite[i].dstSet = vkDescriptorSet;
            //Use binding array to get final binding point.
            //const uint32_t bindingPoint = binding.start;
            descriptorWrite[i].dstBinding = binding.binding;
            descriptorWrite[i].dstArrayElement = 0;
            descriptorWrite[i].descriptorCount = 1;

            switch (binding.descriptorType)
            {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
                descriptorWrite[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                TextureHandle textureHandle = { resources[i] };
                Texture* textureData = gpu.accessTexture(textureHandle);

                //Find the proper sampler.
                // TODO: There might be away to improve this by removing the single texture interface.
                imageInfo[i].sampler = vkDefaultSampler;
                if (textureData->sampler)
                {
                    imageInfo[i].sampler = textureData->sampler->vkSampler;
                }

                if (samplers[res].index != INVALID_INDEX)
                {
                    SamplerHandle samplerHandle{ samplers[res] };
                    Sampler* sampler = gpu.accessSampler(samplerHandle);
                    imageInfo[i].sampler = sampler->vkSampler;
                }

                imageInfo[i].imageLayout = TextureFormat::hasDepthOrStencil(textureData->vkFormat) ?
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo[i].imageView = textureData->vkImageView;

                descriptorWrite[i].pImageInfo = &imageInfo[i];
                break;
            }
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                descriptorWrite[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle textureHandle = { resources[res] };
                Texture* textureData = gpu.accessTexture(textureHandle);

                imageInfo[i].sampler = nullptr;
                imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfo[i].imageView = textureData->vkImageView;

                descriptorWrite[i].pImageInfo = &imageInfo[i];
                break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                BufferHandle bufferHandle = { resources[res] };
                Buffer* buffer = gpu.accessBuffer(bufferHandle);

                descriptorWrite[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite[i].descriptorType = buffer->usage == ResourceType::Type::DYNAMIC ?
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC :
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                //Bind parent buffer if present, used for dynamic resource.
                if (buffer->parentBuffer.index != INVALID_INDEX)
                {
                    Buffer* parentBuffer = gpu.accessBuffer(buffer->parentBuffer);
                    bufferInfo[i].buffer = parentBuffer->vkBuffer;
                }
                else
                {
                    bufferInfo[i].buffer = buffer->vkBuffer;
                }

                bufferInfo[i].offset = 0;
                bufferInfo[i].range = buffer->size;

                descriptorWrite[i].pBufferInfo = &bufferInfo[i];

                break;
            }
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                BufferHandle bufferHandle = { resources[res] };
                Buffer* buffer = gpu.accessBuffer(bufferHandle);

                descriptorWrite[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                //Bind parent buffer if present, used for dynamic resouces.
                if (buffer->parentBuffer.index != INVALID_INDEX)
                {
                    Buffer* parentBuffer = gpu.accessBuffer(buffer->parentBuffer);

                    bufferInfo[i].buffer = parentBuffer->vkBuffer;
                }
                else
                {
                    bufferInfo[i].buffer = buffer->vkBuffer;
                }

                bufferInfo[i].offset = 0;
                bufferInfo[i].range = buffer->size;

                descriptorWrite[i].pBufferInfo = &bufferInfo[i];
                break;
            }
            default:
                VOID_ASSERTM(false, "Resource type %d not supported in descriptor set creation.\n", binding.descriptorType);
                break;
            }
        }

        numResources = usedResources;
    }

    void vulkanCreateSwapchainPass(GPUDevice& gpu, const RenderPassCreation& creation, RenderPass* renderPass)
    {
        VkAttachmentDescription colourAttachment{};
        colourAttachment.format = gpu.vulkanSurfaceFormat.format;
        colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colourAttachmentRef{};
        colourAttachmentRef.attachment = 0;
        colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //Depth attachment
        VkAttachmentDescription depthAttachment{};
        Texture* depthTextureVK = gpu.accessTexture(gpu.depthTexture);
        depthAttachment.format = depthTextureVK->vkFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colourAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkAttachmentDescription attachments[] = { colourAttachment, depthAttachment };
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 2;
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        check(vkCreateRenderPass(gpu.vulkanDevice, &renderPassInfo, nullptr, &renderPass->vkRenderPass));

        gpu.setResourceName(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<uint64_t>(renderPass->vkRenderPass), creation.name);

        //Create framebuffer info device.
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->vkRenderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.width = gpu.swapchainWidth;
        framebufferInfo.height = gpu.swapchainHeight;
        framebufferInfo.layers = 1;

        VkImageView framebufferAttachments[2];
        framebufferAttachments[1] = depthTextureVK->vkImageView;

        for (size_t i = 0; i < gpu.vulkanSwapchainImageCount; ++i)
        {
            framebufferAttachments[0] = gpu.vulkanSwapchainImageViews[i];
            framebufferInfo.pAttachments = framebufferAttachments;

            vkCreateFramebuffer(gpu.vulkanDevice, &framebufferInfo, nullptr, &gpu.vulkanSwapchainFramebuffers[i]);
            gpu.setResourceName(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<uint64_t>(gpu.vulkanSwapchainFramebuffers[i]), creation.name);
        }

        renderPass->width = gpu.swapchainWidth;
        renderPass->height = gpu.swapchainHeight;

        //Manually transition the texture
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        CommandBuffer* commandBuffer = gpu.getInstantCommandBuffer();
        vkBeginCommandBuffer(commandBuffer->vkCommandBuffer, &beginInfo);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { gpu.swapchainWidth, gpu.swapchainHeight };

        //Transition
        for (size_t i = 0; i < gpu.vulkanSwapchainImageCount; ++i)
        {
            transitionImageLayout(commandBuffer->vkCommandBuffer, gpu.vulkanSwapchainImages[i], gpu.vulkanSurfaceFormat.format,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false);
        }

        vkEndCommandBuffer(commandBuffer->vkCommandBuffer);

        //Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer->vkCommandBuffer;

        vkQueueSubmit(gpu.vulkanQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(gpu.vulkanQueue);
    }

    void vulkanCreateFramebuffer(GPUDevice& gpu, RenderPass* renderPass, const TextureHandle* outputTexture,
                                 uint32_t numRenderTargets, TextureHandle depthStencilTexture)
    {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->vkRenderPass;
        framebufferInfo.width = renderPass->width;
        framebufferInfo.height = renderPass->height;
        framebufferInfo.layers = 1;

        VkImageView framebufferAttachments[MAX_IMAGE_OUTPUT + 1]{};
        uint32_t activeAttachments = 0;
        for (; activeAttachments < numRenderTargets; ++activeAttachments)
        {
            Texture* textureVK = gpu.accessTexture(outputTexture[activeAttachments]);
            framebufferAttachments[activeAttachments] = textureVK->vkImageView;
        }

        if (depthStencilTexture.index != INVALID_INDEX)
        {
            Texture* depthTextureVK = gpu.accessTexture(depthStencilTexture);
            framebufferAttachments[activeAttachments++] = depthTextureVK->vkImageView;
        }
        framebufferInfo.pAttachments = framebufferAttachments;
        framebufferInfo.attachmentCount = activeAttachments;

        vkCreateFramebuffer(gpu.vulkanDevice, &framebufferInfo, nullptr, &renderPass->vkFrameBuffer);
        gpu.setResourceName(VK_OBJECT_TYPE_FRAMEBUFFER, reinterpret_cast<uint64_t>(renderPass->vkFrameBuffer), renderPass->name);
    }

    VkRenderPass vulkanCreateRenderPass(GPUDevice& gpu, const RenderPassOutput& output, const char* name)
    {
        VkAttachmentDescription colourAttachments[8]{};
        VkAttachmentReference colourAttachmentsRef[8]{};

        VkAttachmentLoadOp colourOP;
        VkAttachmentLoadOp depthOP;
        VkAttachmentLoadOp stencilOP;

        VkImageLayout colourInitial;
        VkImageLayout depthInitial;

        switch (output.colourOperations)
        {
        case RenderPassType::Operations::LOAD:
            colourOP = VK_ATTACHMENT_LOAD_OP_LOAD;
            colourInitial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassType::Operations::CLEAR:
            colourOP = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colourInitial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        default:
            colourOP = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colourInitial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        }

        switch (output.depthOperations)
        {
        case RenderPassType::LOAD:
            depthOP = VK_ATTACHMENT_LOAD_OP_LOAD;
            depthInitial = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassType::CLEAR:
            depthOP = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthInitial = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        default:
            depthOP = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthInitial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        }

        switch (output.stencilOperations)
        {
        case RenderPassType::LOAD:
            stencilOP = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case RenderPassType::CLEAR:
            stencilOP = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        default:
            stencilOP = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
        }

        //Colour attachment setup
        uint32_t colourFormat = 0;
        for (; colourFormat < output.numColourFormats; ++colourFormat)
        {
            VkAttachmentDescription& colourAttachment = colourAttachments[colourFormat];
            colourAttachment.format = output.colourFormats[colourFormat];
            colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colourAttachment.loadOp = colourOP;
            colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colourAttachment.stencilLoadOp = stencilOP;
            colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colourAttachment.initialLayout = colourInitial;
            colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference& colourAttachmentRef = colourAttachmentsRef[colourFormat];
            colourAttachmentRef.attachment = colourFormat;
            colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        //Depth attachment setup
        VkAttachmentDescription depthAttachment{};
        VkAttachmentReference depthAttachmentRef{};

        if (output.depthStencilFormat != VK_FORMAT_UNDEFINED)
        {
            depthAttachment.format = output.depthStencilFormat;
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = depthOP;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.stencilLoadOp = stencilOP;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = depthInitial;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depthAttachmentRef.attachment = colourFormat;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        //Subpass setup
        //TODO: For now this is just a simpler subpass we might want to do more with it.
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        //Calculate the active attachments for the subpass.
        VkAttachmentDescription attachments[MAX_IMAGE_OUTPUT + 1]{};
        uint32_t activeAttachments = 0;
        for (; activeAttachments < output.numColourFormats; ++activeAttachments)
        {
            attachments[activeAttachments] = colourAttachments[activeAttachments];
            ++activeAttachments;
        }
        subpass.colorAttachmentCount = activeAttachments ? activeAttachments - 1 : 0;
        subpass.pColorAttachments = colourAttachmentsRef;

        subpass.pDepthStencilAttachment = nullptr;

        uint32_t depthStencilCount = 0;
        if (output.depthStencilFormat != VK_FORMAT_UNDEFINED)
        {
            attachments[subpass.colorAttachmentCount] = depthAttachment;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
            depthStencilCount = 1;
        }

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = (activeAttachments ? activeAttachments - 1 : 0) + depthStencilCount;
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VkRenderPass vkRenderPass;
        //TODO: Check that to make sure we don't need the vulkan allocation callbacks here.
        check(vkCreateRenderPass(gpu.vulkanDevice, &renderPassInfo, nullptr, &vkRenderPass));

        gpu.setResourceName(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<uint64_t>(vkRenderPass), name);

        return vkRenderPass;
    }

    RenderPassOutput fillRenderPassOutput(GPUDevice& gpu, const RenderPassCreation& creation)
    {
        RenderPassOutput output;
        output.reset();

        for (uint32_t i = 0; i < creation.numRenderTargets; ++i)
        {
            Texture* textureVK = gpu.accessTexture(creation.outputTextures[i]);
            output.colour(textureVK->vkFormat);
        }

        if (creation.depthStencilTexture.index != INVALID_INDEX)
        {
            Texture* textureVK = gpu.accessTexture(creation.depthStencilTexture);
            output.depth(textureVK->vkFormat);
        }

        output.colourOperations = creation.colourOperations;
        output.depthOperations = creation.depthOperations;
        output.stencilOperations = creation.stencilOperations;

        return output;
    }

    void vulkanResizeTexture(GPUDevice gpu, Texture* texture, Texture* textureToDelete, uint16_t width, uint16_t height, uint16_t depth)
    {
        //The caching handles destroying this texture.
        textureToDelete->vkImageView = texture->vkImageView;
        textureToDelete->vkImage = texture->vkImage;
        textureToDelete->vmaAllocation = texture->vmaAllocation;

        //Re-create image in place
        TextureCreation textureCreation;
        textureCreation.setFlags(texture->mipmaps, texture->flags)
                       .setFormatType(texture->vkFormat, texture->imageType, texture->imageViewType)
                       .setName(texture->name)
                       .setSize(width, height, depth);
        vulkanCreateTexture(gpu, textureCreation, texture->handle, texture);
    }

    VkPresentModeKHR toVkPresentMode(PresentMode::Types mode)
    {
        switch (mode)
        {
        case PresentMode::Types::VSYNC_FAST:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        case PresentMode::Types::VSYNC_RELAXED:
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        case PresentMode::Types::IMMEDIATE:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case PresentMode::Types::VSYNC:
        default:
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }

}//Anon

struct CommandBufferRing
{
    void init(GPUDevice* newGPU);
    void shutdown();

    void resetPools(uint32_t frameIndex);

    CommandBuffer* getCommandBuffer(uint32_t frame, bool begin);
    CommandBuffer* getCommandBufferInstant(uint32_t frame, bool begin);

    static uint16_t poolFromIndex(uint32_t index);

    GPUDevice* gpu = nullptr;
    Array<VkCommandPool> vulkanCommandPools;
    Array<CommandBuffer> commandBuffers;
    Array<uint8_t> nextFreePerThreadFrame;

    static constexpr uint16_t MAX_THREADS = 1;
    static constexpr uint16_t BUFFER_PER_POOL = 4;
    uint8_t imageThreadCount = 0;
    uint8_t commandBufferCount = 0;
};
static CommandBufferRing commandBufferRing;

void CommandBufferRing::init(GPUDevice* newGPU)
{
    gpu = newGPU;

    imageThreadCount = MAX_THREADS * gpu->vulkanSwapchainImageCount;
    commandBufferCount = imageThreadCount * BUFFER_PER_POOL;
    
    vulkanCommandPools.init(gpu->allocator, imageThreadCount, imageThreadCount);
    commandBuffers.init(gpu->allocator, commandBufferCount, commandBufferCount);
    nextFreePerThreadFrame.init(gpu->allocator, imageThreadCount, imageThreadCount);

    for (uint32_t i = 0; i < imageThreadCount; ++i)
    {
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.pNext = nullptr;
        cmdPoolInfo.queueFamilyIndex = gpu->vulkanQueueFamily;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        check(vkCreateCommandPool(gpu->vulkanDevice, &cmdPoolInfo, gpu->vulkanAllocationCallbacks, &vulkanCommandPools[i]));
    }

    for (uint32_t i = 0; i < commandBufferCount; ++i)
    {
        VkCommandBufferAllocateInfo cmd{};
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd.pNext = nullptr;

        const uint32_t poolNext = poolFromIndex(i);
        cmd.commandPool = vulkanCommandPools[poolNext];
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(gpu->vulkanDevice, &cmd, &commandBuffers[i].vkCommandBuffer));

        commandBuffers[i].device = gpu;
        commandBuffers[i].handle = i;
        commandBuffers[i].reset();
    }
}

void CommandBufferRing::shutdown()
{
    for (uint32_t i = 0; i < imageThreadCount; ++i)
    {
        vkDestroyCommandPool(gpu->vulkanDevice, vulkanCommandPools[i], gpu->vulkanAllocationCallbacks);
    }

    vulkanCommandPools.shutdown();
    commandBuffers.shutdown();
    nextFreePerThreadFrame.shutdown();
}

void CommandBufferRing::resetPools(uint32_t frameIndex)
{
    for (uint32_t i = 0; i < MAX_THREADS; ++i)
    {
        vkResetCommandPool(gpu->vulkanDevice, vulkanCommandPools[frameIndex * MAX_THREADS + i], 0);
    }
}

CommandBuffer* CommandBufferRing::getCommandBuffer(uint32_t frame, bool begin)
{
    //TODO: Threads aren't handled for this function that's why we are have 1 thread for the command pool set up.
    CommandBuffer* commandBuffer = &commandBuffers[frame * BUFFER_PER_POOL];

    if (begin)
    {
        commandBuffer->reset();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer->vkCommandBuffer, &beginInfo);
    }

    return commandBuffer;
}

CommandBuffer* CommandBufferRing::getCommandBufferInstant(uint32_t frame, bool begin)
{
    CommandBuffer* commandBuffer = &commandBuffers[frame * BUFFER_PER_POOL + 1];
    return commandBuffer;
}

uint16_t CommandBufferRing::poolFromIndex(uint32_t index)
{
    return static_cast<uint16_t>(index) / BUFFER_PER_POOL;
}

void GPUTimestampManager::init(Allocator* newAllocator, uint16_t newQueriesPerFrame, uint16_t maxFrame)
{
    allocator = newAllocator;
    queriesPerFrame = newQueriesPerFrame;

    //Data is start, end in 2 uint64_t numbers.
    const uint32_t dataDataPerQuery = 2;
    const size_t allocatorSize = sizeof(GPUTimestamp) * queriesPerFrame * maxFrame + sizeof(uint64_t) * queriesPerFrame * maxFrame * dataDataPerQuery;
    uint8_t* memory = void_allocam(allocatorSize, allocator);

    timestamps = reinterpret_cast<GPUTimestamp*>(memory);
    //Data is start, end in 2 uint64_t numbers.
    timestampsData = reinterpret_cast<uint64_t*>(memory + sizeof(GPUTimestamp) * queriesPerFrame * maxFrame);

    reset();
}

void GPUTimestampManager::shutdown()
{
    void_free(timestamps, allocator);
}

bool GPUTimestampManager::hasValidQueries() const
{
    //Even number of queries asymettrical queries, thus we don't sample.
    return currentQuery > 0 && (depth == 0);
}

void GPUTimestampManager::reset()
{
    currentQuery = 0;
    parentIndex = 0;
    currentFrameResolved = false;
    depth = 0;
}

//Returns the total queries for this frame.
uint32_t GPUTimestampManager::resolve(uint32_t currentFrame, GPUTimestamp* timestampsToFill)
{
    memoryCopy(timestampsToFill, &timestamps[currentFrame * queriesPerFrame], sizeof(GPUTimestamp) * currentQuery);
    return currentQuery;
}

//Returns the timestamp query index.
uint32_t GPUTimestampManager::push(uint32_t currentFrame, const char* name)
{
    uint32_t queryIndex = (currentFrame * queriesPerFrame) + currentQuery;

    GPUTimestamp& timestamp = timestamps[queryIndex];
    timestamp.parentIndex = static_cast<uint16_t>(parentIndex);
    timestamp.start = queryIndex * 2;
    timestamp.end = timestamp.start + 1;
    timestamp.name = name;
    timestamp.depth = static_cast<uint16_t>(depth++);

    parentIndex = currentQuery;
    ++currentQuery;

    return (queryIndex * 2);
}

uint32_t GPUTimestampManager::pop(uint32_t currentFrame)
{
    uint32_t queryIndex = (currentFrame * queriesPerFrame) + parentIndex;
    GPUTimestamp& timestamp = timestamps[queryIndex];
    //Go up a level
    parentIndex = timestamp.parentIndex;
    --depth;

    return (queryIndex * 2) + 1;
}

DeviceCreation& DeviceCreation::setWindow(uint32_t newWidth, uint32_t newHeight, void* handle)
{
    width = static_cast<uint16_t>(newWidth);
    height = static_cast<uint16_t>(newHeight);
    window = handle;

    return *this;
}

DeviceCreation& DeviceCreation::setAllocator(Allocator* newAllocator)
{
    allocator = newAllocator;
    return *this;
}

DeviceCreation& DeviceCreation::setLinearAllocator(StackAllocator* alloc)
{
    tempAllocator = alloc;
    return *this;
}

GPUDevice GPUDevice::instance()
{
    static GPUDevice instance;
    return instance;
}

//Init/shutdown
void GPUDevice::init(const DeviceCreation& creation)
{
    vprint("GPU Device init.\n");
    allocator = creation.allocator;
    tempAllocator = creation.tempAllocator;
    stringBuffer.init(1024 * 1024, creation.allocator);

    VkResult result;
    vulkanAllocationCallbacks = nullptr;

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = nullptr;
    applicationInfo.pApplicationName = "Void Game";
    applicationInfo.applicationVersion = 1;
    applicationInfo.pEngineName = "Void Engine";
    applicationInfo.apiVersion = VK_MAKE_VERSION(1, 3, 0);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &applicationInfo;
#if defined(VULKAN_DEBUG_REPORT)
    createInfo.enabledLayerCount = ArraySize(REQUESTED_LAYERS);
    createInfo.ppEnabledLayerNames = REQUESTED_LAYERS;
#else
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
#endif //VULKAN_DEBUG_REPORT
    createInfo.enabledExtensionCount = ArraySize(REQUESTED_EXTENSIONS);
    createInfo.ppEnabledExtensionNames = REQUESTED_EXTENSIONS;

#if defined(VULKAN_DEBUG_REPORT)
    const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = createDebugUtilsMessengerInfo();

#if defined(VULKAN_SYNCHRONIZATION_VALIDATION)
    const VkValidationFeatureEnableEXT featuresRequested[] =
    {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };

    VkValidationFeaturesEXT features{};
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.pNext = nullptr;
    features.enabledValidationFeatureCount = sizeof(featuresRequested) / sizeof(featuresRequested[0]);
    features.pEnabledValidationFeatures = featuresRequested;
    createInfo.pNext = &features;
#else
    createInfo.pNext = &debugCreateInfo;
#endif //VULKAN_SYNCHRONIZATION_VALIDATION
#endif //VULKAN_DEBUG_REPORT

    result = vkCreateInstance(&createInfo, vulkanAllocationCallbacks, &vulkanInstance);
    check(result);

    swapchainWidth = creation.width;
    swapchainHeight = creation.height;

#if defined(VULKAN_DEBUG_REPORT)
    uint32_t numInstanceExtensions;
    vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExtensions, nullptr);
    VkExtensionProperties* extensions = reinterpret_cast<VkExtensionProperties*>(void_alloca(sizeof(VkExtensionProperties) * numInstanceExtensions, allocator));
    vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExtensions, extensions);

    for (size_t i = 0; i < numInstanceExtensions; ++i)
    {
        if (strcmp(extensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == false)
        {
            debugUtilsExtensionPresent = true;
            break;
        }
    }

    void_free(extensions, allocator);

    if (debugUtilsExtensionPresent == false)
    {
        vprint("Extension %s for debugging non present.", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    else
    {
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vulkanInstance, "vkCreateDebugUtilsMessengerEXT");
        VkDebugUtilsMessengerCreateInfoEXT debugMessenegerCreateInfo = createDebugUtilsMessengerInfo();

        vkCreateDebugUtilsMessengerEXT(vulkanInstance, &debugMessenegerCreateInfo, vulkanAllocationCallbacks, &vulkanDebugUtilsMessenger);
    }
#endif //VULKAN_DEBUG_REPORT

    //Physical device
    uint32_t numPhysicalDevice;
    result = vkEnumeratePhysicalDevices(vulkanInstance, &numPhysicalDevice, nullptr);
    check(result);

    VkPhysicalDevice* gpus = reinterpret_cast<VkPhysicalDevice*>(void_alloca(sizeof(VkPhysicalDevice) * numPhysicalDevice, allocator));
    result = vkEnumeratePhysicalDevices(vulkanInstance, &numPhysicalDevice, gpus);
    check(result);

    //Surface creation
    SDL_Window* window = reinterpret_cast<SDL_Window*>(creation.window);
    if (SDL_Vulkan_CreateSurface(window, vulkanInstance, vulkanAllocationCallbacks, &vulkanWindowSurface) == false)
    {
        vprint("Failed to create Vulkan surface.\n");
    }

    SDLWindow = window;

    VkPhysicalDevice discreteGPU = VK_NULL_HANDLE;
    VkPhysicalDevice integrateGPU = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < numPhysicalDevice; ++i)
    {
        VkPhysicalDevice physicalDevice = gpus[i];
        vkGetPhysicalDeviceProperties(physicalDevice, &vulkanPhysicalProperties);

        if (vulkanPhysicalProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            if (getFamilyQueue(physicalDevice))
            {
                discreteGPU = physicalDevice;
            }

            continue;
        }


        if (vulkanPhysicalProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            if (getFamilyQueue(physicalDevice))
            {
                integrateGPU = physicalDevice;
            }

            continue;
        }
    }

    if (discreteGPU != VK_NULL_HANDLE)
    {
        vulkanPhysicalDevice = discreteGPU;
    }
    else if (integrateGPU != VK_NULL_HANDLE)
    {
        vulkanPhysicalDevice = integrateGPU;
    }
    else
    {
        VOID_ASSERTM(false, "No suitable GPU found.");
    }

    void_free(gpus, allocator);

    vkGetPhysicalDeviceProperties(vulkanPhysicalDevice, &vulkanPhysicalProperties);
    gpuTimestampFrequency = vulkanPhysicalProperties.limits.timestampPeriod / (1000 * 1000);

    vprint("GPU Used: %s\n", vulkanPhysicalProperties.deviceName);

    UBO_ALIGNMENT = vulkanPhysicalProperties.limits.minUniformBufferOffsetAlignment;
    SSBO_ALIGNMENT = vulkanPhysicalProperties.limits.minStorageBufferOffsetAlignment;

    //Get the logical device
    uint32_t deviceExtensionCount = 1;
    const char* deviceExtensions[] = { "VK_KHR_swapchain" };
    const float queuePriority[] = { 1.f };
    VkDeviceQueueCreateInfo queueInfo[1]{};
    queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo[0].queueFamilyIndex = vulkanQueueFamily;
    queueInfo[0].queueCount = 1;
    queueInfo[0].pQueuePriorities = queuePriority;

    //Enable all features
    VkPhysicalDeviceFeatures2 physicalDeviceFeature2{};
    physicalDeviceFeature2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(vulkanPhysicalDevice, &physicalDeviceFeature2);

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = sizeof(queueInfo) / sizeof(queueInfo[0]);
    deviceCreateInfo.pQueueCreateInfos = queueInfo;
    deviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    deviceCreateInfo.pNext = &physicalDeviceFeature2;

    result = vkCreateDevice(vulkanPhysicalDevice, &deviceCreateInfo, vulkanAllocationCallbacks, &vulkanDevice);
    check(result);

    //Get the functions pointers to debug util functions.
    if (debugUtilsExtensionPresent)
    {
        pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(vulkanDevice, "vkSetDebugUtilsObjectNameEXT");
        pfnCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(vulkanDevice, "vkCmdBeginDebugUtilsLabelEXT");
        pfnCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(vulkanDevice, "vkCmdEndDebugUtilsLabelEXT");
    }

    vkGetDeviceQueue(vulkanDevice, vulkanQueueFamily, 0, &vulkanQueue);

    //Create framebuffers
    int windowWidth;
    int windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    //Select surface format
    const VkFormat surfaceImageFormats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR surfaceColourSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    uint32_t supportCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanWindowSurface, &supportCount, nullptr);
    VkSurfaceFormatKHR* supportedFormats = reinterpret_cast<VkSurfaceFormatKHR*>(void_alloca(sizeof(VkSurfaceFormatKHR) * supportCount, allocator));
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanWindowSurface, &supportCount, supportedFormats);

    //Cache render
    swapchainOutput.reset();

    //Check for supported formats
    bool formatFound = false;
    const uint32_t surfaceFormatCount = ArraySize(surfaceImageFormats);

    for (uint32_t i = 0; i < surfaceFormatCount; ++i)
    {
        for (uint32_t j = 0; j < supportCount; ++j)
        {
            if (supportedFormats[j].format == surfaceImageFormats[i] && supportedFormats[j].colorSpace == surfaceColourSpace)
            {
                vulkanSurfaceFormat = supportedFormats[j];
                formatFound = true;
                break;
            }
        }

        if (formatFound)
        {
            break;
        }
    }

    //Default to the first format supported.
    if (formatFound == false)
    {
        vulkanSurfaceFormat = supportedFormats[0];
        VOID_ASSERT(false);
    }

    void_free(supportedFormats, allocator);

    swapchainOutput.colour(vulkanSurfaceFormat.format);

    setPresentMode(presentMode);
    //Create swapchain
    createSwapchain();

    //Create VMA Allocator
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = vulkanPhysicalDevice;
    allocatorInfo.device = vulkanDevice;
    allocatorInfo.instance = vulkanInstance;

    result = vmaCreateAllocator(&allocatorInfo, &VMAAllocator);
    check(result);

    //Create the pools.
    static const uint32_t GLOBAL_POOL_ELEMENTS = 128;
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, GLOBAL_POOL_ELEMENTS },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, GLOBAL_POOL_ELEMENTS }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = GLOBAL_POOL_ELEMENTS * ArraySize(poolSizes);
    poolInfo.poolSizeCount = static_cast<uint32_t>(ArraySize(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
    result = vkCreateDescriptorPool(vulkanDevice, &poolInfo, vulkanAllocationCallbacks, &vulkanDescriptorPool);
    check(result);

    //Create timestamp query pool used for GPU timings.
    VkQueryPoolCreateInfo vkQueryPoolInfo{};
    vkQueryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    vkQueryPoolInfo.pNext = nullptr;
    vkQueryPoolInfo.flags = 0;
    vkQueryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    vkQueryPoolInfo.queryCount = creation.GPUTimeQueriesPerFrame * 2u * vulkanSwapchainImageCount;
    vkQueryPoolInfo.pipelineStatistics = 0;
    result = vkCreateQueryPool(vulkanDevice, &vkQueryPoolInfo, vulkanAllocationCallbacks, &vulkanTimestampQueryPool);

    buffers.init(allocator, 4096, sizeof(Buffer));
    textures.init(allocator, 512, sizeof(Texture));
    renderPasses.init(allocator, 256, sizeof(RenderPass));
    descriptorSetLayouts.init(allocator, 128, sizeof(DescriptorSetLayout));
    pipelines.init(allocator, 128, sizeof(Pipeline));
    shaders.init(allocator, 128, sizeof(ShaderState));
    descriptorSets.init(allocator, 256, sizeof(DescriptorSet));
    samplers.init(allocator, 32, sizeof(Sampler));

    //Init render frame informations. This includes fences, semaphores and command buffers.
    //TODO: memory allocate memory of all the Device render frame stuff.
    uint8_t* memory = void_allocam(sizeof(GPUTimestampManager) + sizeof(CommandBuffer*) * 128, allocator);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vulkanImageAcquiredSemaphore.init(allocator, vulkanSwapchainImageCount, vulkanSwapchainImageCount);
    vulkanRenderCompleteSemaphore.init(allocator, vulkanSwapchainImageCount, vulkanSwapchainImageCount);
    vulkanCommandBufferExecutedFence.init(allocator, vulkanSwapchainImageCount, vulkanSwapchainImageCount);

    for (uint32_t i = 0; i < vulkanSwapchainImageCount; ++i)
    {
        vkCreateSemaphore(vulkanDevice, &semaphoreInfo, vulkanAllocationCallbacks, &vulkanImageAcquiredSemaphore[i]);
        vkCreateSemaphore(vulkanDevice, &semaphoreInfo, vulkanAllocationCallbacks, &vulkanRenderCompleteSemaphore[i]);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(vulkanDevice, &fenceInfo, vulkanAllocationCallbacks, &vulkanCommandBufferExecutedFence[i]);
    }

    gpuTimestampManager = reinterpret_cast<GPUTimestampManager*>(memory);
    gpuTimestampManager->init(allocator, creation.GPUTimeQueriesPerFrame, vulkanSwapchainImageCount);

    commandBufferRing.init(this);

    //Allocate queued command buffers array
    queuedCommandBuffers = reinterpret_cast<CommandBuffer**>(gpuTimestampManager + 1);
    CommandBuffer** correctlyAllocatedBuffer = reinterpret_cast<CommandBuffer**>(memory + sizeof(GPUTimestampManager));
    VOID_ASSERTM(queuedCommandBuffers == correctlyAllocatedBuffer, "Wrong calculations for queue command buffers arrays. Should be %p, but it's %p",
        correctlyAllocatedBuffer, queuedCommandBuffers);

    vulkanImageIndex = 0;
    currentFrame = 1;
    previousFrame = 0;
    absoluteFrame = 0;
    timestampsEnabled = false;

    resourceDeletionQueue.init(allocator, 16);
    descriptorSetUpdates.init(allocator, 16);

    //Init primitive resource.
    SamplerCreation samplerCreaion{};
    samplerCreaion.setAddressModeUVW(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
        .setMinMagMip(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .setName("Sampler Default");
    defaultSampler = createSampler(samplerCreaion);

    BufferCreation fullscreenBufferVbCreation{};
    fullscreenBufferVbCreation.typeFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    fullscreenBufferVbCreation.usage = ResourceType::Type::IMMUTABLE;
    fullscreenBufferVbCreation.size = 0;
    fullscreenBufferVbCreation.initialData = nullptr;
    fullscreenBufferVbCreation.name = "FullscreenVB";
    fullscreenVertexBuffer = createBuffer(fullscreenBufferVbCreation);

    //Create depth image
    TextureCreation depthTextureCreation{};
    depthTextureCreation.initialData = nullptr;
    depthTextureCreation.width = swapchainWidth;
    depthTextureCreation.height = swapchainHeight;
    depthTextureCreation.depth = 1;
    depthTextureCreation.mipmaps = 1;
    depthTextureCreation.flags = 0;
    depthTextureCreation.format = VK_FORMAT_D32_SFLOAT;
    depthTextureCreation.imageType = VK_IMAGE_TYPE_2D;
    depthTextureCreation.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    depthTextureCreation.name = "DepthImage_Texture";
    depthTexture = createTexture(depthTextureCreation);

    swapchainOutput.depth(VK_FORMAT_D32_SFLOAT);

    RenderPassCreation swapchainPassCreation{};
    swapchainPassCreation.setType(RenderPassType::Types::SWAPCHAIN).setName("Swapchain");
    swapchainPassCreation.setOperations(RenderPassType::Operations::CLEAR,
        RenderPassType::Operations::CLEAR,
        RenderPassType::Operations::CLEAR);
    swapchainPass = createRenderPass(swapchainPassCreation);

    //Init the dummy resources
    TextureCreation dummyTextureCreation{};
    dummyTextureCreation.initialData = nullptr;
    dummyTextureCreation.width = 1;
    dummyTextureCreation.height = 1;
    dummyTextureCreation.depth = 1;
    dummyTextureCreation.mipmaps = 1;
    dummyTextureCreation.flags = 0;
    dummyTextureCreation.format = VK_FORMAT_R8_UINT;
    dummyTextureCreation.imageType = VK_IMAGE_TYPE_2D;
    dummyTextureCreation.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    dummyTexture = createTexture(dummyTextureCreation);

    BufferCreation dummyConstantBufferCreation{};
    dummyConstantBufferCreation.typeFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    dummyConstantBufferCreation.usage = ResourceType::Type::IMMUTABLE;
    dummyConstantBufferCreation.size = 16;
    dummyConstantBufferCreation.initialData = nullptr;
    dummyConstantBufferCreation.name = "DummyCB";
    dummyConstantBuffer = createBuffer(dummyConstantBufferCreation);

    //Time to get the binary paths
#if defined(_MSC_VER)
    char* vulkanEnviroment = stringBuffer.reserve(512);
    ExpandEnvironmentStringsA("%VULKAN_SDK%", vulkanEnviroment, 512);
    char* compilerPath = stringBuffer.appendUseF("%s\\Bin\\", vulkanEnviroment);
#else
    char* vulkanEnviroment = getenv("VULKAN_SDK");
    char* compilerPath = stringBuffer.appendUseF("%s/bin/", vulkanEnviroment);
#endif //_MSC_VER

    strcpy(vulkanBinariesPath, compilerPath);
    stringBuffer.clear();

    //Dynamic buffer handling
    dynamicPerFrameSize = 1024 * 1024 * 10;
    BufferCreation buffCreation{};
    buffCreation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        ResourceType::Type::IMMUTABLE, dynamicPerFrameSize * vulkanSwapchainImageCount).setName("Dynamic_Persistent_Buffer");
    dynamicBuffer = createBuffer(buffCreation);

    MapBufferParameters cbMap{};
    cbMap.buffer = dynamicBuffer;
    cbMap.offset = 0;
    cbMap.size = 0;

    dynamicMappedMemory = reinterpret_cast<uint8_t*>(mapBuffer(cbMap));

    //Init the render pass cache
    renderPassCache.init(allocator, 16);
}


void GPUDevice::shutdown()
{
    vkDeviceWaitIdle(vulkanDevice);
    commandBufferRing.shutdown();

    for (uint32_t i = 0; i < vulkanSwapchainImageCount; ++i)
    {
        vkDestroySemaphore(vulkanDevice, vulkanImageAcquiredSemaphore[i], vulkanAllocationCallbacks);
        vkDestroySemaphore(vulkanDevice, vulkanRenderCompleteSemaphore[i], vulkanAllocationCallbacks);
        vkDestroyFence(vulkanDevice, vulkanCommandBufferExecutedFence[i], vulkanAllocationCallbacks);
    }

    vulkanRenderCompleteSemaphore.shutdown();
    vulkanImageAcquiredSemaphore.shutdown();
    vulkanCommandBufferExecutedFence.shutdown();

    gpuTimestampManager->shutdown();

    MapBufferParameters cbMap{};
    cbMap.buffer = dynamicBuffer;
    cbMap.offset = 0;
    cbMap.size = 0;
    unmapBuffer(cbMap);

    //Memory: this contains allocation for GPU timestamp memory, queued command buffers and render frames.
    void_free(gpuTimestampManager, allocator);

    destroyTexture(depthTexture);
    destroyBuffer(fullscreenVertexBuffer);
    destroyBuffer(dynamicBuffer);
    destroyRenderPass(swapchainPass);
    destroyTexture(dummyTexture);
    destroyBuffer(dummyConstantBuffer);
    destroySampler(defaultSampler);

    //Destroy all pending resources.
    for (uint32_t i = 0; i < resourceDeletionQueue.size; ++i)
    {
        ResourceUpdate& resourceDeletion = resourceDeletionQueue[i];

        //Skip just freed resources.
        if (resourceDeletion.currentFrame == UINT_MAX)
        {
            continue;
        }

        switch (resourceDeletion.type)
        {
        case ResourceDeletion::Types::BUFFER:
            destroyBufferInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::PIPELINE:
            destroyPipelineInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::RENDER_PASS:
            destroyRenderPassInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::DESCRIPTOR_SET:
            destroyDescriptorSetInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::DESCRIPTOR_SET_LAYOUT:
            destroyDescriptorSetLayoutInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::SAMPLER:
            destroySamplerInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::SHADER_STATE:
            destroyShaderStateInstant(resourceDeletion.handle);
            break;
        case ResourceDeletion::Types::TEXTURE:
            destroyTextureInstant(resourceDeletion.handle);
            break;
        }
    }

    //Destroy render pass from the cache.
    FlatHashMapIterator it = renderPassCache.iteratorBegin();
    while (it.isValid())
    {
        VkRenderPass vkRenderPass = renderPassCache.get(it);
        vkDestroyRenderPass(vulkanDevice, vkRenderPass, vulkanAllocationCallbacks);
        renderPassCache.iteratorAdvance(it);
    }
    renderPassCache.shutdown();

    //Destroy swapchain render pass, not present in the cache.
    RenderPass* vkSwapchainPass = accessRenderPass(swapchainPass);
    vkDestroyRenderPass(vulkanDevice, vkSwapchainPass->vkRenderPass, vulkanAllocationCallbacks);

    //Destroy swapchain
    destroySwapchain();
    vkDestroySurfaceKHR(vulkanInstance, vulkanWindowSurface, vulkanAllocationCallbacks);

    vmaDestroyAllocator(VMAAllocator);

    resourceDeletionQueue.shutdown();
    descriptorSetUpdates.shutdown();

    pipelines.shutdown();
    buffers.shutdown();
    shaders.shutdown();
    textures.shutdown();
    samplers.shutdown();
    descriptorSetLayouts.shutdown();
    descriptorSets.shutdown();
    renderPasses.shutdown();
#if defined(VULKAN_DEBUG_REPORT)
    auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vulkanInstance, "vkDestroyDebugUtilsMessengerEXT");
    vkDestroyDebugUtilsMessengerEXT(vulkanInstance, vulkanDebugUtilsMessenger, vulkanAllocationCallbacks);
#endif //VULKAN_DEBUG_REPORT

    vkDestroyDescriptorPool(vulkanDevice, vulkanDescriptorPool, vulkanAllocationCallbacks);
    vkDestroyQueryPool(vulkanDevice, vulkanTimestampQueryPool, vulkanAllocationCallbacks);

    vkDestroyDevice(vulkanDevice, vulkanAllocationCallbacks);
    vkDestroyInstance(vulkanInstance, vulkanAllocationCallbacks);

    stringBuffer.shutdown();

    vprint("GPU Device Shutdown.\n");
}

//Creation/Destruction of resources
BufferHandle GPUDevice::createBuffer(const BufferCreation& creation)
{
    BufferHandle handle = { buffers.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    Buffer* buffer = accessBuffer(handle);

    buffer->name = creation.name;
    buffer->size = creation.size;
    buffer->typeFlags = creation.typeFlags;
    buffer->usage = creation.usage;
    buffer->handle = handle;
    buffer->globalOffset = 0;
    buffer->parentBuffer = INVALID_BUFFER;

    //Cache and calculate if the dynamic can be used.
    static const VkBufferUsageFlags DYNAMIC_BUFFER_MASK = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const bool useGlobalBuffer = (creation.typeFlags & DYNAMIC_BUFFER_MASK) != 0;
    if (creation.usage == ResourceType::Type::DYNAMIC && useGlobalBuffer)
    {
        buffer->parentBuffer = dynamicBuffer;
        return handle;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | creation.typeFlags;
    bufferInfo.size = creation.size > 0 ? creation.size : 1;

    VmaAllocationCreateInfo memoryInfo{};
    memoryInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    memoryInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VmaAllocationInfo allocationInfo{};
    check(vmaCreateBuffer(VMAAllocator, &bufferInfo, &memoryInfo, &buffer->vkBuffer, &buffer->vmaAllocation, &allocationInfo));

    setResourceName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer->vkBuffer), creation.name);
    buffer->vkDeviceMemory = allocationInfo.deviceMemory;

    if (creation.initialData)
    {
        void* data;
        vmaMapMemory(VMAAllocator, buffer->vmaAllocation, &data);
        memcpy(data, creation.initialData, static_cast<size_t>(creation.size));
        vmaUnmapMemory(VMAAllocator, buffer->vmaAllocation);
    }

    return handle;
}

TextureHandle GPUDevice::createTexture(const TextureCreation& creation)
{
    uint32_t resourceIndex = textures.obtainResource();
    TextureHandle handle = { resourceIndex };
    if (resourceIndex == INVALID_INDEX)
    {
        return handle;
    }

    Texture* texture = accessTexture(handle);

    vulkanCreateTexture(*this, creation, handle, texture);

    //Copy buffer data if present
    if (creation.initialData)
    {
        //Create stating buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        uint32_t imageSize = creation.width * creation.height * 4;
        bufferInfo.size = imageSize;

        VmaAllocationCreateInfo memoryInfo{};
        memoryInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        memoryInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VmaAllocationInfo allocationInfo{};
        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;
        check(vmaCreateBuffer(VMAAllocator, &bufferInfo, &memoryInfo, &stagingBuffer, &stagingAllocation, &allocationInfo));

        //Copy buffer data
        void* destinationData;
        vmaMapMemory(VMAAllocator, stagingAllocation, &destinationData);
        memcpy(destinationData, creation.initialData, static_cast<size_t>(imageSize));
        vmaUnmapMemory(VMAAllocator, stagingAllocation);

        //Execute command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        CommandBuffer* commandBuffer = getInstantCommandBuffer();
        vkBeginCommandBuffer(commandBuffer->vkCommandBuffer, &beginInfo);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { creation.width, creation.height, creation.depth };

        //Transition
        transitionImageLayout(commandBuffer->vkCommandBuffer, texture->vkImage, texture->vkFormat,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false);
        //Copy
        vkCmdCopyBufferToImage(commandBuffer->vkCommandBuffer, stagingBuffer, texture->vkImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        //Trasition
        transitionImageLayout(commandBuffer->vkCommandBuffer, texture->vkImage, texture->vkFormat,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
        vkEndCommandBuffer(commandBuffer->vkCommandBuffer);

        //Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer->vkCommandBuffer;

        vkQueueSubmit(vulkanQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkanQueue);

        vmaDestroyBuffer(VMAAllocator, stagingBuffer, stagingAllocation);

        //TODO: Maybe I need to free the command buffer.
        vkResetCommandBuffer(commandBuffer->vkCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        texture->vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    return handle;
}

PipelineHandle GPUDevice::createPipeline(const PipelineCreation& creation)
{
    PipelineHandle handle = { pipelines.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    ShaderStateHandle shaderState = createShaderState(creation.shaders);
    if (shaderState.index == INVALID_INDEX)
    {
        //Shader did not compile
        pipelines.releaseResource(handle.index);
        handle.index = INVALID_INDEX;

        return handle;
    }

    //Now that shaders have compiled we can create the pipeline.
    Pipeline* pipeline = accessPipeline(handle);
    ShaderState* shaderStateData = accessShaderState(shaderState);

    pipeline->shaderState = shaderState;

    VkDescriptorSetLayout vkLayout[MAX_DESCRIPTOR_SET_LAYOUTS]{};

    //Create VkPipelineLayout 
    for (uint32_t layout = 0; layout < creation.numActiveLayouts; ++layout)
    {
        pipeline->descriptorSetLayout[layout] = accessDescriptorSetLayout(creation.descriptorSetLayout[layout]);
        pipeline->descriptorSetLayoutHandle[layout] = creation.descriptorSetLayout[layout];

        vkLayout[layout] = pipeline->descriptorSetLayout[layout]->vkDescriptorSetLayout;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = vkLayout;
    pipelineLayoutInfo.setLayoutCount = creation.numActiveLayouts;

    VkPipelineLayout pipelineLayout;
    check(vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutInfo, vulkanAllocationCallbacks, &pipelineLayout));
    //Cache the pipeline you created
    pipeline->vkPipelineLayout = pipelineLayout;
    pipeline->numActiveLayouts = creation.numActiveLayouts;

    //Create full pipeline
    if (shaderStateData->graphicsPipeline)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        //Shader stage
        pipelineInfo.pStages = shaderStateData->shaderStateInfo;
        pipelineInfo.stageCount = shaderStateData->activeShaders;
        //Pipeline layout
        pipelineInfo.layout = pipelineLayout;

        //Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        //Vertex attributes.
        VkVertexInputAttributeDescription vertexAttributes[8]{};
        if (creation.vertexInput.numVertexAttributes)
        {
            for (uint32_t i = 0; i < creation.vertexInput.numVertexAttributes; ++i)
            {
                const VertexAttribute& vertexAttribute = creation.vertexInput.vertexAttributes[i];
                vertexAttributes[i] =
                {
                    vertexAttribute.location, vertexAttribute.binding, vertexAttribute.format, vertexAttribute.offset
                };
            }

            vertexInputInfo.vertexAttributeDescriptionCount = creation.vertexInput.numVertexAttributes;
            vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes;
        }
        else
        {
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }

        //Vertex bindings
        VkVertexInputBindingDescription vertexBindings[8];
        if (creation.vertexInput.numVertexStreams)
        {
            vertexInputInfo.vertexBindingDescriptionCount = creation.vertexInput.numVertexStreams;

            for (uint32_t i = 0; i < creation.vertexInput.numVertexStreams; ++i)
            {
                const VertexStream& vertexStream = creation.vertexInput.vertexStreams[i];
                VkVertexInputRate vertexRate = vertexStream.inputRate == VK_VERTEX_INPUT_RATE_VERTEX ?
                                                                         VK_VERTEX_INPUT_RATE_VERTEX :
                                                                         VK_VERTEX_INPUT_RATE_INSTANCE;
                vertexBindings[i] = { vertexStream.binding, vertexStream.stride, vertexRate };
            }
            vertexInputInfo.pVertexBindingDescriptions = vertexBindings;
        }
        else
        {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
        }

        pipelineInfo.pVertexInputState = &vertexInputInfo;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        pipelineInfo.pInputAssemblyState = &inputAssembly;

        //Colour blending
        VkPipelineColorBlendAttachmentState colourBlendAttachment[8];
        if (creation.blendState.activeStates)
        {
            for (size_t i = 0; i < creation.blendState.activeStates; ++i)
            {
                const BlendState& blendState = creation.blendState.blendStates[i];

                colourBlendAttachment[i].colorWriteMask = blendState.colourWriteMask;
                colourBlendAttachment[i].blendEnable = blendState.blendEnabled ? VK_TRUE : VK_FALSE;
                colourBlendAttachment[i].srcColorBlendFactor = blendState.sourceColour;
                colourBlendAttachment[i].dstColorBlendFactor = blendState.destinationColour;
                colourBlendAttachment[i].colorBlendOp = blendState.colourOperation;

                if (blendState.separateBlend)
                {
                    colourBlendAttachment[i].srcAlphaBlendFactor = blendState.sourceAlpha;
                    colourBlendAttachment[i].dstAlphaBlendFactor = blendState.destinationAlpha;
                    colourBlendAttachment[i].alphaBlendOp = blendState.alphaOperation;
                }
                else
                {
                    colourBlendAttachment[i].srcAlphaBlendFactor = blendState.sourceColour;
                    colourBlendAttachment[i].dstAlphaBlendFactor = blendState.destinationColour;
                    colourBlendAttachment[i].alphaBlendOp = blendState.colourOperation;
                }
            }
        }
        else
        {
            //Default non blended state
            colourBlendAttachment[0] = {};
            colourBlendAttachment[0].blendEnable = VK_FALSE;
            colourBlendAttachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo colourBlending{};
        colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colourBlending.logicOpEnable = VK_FALSE;
        colourBlending.logicOp = VK_LOGIC_OP_COPY;
        colourBlending.attachmentCount = creation.blendState.activeStates ? creation.blendState.activeStates : 1;
        colourBlending.pAttachments = colourBlendAttachment;
        colourBlending.blendConstants[0] = 0.f;
        colourBlending.blendConstants[1] = 0.f;
        colourBlending.blendConstants[2] = 0.f;
        colourBlending.blendConstants[3] = 0.f;

        pipelineInfo.pColorBlendState = &colourBlending;

        //Depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthWriteEnable = creation.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.stencilTestEnable = creation.depthStencil.stencilEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthTestEnable = creation.depthStencil.depthEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = creation.depthStencil.depthComparison;
        if (creation.depthStencil.stencilEnable)
        {
            //TODO: add stencil
            VOID_ASSERTM(false, "The stencil part of the depth stencil pipeline creation is set to true.");
        }

        pipelineInfo.pDepthStencilState = &depthStencil;

        //Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToOneEnable = VK_FALSE;
        multisampling.alphaToCoverageEnable = VK_FALSE;

        pipelineInfo.pMultisampleState = &multisampling;

        VkPipelineRasterizationStateCreateInfo rasteriser{};
        rasteriser.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasteriser.depthClampEnable = VK_FALSE;
        rasteriser.rasterizerDiscardEnable = VK_FALSE;
        rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
        rasteriser.lineWidth = 1.f;
        rasteriser.cullMode = creation.rasterisation.cullMode;
        rasteriser.frontFace = creation.rasterisation.front;
        rasteriser.depthBiasEnable = VK_FALSE;
        rasteriser.depthBiasConstantFactor = 0.f;
        rasteriser.depthBiasClamp = 0.f;
        rasteriser.depthBiasSlopeFactor = 0.f;

        pipelineInfo.pRasterizationState = &rasteriser;

        //TODO: Check if we need tessellation for what we are doing.
        //Tessellation
        pipelineInfo.pTessellationState = nullptr;

        //Viewport
        VkViewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(swapchainWidth);
        viewport.height = static_cast<float>(swapchainHeight);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { swapchainWidth, swapchainHeight };

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        pipelineInfo.pViewportState = &viewportState;

        //Render pass
        pipelineInfo.renderPass = getVulkanRenderPass(creation.renderPass, creation.name);

        //Dynamic state
        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates);
        dynamicState.pDynamicStates = dynamicStates;

        pipelineInfo.pDynamicState = &dynamicState;

        vkCreateGraphicsPipelines(vulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, vulkanAllocationCallbacks, &pipeline->vkPipeline);

        pipeline->vkBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
    }
    else
    {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStateData->shaderStateInfo[0];
        pipelineInfo.layout = pipelineLayout;

        vkCreateComputePipelines(vulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, vulkanAllocationCallbacks, &pipeline->vkPipeline);

        pipeline->vkBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    return handle;
}

PipelineHandle GPUDevice::createPipeline(const PipelineCreation& creation, DescriptorSetLayoutHandle one, DescriptorSetLayoutHandle two)
{
    PipelineHandle handle = { pipelines.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    ShaderStateHandle shaderState = createShaderState(creation.shaders);
    if (shaderState.index == INVALID_INDEX)
    {
        //Shader did not compile
        pipelines.releaseResource(handle.index);
        handle.index = INVALID_INDEX;

        return handle;
    }

    //Now that shaders have compiled we can create the pipeline.
    Pipeline* pipeline = accessPipeline(handle);
    ShaderState* shaderStateData = accessShaderState(shaderState);

    pipeline->shaderState = shaderState;
    DescriptorSetLayout* oneLayout = accessDescriptorSetLayout(one);
    DescriptorSetLayout* twoLayout = accessDescriptorSetLayout(two);
    VkDescriptorSetLayout vkLayout[] = { oneLayout->vkDescriptorSetLayout, twoLayout->vkDescriptorSetLayout };

    //Create VkPipelineLayout 
    //for (uint32_t layout = 0; layout < creation.numActiveLayouts; ++layout)
    //{
    //    pipeline->descriptorSetLayout[layout] = accessDescriptorSetLayout(creation.descriptorSetLayout[layout]);
    //    pipeline->descriptorSetLayoutHandle[layout] = creation.descriptorSetLayout[layout];

    //    vkLayout[layout] = pipeline->descriptorSetLayout[layout]->vkDescriptorSetLayout;
    //}

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pSetLayouts = vkLayout;
    pipelineLayoutInfo.setLayoutCount = 2;

    VkPipelineLayout pipelineLayout;
    check(vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutInfo, vulkanAllocationCallbacks, &pipelineLayout));
    //Cache the pipeline you created
    pipeline->vkPipelineLayout = pipelineLayout;
    pipeline->numActiveLayouts = creation.numActiveLayouts;

    //Create full pipeline
    if (shaderStateData->graphicsPipeline)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        //Shader stage
        pipelineInfo.pStages = shaderStateData->shaderStateInfo;
        pipelineInfo.stageCount = shaderStateData->activeShaders;
        //Pipeline layout
        pipelineInfo.layout = pipelineLayout;

        //Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        //Vertex attributes.
        VkVertexInputAttributeDescription vertexAttributes[8]{};
        if (creation.vertexInput.numVertexAttributes)
        {
            for (uint32_t i = 0; i < creation.vertexInput.numVertexAttributes; ++i)
            {
                const VertexAttribute& vertexAttribute = creation.vertexInput.vertexAttributes[i];
                vertexAttributes[i] =
                {
                    vertexAttribute.location, vertexAttribute.binding, vertexAttribute.format, vertexAttribute.offset
                };
            }

            vertexInputInfo.vertexAttributeDescriptionCount = creation.vertexInput.numVertexAttributes;
            vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes;
        }
        else
        {
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }

        //Vertex bindings
        VkVertexInputBindingDescription vertexBindings[8];
        if (creation.vertexInput.numVertexStreams)
        {
            vertexInputInfo.vertexBindingDescriptionCount = creation.vertexInput.numVertexStreams;

            for (uint32_t i = 0; i < creation.vertexInput.numVertexStreams; ++i)
            {
                const VertexStream& vertexStream = creation.vertexInput.vertexStreams[i];
                VkVertexInputRate vertexRate = vertexStream.inputRate == VK_VERTEX_INPUT_RATE_VERTEX ?
                    VK_VERTEX_INPUT_RATE_VERTEX :
                    VK_VERTEX_INPUT_RATE_INSTANCE;
                vertexBindings[i] = { vertexStream.binding, vertexStream.stride, vertexRate };
            }
            vertexInputInfo.pVertexBindingDescriptions = vertexBindings;
        }
        else
        {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
        }

        pipelineInfo.pVertexInputState = &vertexInputInfo;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        pipelineInfo.pInputAssemblyState = &inputAssembly;

        //Colour blending
        VkPipelineColorBlendAttachmentState colourBlendAttachment[8];
        if (creation.blendState.activeStates)
        {
            for (size_t i = 0; i < creation.blendState.activeStates; ++i)
            {
                const BlendState& blendState = creation.blendState.blendStates[i];

                colourBlendAttachment[i].colorWriteMask = blendState.colourWriteMask;
                colourBlendAttachment[i].blendEnable = blendState.blendEnabled ? VK_TRUE : VK_FALSE;
                colourBlendAttachment[i].srcColorBlendFactor = blendState.sourceColour;
                colourBlendAttachment[i].dstColorBlendFactor = blendState.destinationColour;
                colourBlendAttachment[i].colorBlendOp = blendState.colourOperation;

                if (blendState.separateBlend)
                {
                    colourBlendAttachment[i].srcAlphaBlendFactor = blendState.sourceAlpha;
                    colourBlendAttachment[i].dstAlphaBlendFactor = blendState.destinationAlpha;
                    colourBlendAttachment[i].alphaBlendOp = blendState.alphaOperation;
                }
                else
                {
                    colourBlendAttachment[i].srcAlphaBlendFactor = blendState.sourceColour;
                    colourBlendAttachment[i].dstAlphaBlendFactor = blendState.destinationColour;
                    colourBlendAttachment[i].alphaBlendOp = blendState.colourOperation;
                }
            }
        }
        else
        {
            //Default non blended state
            colourBlendAttachment[0] = {};
            colourBlendAttachment[0].blendEnable = VK_FALSE;
            colourBlendAttachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo colourBlending{};
        colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colourBlending.logicOpEnable = VK_FALSE;
        colourBlending.logicOp = VK_LOGIC_OP_COPY;
        colourBlending.attachmentCount = creation.blendState.activeStates ? creation.blendState.activeStates : 1;
        colourBlending.pAttachments = colourBlendAttachment;
        colourBlending.blendConstants[0] = 0.f;
        colourBlending.blendConstants[1] = 0.f;
        colourBlending.blendConstants[2] = 0.f;
        colourBlending.blendConstants[3] = 0.f;

        pipelineInfo.pColorBlendState = &colourBlending;

        //Depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthWriteEnable = creation.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.stencilTestEnable = creation.depthStencil.stencilEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthTestEnable = creation.depthStencil.depthEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = creation.depthStencil.depthComparison;
        if (creation.depthStencil.stencilEnable)
        {
            //TODO: add stencil
            VOID_ASSERTM(false, "The stencil part of the depth stencil pipeline creation is set to true.");
        }

        pipelineInfo.pDepthStencilState = &depthStencil;

        //Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToOneEnable = VK_FALSE;
        multisampling.alphaToCoverageEnable = VK_FALSE;

        pipelineInfo.pMultisampleState = &multisampling;

        VkPipelineRasterizationStateCreateInfo rasteriser{};
        rasteriser.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasteriser.depthClampEnable = VK_FALSE;
        rasteriser.rasterizerDiscardEnable = VK_FALSE;
        rasteriser.polygonMode = VK_POLYGON_MODE_FILL;
        rasteriser.lineWidth = 1.f;
        rasteriser.cullMode = creation.rasterisation.cullMode;
        rasteriser.frontFace = creation.rasterisation.front;
        rasteriser.depthBiasEnable = VK_FALSE;
        rasteriser.depthBiasConstantFactor = 0.f;
        rasteriser.depthBiasClamp = 0.f;
        rasteriser.depthBiasSlopeFactor = 0.f;

        pipelineInfo.pRasterizationState = &rasteriser;

        //TODO: Check if we need tessellation for what we are doing.
        //Tessellation
        pipelineInfo.pTessellationState = nullptr;

        //Viewport
        VkViewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(swapchainWidth);
        viewport.height = static_cast<float>(swapchainHeight);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { swapchainWidth, swapchainHeight };

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        pipelineInfo.pViewportState = &viewportState;

        //Render pass
        pipelineInfo.renderPass = getVulkanRenderPass(creation.renderPass, creation.name);

        //Dynamic state
        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates);
        dynamicState.pDynamicStates = dynamicStates;

        pipelineInfo.pDynamicState = &dynamicState;

        vkCreateGraphicsPipelines(vulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, vulkanAllocationCallbacks, &pipeline->vkPipeline);

        pipeline->vkBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
    }
    else
    {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStateData->shaderStateInfo[0];
        pipelineInfo.layout = pipelineLayout;

        vkCreateComputePipelines(vulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, vulkanAllocationCallbacks, &pipeline->vkPipeline);

        pipeline->vkBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    return handle;
}

SamplerHandle GPUDevice::createSampler(const SamplerCreation& creation)
{
    SamplerHandle handle = { samplers.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    Sampler* sampler = accessSampler(handle);

    sampler->addressModeU = creation.addressModeU;
    sampler->addressModeV = creation.addressModeV;
    sampler->addressModeW = creation.addressModeW;
    sampler->minFilter = creation.minFilter;
    sampler->magFilter = creation.magFilter;
    sampler->mipFilter = creation.mipFilter;
    sampler->name = creation.name;

    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.addressModeU = creation.addressModeU;
    createInfo.addressModeV = creation.addressModeV;
    createInfo.addressModeW = creation.addressModeW;
    createInfo.minFilter = creation.minFilter;
    createInfo.magFilter = creation.magFilter;
    createInfo.mipmapMode = creation.mipFilter;
    createInfo.anisotropyEnable = 0;
    createInfo.compareEnable = 0;
    createInfo.unnormalizedCoordinates = 0;
    createInfo.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_WHITE;

    vkCreateSampler(vulkanDevice, &createInfo, vulkanAllocationCallbacks, &sampler->vkSampler);

    setResourceName(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64_t>(sampler->vkSampler), creation.name);

    return handle;
}

DescriptorSetLayoutHandle GPUDevice::createDescriptorSetLayout(const DescriptorSetLayoutCreation& creation)
{
    DescriptorSetLayoutHandle handle = { descriptorSetLayouts.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    DescriptorSetLayout* descriptorSetLayout = accessDescriptorSetLayout(handle);

    //TODO: add support for muliple sets.
    //Create flattened binding list
    descriptorSetLayout->numBindings = static_cast<uint16_t>(creation.numBindings);
    uint8_t* memory = void_allocam((sizeof(VkDescriptorSetLayoutBinding) + sizeof(DescriptorBinding)) * creation.numBindings, allocator);
    descriptorSetLayout->bindings = reinterpret_cast<DescriptorBinding*>(memory);
    descriptorSetLayout->vkBinding = reinterpret_cast<VkDescriptorSetLayoutBinding*>(memory + sizeof(DescriptorBinding) * creation.numBindings);
    descriptorSetLayout->handle = handle;
    descriptorSetLayout->setIndex = static_cast<uint16_t>(creation.setIndex);

    uint32_t usedBindings = 0;
    for (uint32_t i = 0; i < creation.numBindings; ++i)
    {
        DescriptorBinding& binding = descriptorSetLayout->bindings[i];
        const DescriptorSetLayoutCreation::Binding& inputBinding = creation.bindings[i];
        binding.start = inputBinding.start == UINT16_MAX ? static_cast<uint16_t>(i) : inputBinding.start;
        binding.count = 1;
        binding.type = inputBinding.type;
        binding.name = inputBinding.name;

        VkDescriptorSetLayoutBinding& vkBinding = descriptorSetLayout->vkBinding[usedBindings];
        ++usedBindings;

        vkBinding.binding = binding.start;
        vkBinding.descriptorType = inputBinding.type;
        vkBinding.descriptorType = vkBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ?
                                                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC :
                                                                vkBinding.descriptorType;
        vkBinding.descriptorCount = 1;

        vkBinding.stageFlags = inputBinding.stage;
        vkBinding.pImmutableSamplers = nullptr;
    }

    //Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = usedBindings;
    layoutInfo.pBindings = descriptorSetLayout->vkBinding;

    vkCreateDescriptorSetLayout(vulkanDevice, &layoutInfo, vulkanAllocationCallbacks, &descriptorSetLayout->vkDescriptorSetLayout);

    return handle;
}

DescriptorSetHandle GPUDevice::createDescriptorSet(const DescriptorSetCreation& creation)
{
    DescriptorSetHandle handle = { descriptorSets.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    DescriptorSet* descriptorSet = accessDescriptorSet(handle);
    const DescriptorSetLayout* descriptorSetLayout = accessDescriptorSetLayout(creation.layout);

    //Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vulkanDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout->vkDescriptorSetLayout;

    check(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &descriptorSet->vkDescriptorSet));

    //Cache data
    uint8_t* memory = void_allocam((sizeof(uint32_t) + sizeof(SamplerHandle) + sizeof(uint16_t)) * creation.numResources, allocator);
    descriptorSet->resources = reinterpret_cast<uint32_t*>(memory);
    descriptorSet->samplers = reinterpret_cast<SamplerHandle*>(memory + sizeof(uint32_t) * creation.numResources);
    descriptorSet->bindings = reinterpret_cast<uint16_t*>(memory + (sizeof(uint32_t) + sizeof(SamplerHandle)) * creation.numResources);
    descriptorSet->numResources = creation.numResources;
    descriptorSet->layout = descriptorSetLayout;

    //Update descriptor set
    VkWriteDescriptorSet descriptorWrite[8];
    VkDescriptorBufferInfo bufferInfo[8];
    VkDescriptorImageInfo imageInfo[8];

    Sampler* vkDefaultSampler = accessSampler(defaultSampler);

    uint32_t numResources = creation.numResources;
    vulkanFillWriteDescriptorSets(*this, descriptorSetLayout, descriptorSet->vkDescriptorSet, descriptorWrite, bufferInfo, imageInfo,
        vkDefaultSampler->vkSampler, numResources, creation.resources, creation.samplers, creation.bindings);

    //Cache resource
    for (uint32_t res = 0; res < creation.numResources; ++res)
    {
        descriptorSet->resources[res] = creation.resources[res];
        descriptorSet->samplers[res] = creation.samplers[res];
        descriptorSet->bindings[res] = creation.bindings[res];
    }

    vkUpdateDescriptorSets(vulkanDevice, numResources, descriptorWrite, 0, nullptr);

    return handle;
}

RenderPassHandle GPUDevice::createRenderPass(const RenderPassCreation& creation)
{
    RenderPassHandle handle = { renderPasses.obtainResource() };
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    RenderPass* renderPass = accessRenderPass(handle);
    renderPass->type = creation.type;
    renderPass->numRenderTargets = static_cast<uint8_t>(creation.numRenderTargets);
    renderPass->dispatchX = 0;
    renderPass->dispatchY = 0;
    renderPass->dispatchZ = 0;
    renderPass->name = creation.name;
    renderPass->vkFrameBuffer = nullptr;
    renderPass->vkRenderPass = nullptr;
    renderPass->scaleX = creation.scaleX;
    renderPass->scaleY = creation.scaleY;
    renderPass->resize = creation.resize;

    //Cache texture handles
    uint32_t renderTarget = 0;
    for (; renderTarget < creation.numRenderTargets; ++renderTarget)
    {
        Texture* textureVK = accessTexture(creation.outputTextures[renderTarget]);

        renderPass->width = textureVK->width;
        renderPass->height = textureVK->height;

        //Cache texture handles
        renderPass->outputTextures[renderTarget] = creation.outputTextures[renderTarget];
    }

    renderPass->outputDepth = creation.depthStencilTexture;

    switch (creation.type)
    {
    case RenderPassType::Types::SWAPCHAIN:
        vulkanCreateSwapchainPass(*this, creation, renderPass);
        break;
    case RenderPassType::Types::COMPUTE:
        break;
    case RenderPassType::Types::GEOMETRY:
        renderPass->output = fillRenderPassOutput(*this, creation);
        renderPass->vkRenderPass = getVulkanRenderPass(renderPass->output, creation.name);

        vulkanCreateFramebuffer(*this, renderPass, creation.outputTextures, creation.numRenderTargets, creation.depthStencilTexture);
        break;
    }

    return handle;
}

ShaderStateHandle GPUDevice::createShaderState(const ShaderStateCreation& creation)
{
    ShaderStateHandle handle = { INVALID_INDEX };

    if (creation.stagesCount == 0 || creation.stages == nullptr)
    {
        vprint("Shader %s does not contain shader.\n", creation.name);
        return handle;
    }

    handle.index = shaders.obtainResource();
    if (handle.index == INVALID_INDEX)
    {
        return handle;
    }

    //For each shader stage, compile them individually.
    uint32_t compiledShaders = 0;

    ShaderState* shaderState = accessShaderState(handle);
    shaderState->graphicsPipeline = true;
    shaderState->activeShaders = 0;

    size_t currentTemporaryMarker = tempAllocator->getMarker();

    for (compiledShaders = 0; compiledShaders < creation.stagesCount; ++compiledShaders)
    {
        const ShaderStage& stage = creation.stages[compiledShaders];

        //Gives priority to compute: if any is present (and it should not be) then it is not a graphics pipeline.
        if (stage.type == VK_SHADER_STAGE_COMPUTE_BIT)
        {
            shaderState->graphicsPipeline = false;
        }

        VkShaderModuleCreateInfo shaderCreateInfo{};
        shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        if (creation.spvInput)
        {
            shaderCreateInfo.codeSize = stage.codeSize;
            shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(stage.code);
        }
        else
        {
            VOID_ERROR("We are always using compiled shaders now.");
        }

        //Compiler shader module
        VkPipelineShaderStageCreateInfo& shaderStageInfo = shaderState->shaderStateInfo[compiledShaders];
        memset(&shaderStageInfo, 0, sizeof(VkPipelineShaderStageCreateInfo));
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.pName = "main";
        shaderStageInfo.stage = stage.type;

        if (vkCreateShaderModule(vulkanDevice, &shaderCreateInfo, nullptr, &shaderState->shaderStateInfo[compiledShaders].module) != VK_SUCCESS)
        {
            break;
        }

        setResourceName(VK_OBJECT_TYPE_SHADER_MODULE, reinterpret_cast<uint64_t>(shaderState->shaderStateInfo[compiledShaders].module), creation.name);
    }

    tempAllocator->freeMarker(currentTemporaryMarker);

    bool creationFailed = compiledShaders != creation.stagesCount;
    if (creationFailed == false)
    {
        shaderState->activeShaders = compiledShaders;
        shaderState->name = creation.name;
    }
    else
    {
        destroyShaderState(handle);
        handle.index = INVALID_INDEX;

        //Dump the old shader code
        vprint("Error in creation of shader %s. Dumping all shader information.\n", creation.name);
        for (compiledShaders = 0; compiledShaders < creation.stagesCount; ++compiledShaders)
        {
            const ShaderStage& stage = creation.stages[compiledShaders];
            vprint("%u:\n%s\n", stage.type, stage.code);
        }
    }
    return handle;
}

void GPUDevice::destroyBuffer(BufferHandle buffer)
{
    if (buffer.index < buffers.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::BUFFER, buffer.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid buffer %u\n", buffer.index);
    }
}

void GPUDevice::destroyTexture(TextureHandle texture)
{
    if (texture.index < textures.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::TEXTURE, texture.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid texture %u\n", texture.index);
    }
}

void GPUDevice::destroyPipeline(PipelineHandle pipeline)
{
    if (pipeline.index < pipelines.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::PIPELINE, pipeline.index, currentFrame });
        //Shader state current is handled internally when creating a pipeline, thus add this to track correctly.
        Pipeline* pipelineTrack = accessPipeline(pipeline);
        destroyShaderState(pipelineTrack->shaderState);
    }
    else
    {
        vprint("Graphics error: Trying to free invalid pipline %u\n", pipeline.index);
    }
}

void GPUDevice::destroySampler(SamplerHandle sampler)
{
    if (sampler.index < samplers.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::SAMPLER, sampler.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid sampler %u\n", sampler.index);
    }
}

void GPUDevice::destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout)
{
    if (layout.index < descriptorSetLayouts.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::DESCRIPTOR_SET_LAYOUT, layout.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid descriptor set layout %u\n", layout.index);
    }
}

void GPUDevice::destroyDescriptorSet(DescriptorSetHandle layout)
{
    if (layout.index < descriptorSets.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::DESCRIPTOR_SET, layout.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid descriptor set %u\n", layout.index);
    }
}

void GPUDevice::destroyRenderPass(RenderPassHandle renderPass)
{
    if (renderPass.index < renderPasses.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::RENDER_PASS, renderPass.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid render pass %u\n", renderPass.index);
    }
}

void GPUDevice::destroyShaderState(ShaderStateHandle shader)
{
    if (shader.index < shaders.poolSize)
    {
        resourceDeletionQueue.push({ ResourceDeletion::Types::SHADER_STATE, shader.index, currentFrame });
    }
    else
    {
        vprint("Graphics error: Trying to free invalid shaders %u\n", shader.index);
    }
}

//Query description
void GPUDevice::queryBuffer(BufferHandle buffer, BufferDescription& outDescriptor)
{
    if (buffer.index != INVALID_INDEX)
    {
        Buffer* bufferData = accessBuffer(buffer);

        outDescriptor.name = bufferData->name;
        outDescriptor.size = bufferData->size;
        outDescriptor.typeFlags = bufferData->typeFlags;
        outDescriptor.usage = bufferData->usage;
        outDescriptor.parentHandle = bufferData->parentBuffer;
        outDescriptor.nativeHandle = reinterpret_cast<void*>(&bufferData->vkBuffer);
    }
}

void GPUDevice::queryTexture(TextureHandle texture, TextureDescription& outDescriptor)
{
    if (texture.index != INVALID_INDEX)
    {
        Texture* textureData = accessTexture(texture);

        outDescriptor.width = textureData->width;
        outDescriptor.height = textureData->height;
        outDescriptor.depth = textureData->depth;
        outDescriptor.format = textureData->vkFormat;
        outDescriptor.mipmaps = textureData->mipmaps;
        outDescriptor.imageType = textureData->imageType;
        outDescriptor.imageViewType = textureData->imageViewType;
        outDescriptor.nativeHandle = reinterpret_cast<void*>(&textureData->vkImage);
        outDescriptor.name = textureData->name;
    }
}

void GPUDevice::queryPipeline(PipelineHandle pipeline, PipelineDescription& outDescriptor)
{
    if (pipeline.index != INVALID_INDEX)
    {
        const Pipeline* pipelineData = accessPipeline(pipeline);
        outDescriptor.shader = pipelineData->shaderState;
    }
}

void GPUDevice::querySampler(SamplerHandle sampler, SamplerDescription& outDescriptor)
{
    if (sampler.index != INVALID_INDEX)
    {
        const Sampler* samplerData = accessSampler(sampler);

        outDescriptor.addressModeU = samplerData->addressModeU;
        outDescriptor.addressModeV = samplerData->addressModeV;
        outDescriptor.addressModeW = samplerData->addressModeW;
        outDescriptor.minFilter = samplerData->minFilter;
        outDescriptor.magFilter = samplerData->magFilter;
        outDescriptor.mipFilter = samplerData->mipFilter;
        outDescriptor.name = samplerData->name;
    }
}

void GPUDevice::queryDescriptorSetLayout(DescriptorSetLayoutHandle layout, DescriptorSetLayoutDescription& outDescriptor)
{
    if (layout.index != INVALID_INDEX)
    {
        const DescriptorSetLayout* descriptorSetLayoutData = accessDescriptorSetLayout(layout);

        const uint32_t numBinding = descriptorSetLayoutData->numBindings;
        for (uint32_t i = 0; i < numBinding; ++i)
        {
            outDescriptor.bindings[i].name = descriptorSetLayoutData->bindings[i].name;
            outDescriptor.bindings[i].type = descriptorSetLayoutData->bindings[i].type;
        }

        outDescriptor.numActiveBindings = descriptorSetLayoutData->numBindings;
    }
}

void GPUDevice::queryDescriptorSet(DescriptorSetHandle set, DescriptorSetDescription& outDescriptor)
{
    if (set.index != INVALID_INDEX)
    {
        const DescriptorSet* descriptorSetData = accessDescriptorSet(set);

        outDescriptor.numActiveResources = descriptorSetData->numResources;
    }
}

void GPUDevice::queryShaderState(ShaderStateHandle shader, ShaderStateDescription& outDescriptor)
{
    if (shader.index != INVALID_INDEX)
    {
        const ShaderState* shaderState = accessShaderState(shader);

        outDescriptor.name = shaderState->name;
        //TODO: ShaderStateDescription and ShaderState are very different and likely need refactoring.
    }
}

const RenderPassOutput& GPUDevice::getRenderPassOutput(RenderPassHandle renderPass) const
{
    const RenderPass* vulkanRenderPass = accessRenderPass(renderPass);
    return vulkanRenderPass->output;
}

//Update/Reload resources
void GPUDevice::resizeOutputTextures(RenderPassHandle renderPass, uint32_t width, uint32_t height)
{
    //For each texture, create a temporary pooled texture and cache the handles to delete.
    //This is because we substitute just the Vulkan texture when resizing so that external
    //users don't need to update the handle.

    RenderPass* vkRenderPass = accessRenderPass(renderPass);
    if (vkRenderPass)
    {
        //No need to resize!
        if (vkRenderPass->resize == false)
        {
            return;
        }
    }

    //Calculate new width and height based on render pass sizing informations.
    uint16_t newWidth = static_cast<uint16_t>(width * vkRenderPass->scaleX);
    uint16_t newHeight = static_cast<uint16_t>(height * vkRenderPass->scaleY);

    //Resize textures if needed.
    const uint32_t rts = vkRenderPass->numRenderTargets;
    for (uint32_t i = 0; i < rts; ++i)
    {
        TextureHandle texture = vkRenderPass->outputTextures[i];
        Texture* vkTexture = accessTexture(texture);

        if (vkTexture->width == newWidth && vkTexture->height == newHeight)
        {
            continue;
        }

        //Queue deletion of the texture by creating a temp one.
        TextureHandle textureToDelete = { textures.obtainResource() };
        Texture* vkTextureToDelete = accessTexture(textureToDelete);
        //Update handle so it can be used to update bindless to dummy texture.
        vkTextureToDelete->handle = textureToDelete;
        vulkanResizeTexture(*this, vkTexture, vkTextureToDelete, newWidth, newHeight, 1);

        destroyTexture(textureToDelete);
    }

    if (vkRenderPass->outputDepth.index != INVALID_INDEX)
    {
        Texture* vkTexture = accessTexture(vkRenderPass->outputDepth);

        if (vkTexture->width != newWidth || vkTexture->height != newHeight)
        {
            //Queue deletion of texture by creating a temporay one.
            TextureHandle textureToDelete = { textures.obtainResource() };
            Texture* vkTextureToDelete = accessTexture(textureToDelete);
            //Update handle so it can used to update bindless to dummy texutre.
            vkTextureToDelete->handle = textureToDelete;
            vulkanResizeTexture(*this, vkTexture, vkTextureToDelete, newWidth, newHeight, 1);

            destroyTexture(textureToDelete);
        }
    }

    //Again: create temporary resource to use the standard deferred deletion mechanism.
    RenderPassHandle renderPassToDestroy = { renderPasses.obtainResource() };
    RenderPass* vkRenderPassToDestroy = accessRenderPass(renderPassToDestroy);

    vkRenderPassToDestroy->vkFrameBuffer = vkRenderPass->vkFrameBuffer;
    //This is checked in the destroy method to proceed with frame buffer destruction.
    vkRenderPassToDestroy->numRenderTargets = 1;
    //Set this to 0 so deletion won't be performed.
    vkRenderPassToDestroy->vkRenderPass = 0;

    destroyRenderPass(renderPassToDestroy);

    //Update render pass size
    vkRenderPass->width = newWidth;
    vkRenderPass->height = newHeight;

    //Recreate framebuffer if present (mainly for dispatch only passes)
    if (vkRenderPass->vkFrameBuffer)
    {
        vulkanCreateFramebuffer(*this, vkRenderPass, vkRenderPass->outputTextures,
                                vkRenderPass->numRenderTargets, vkRenderPass->outputDepth);
    }
}

void GPUDevice::updateDescriptorSet(DescriptorSetHandle set)
{
    if (set.index < descriptorSets.poolSize)
    {
        DescriptorSetUpdate newUpdate = { set, currentFrame };
        descriptorSetUpdates.push(newUpdate);
    }
    else
    {
        vprint("Graphics error: trying to update invalid DescriptorSet %u\n", set);
    }
}

//Misc
//TODO: For now specify a sampler for a texture or use the default one.
void GPUDevice::linkTextureSampler(TextureHandle texture, SamplerHandle sampler)
{
    Texture* textureVK = accessTexture(texture);
    Sampler* samplerVK = accessSampler(sampler);

    textureVK->sampler = samplerVK;
}

void GPUDevice::setPresentMode(PresentMode::Types mode)
{
    //Requested a certain mode and confirm that it is available.
    //If not use the VK_PRESENT_MODE_FIFO_KHR which is mandatory.
    uint32_t supportCount = 0;

    static VkPresentModeKHR supportedModeAllocated[8];
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanWindowSurface, &supportCount, nullptr);
    VOID_ASSERT(supportCount < 8);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanWindowSurface, &supportCount, supportedModeAllocated);

    bool modeFound = false;
    VkPresentModeKHR requestedMode = toVkPresentMode(mode);
    for (uint32_t i = 0; i < supportCount; ++i)
    {
        if (requestedMode == supportedModeAllocated[i])
        {
            modeFound = true;
            break;
        }
    }

    //Default to VK_PRESENT_MODE_FIFO_KHR that is guaranteed to always be supported
    vulkanPresentMode = modeFound ? requestedMode : VK_PRESENT_MODE_FIFO_KHR;
    //TODO: Figure out if we need to have (vulkanPresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? 2 : 3;)

    presentMode = modeFound ? mode : PresentMode::Types::VSYNC;
}

void GPUDevice::frameCountersAdvanced()
{
    previousFrame = currentFrame;
    currentFrame = (currentFrame + 1) % vulkanSwapchainImageCount;

    ++absoluteFrame;
}

bool GPUDevice::getFamilyQueue(VkPhysicalDevice physicalDevice)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    VkQueueFamilyProperties* queueFamilies = reinterpret_cast<VkQueueFamilyProperties*>(void_alloca(sizeof(VkQueueFamilyProperties) * queueFamilyCount, allocator));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    VkBool32 surfaceSupport;
    for (uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex)
    {
        VkQueueFamilyProperties queueFamily = queueFamilies[familyIndex];
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, vulkanWindowSurface, &surfaceSupport);

            if (surfaceSupport)
            {
                vulkanQueueFamily = familyIndex;
                break;
            }
        }
    }

    void_free(queueFamilies, allocator);

    return surfaceSupport;
}

//Swapchain
void GPUDevice::createSwapchain()
{
    //Check if the surface is supported
    VkBool32 surfaceSupport;
    vkGetPhysicalDeviceSurfaceSupportKHR(vulkanPhysicalDevice, vulkanQueueFamily, vulkanWindowSurface, &surfaceSupport);
    VOID_ASSERTM(surfaceSupport == VK_TRUE, "Error no WSI support on that physical device. \n");

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanWindowSurface, &surfaceCapabilities);

    VkExtent2D swapchainExtent = surfaceCapabilities.currentExtent;
    if (swapchainExtent.width == UINT32_MAX)
    {
        swapchainExtent.width = clamp(swapchainExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        swapchainExtent.height = clamp(swapchainExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    vulkanSwapchainImageCount = surfaceCapabilities.minImageCount + 1;

    vprint("Create swapchain %u %u - saved %u %u, min image %u\n", swapchainExtent.width, swapchainExtent.height,
        swapchainWidth, swapchainHeight, surfaceCapabilities.minImageCount);

    swapchainWidth = static_cast<uint16_t>(swapchainExtent.width);
    swapchainHeight = static_cast<uint16_t>(swapchainExtent.height);

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = vulkanWindowSurface;
    swapchainCreateInfo.minImageCount = vulkanSwapchainImageCount;
    swapchainCreateInfo.imageFormat = vulkanSurfaceFormat.format;
    swapchainCreateInfo.imageExtent = swapchainExtent;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = vulkanPresentMode;

    check(vkCreateSwapchainKHR(vulkanDevice, &swapchainCreateInfo, nullptr, &vulkanSwapchain));

    //Cache the swapchain image
    vkGetSwapchainImagesKHR(vulkanDevice, vulkanSwapchain, &vulkanSwapchainImageCount, nullptr);

    vulkanSwapchainImages.init(allocator, vulkanSwapchainImageCount, vulkanSwapchainImageCount);
    vkGetSwapchainImagesKHR(vulkanDevice, vulkanSwapchain, &vulkanSwapchainImageCount, vulkanSwapchainImages.data);

    vulkanSwapchainImageViews.init(allocator, vulkanSwapchainImageCount, vulkanSwapchainImageCount);

    vulkanSwapchainFramebuffers.init(allocator, vulkanSwapchainImageCount, vulkanSwapchainImageCount);
    for (uint32_t imageCount = 0; imageCount < vulkanSwapchainImageCount; ++imageCount)
    {
        //Create an image view so we can render into it.
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vulkanSurfaceFormat.format;
        viewInfo.image = vulkanSwapchainImages[imageCount];
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

        check(vkCreateImageView(vulkanDevice, &viewInfo, vulkanAllocationCallbacks, &vulkanSwapchainImageViews[imageCount]));
    }
}

void GPUDevice::destroySwapchain()
{
    for (uint32_t imageCount = 0; imageCount < vulkanSwapchainImageCount; ++imageCount)
    {
        vkDestroyImageView(vulkanDevice, vulkanSwapchainImageViews[imageCount], vulkanAllocationCallbacks);
        vkDestroyFramebuffer(vulkanDevice, vulkanSwapchainFramebuffers[imageCount], vulkanAllocationCallbacks);
    }

    vkDestroySwapchainKHR(vulkanDevice, vulkanSwapchain, vulkanAllocationCallbacks);

    vulkanSwapchainImages.shutdown();
    vulkanSwapchainImageViews.shutdown();
    vulkanSwapchainFramebuffers.shutdown();
}

void GPUDevice::resizeSwapchain()
{
    vkDeviceWaitIdle(vulkanDevice);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanWindowSurface, &surfaceCapabilities);
    VkExtent2D swapchainExtent = surfaceCapabilities.currentExtent;

    //Skip zero-sized swapchain
    if (swapchainExtent.width == 0 || swapchainExtent.height == 0)
    {
        return;
    }

    //Internal destroy of swapchain pass to retain the same handle.
    RenderPass* vkSwapchainPass = accessRenderPass(swapchainPass);
    vkDestroyRenderPass(vulkanDevice, vkSwapchainPass->vkRenderPass, vulkanAllocationCallbacks);

    //Destroy swapchain images and framebuffers.
    destroySwapchain();
    vkDestroySurfaceKHR(vulkanInstance, vulkanWindowSurface, vulkanAllocationCallbacks);

    //Recreate window surface
    if (SDL_Vulkan_CreateSurface(SDLWindow, vulkanInstance, vulkanAllocationCallbacks, &vulkanWindowSurface) == false)
    {
        vprint("Failed to create Vulkan Surface.\n");
    }

    //Create swapchain
    createSwapchain();

    //Resize depth texture, maintaining handle, using a dummy texture to destroy.
    TextureHandle textureToDelete = { textures.obtainResource() };
    Texture* vkTextureToDelete = accessTexture(textureToDelete);
    vkTextureToDelete->handle = textureToDelete;
    Texture* vkDepthTexture = accessTexture(depthTexture);
    vulkanResizeTexture(*this, vkDepthTexture, vkTextureToDelete, swapchainWidth, swapchainHeight, 1);

    destroyTexture(textureToDelete);

    RenderPassCreation swapchainPassCreation{};
    swapchainPassCreation.setType(RenderPassType::SWAPCHAIN).setName("Swapchain");
    vulkanCreateSwapchainPass(*this, swapchainPassCreation, vkSwapchainPass);

    vkDeviceWaitIdle(vulkanDevice);
}

//Map/Unmap
void* GPUDevice::mapBuffer(const MapBufferParameters& parameters)
{
    if (parameters.buffer.index == INVALID_INDEX)
    {
        return nullptr;
    }

    Buffer* buffer = accessBuffer(parameters.buffer);

    if (buffer->parentBuffer.index == dynamicBuffer.index)
    {
        buffer->globalOffset = dynamicAllocatedSize;
        return dynamicAllocate(parameters.size == 0 ? buffer->size : parameters.size);
    }

    void* data;
    vmaMapMemory(VMAAllocator, buffer->vmaAllocation, &data);

    return data;
}

void  GPUDevice::unmapBuffer(const MapBufferParameters& parameters)
{
    if (parameters.buffer.index == INVALID_INDEX)
    {
        return;
    }

    Buffer* buffer = accessBuffer(parameters.buffer);
    if (buffer->parentBuffer.index == dynamicBuffer.index)
    {
        return;
    }

    vmaUnmapMemory(VMAAllocator, buffer->vmaAllocation);
}

void* GPUDevice::dynamicAllocate(uint32_t size)
{
    void* mappedMemory = dynamicMappedMemory + dynamicAllocatedSize;
    dynamicAllocatedSize += static_cast<uint32_t>(memoryAlign(size, UBO_ALIGNMENT));
    return mappedMemory;
}

void GPUDevice::setBufferGlobalOffset(BufferHandle buffer, uint32_t offset)
{
    if (buffer.index == INVALID_INDEX)
    {
        return;
    }

    Buffer* vulkanBuffer = accessBuffer(buffer);
    vulkanBuffer->globalOffset = offset;
}

//Command buffers
CommandBuffer* GPUDevice::getCommandBuffer(VkQueueFlagBits /*type*/, bool begin)
{
    CommandBuffer* comBuffer = commandBufferRing.getCommandBuffer(currentFrame, begin);

    //The first commandBuffer issued in the frame is used to reset the timestamp queries used.
    if (gpuTimestampReset && begin)
    {
        vkCmdResetQueryPool(comBuffer->vkCommandBuffer, vulkanTimestampQueryPool,
            currentFrame * gpuTimestampManager->queriesPerFrame * 2,
            gpuTimestampManager->queriesPerFrame);

        gpuTimestampReset = false;
    }

    return comBuffer;
}

CommandBuffer* GPUDevice::getInstantCommandBuffer()
{
    CommandBuffer* comBuffer = commandBufferRing.getCommandBufferInstant(currentFrame, false);
    return comBuffer;
}

void GPUDevice::queueCommandBuffer(CommandBuffer* commandBuffer)
{
    queuedCommandBuffers[numQueuedCommandBuffers++] = commandBuffer;
}

//Rendering
void GPUDevice::newFrame()
{
    //Fence wait and reset.
    VkFence* renderCompleteFence = &vulkanCommandBufferExecutedFence[currentFrame];

    vkWaitForFences(vulkanDevice, 1, renderCompleteFence, VK_TRUE, UINT64_MAX);

    vkResetFences(vulkanDevice, 1, renderCompleteFence);

    VkResult result = vkAcquireNextImageKHR(vulkanDevice, vulkanSwapchain, UINT64_MAX, vulkanImageAcquiredSemaphore[currentFrame], VK_NULL_HANDLE, &vulkanImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resizeSwapchain();
    }

    //Command pool rest.
    commandBufferRing.resetPools(currentFrame);
    //Dynamic memory update
    const uint32_t usedSize = dynamicAllocatedSize - (dynamicPerFrameSize * previousFrame);
    dynamicMaxPerFrameSize = max(usedSize, dynamicMaxPerFrameSize);
    dynamicAllocatedSize = dynamicPerFrameSize * currentFrame;

    //Descriptor set update.
    if (descriptorSetUpdates.size)
    {
        for (int32_t i = descriptorSetUpdates.size - 1; i >= 0; --i)
        {
            DescriptorSetUpdate& update = descriptorSetUpdates[i];

            updateDescriptorSetInstant(update);
            update.frameIssued = UINT32_MAX;
            descriptorSetUpdates.deleteSwap(i);
        }
    }
}

void GPUDevice::present()
{
    VkSemaphore* renderCompleteSemaphore = &vulkanRenderCompleteSemaphore[currentFrame];
    VkFence* renderCompleteFence = &vulkanCommandBufferExecutedFence[currentFrame];

    //Copy all commands
    VkCommandBuffer enqueuedCommandBuffers[4]{};
    for (uint32_t comBuffer = 0; comBuffer < numQueuedCommandBuffers; ++comBuffer)
    {
        CommandBuffer* commandBuffer = queuedCommandBuffers[comBuffer];
        enqueuedCommandBuffers[comBuffer] = commandBuffer->vkCommandBuffer;

        if (commandBuffer->isRecording && commandBuffer->currentRenderPass &&
            (commandBuffer->currentRenderPass->type != RenderPassType::Types::COMPUTE))
        {
            vkCmdEndRenderPass(commandBuffer->vkCommandBuffer);
        }

        vkEndCommandBuffer(commandBuffer->vkCommandBuffer);
    }

    //Subit command buffer.
    VkSemaphore waitSemaphores[] = { vulkanImageAcquiredSemaphore[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = numQueuedCommandBuffers;
    submitInfo.pCommandBuffers = enqueuedCommandBuffers;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = renderCompleteSemaphore;

    vkQueueSubmit(vulkanQueue, 1, &submitInfo, *renderCompleteFence);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = renderCompleteSemaphore;

    VkSwapchainKHR swapchains[] = { vulkanSwapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &vulkanImageIndex;
    presentInfo.pResults = nullptr;
    VkResult result = vkQueuePresentKHR(vulkanQueue, &presentInfo);

    numQueuedCommandBuffers = 0;

    //Resolve GPU Timestamp
    if (timestampsEnabled)
    {
        if (gpuTimestampManager->hasValidQueries())
        {
            //Query GPU for timestamps.
            const uint32_t queryOffset = (currentFrame * gpuTimestampManager->queriesPerFrame) * 2;
            const uint32_t queryCount = gpuTimestampManager->currentQuery * 2;
            vkGetQueryPoolResults(vulkanDevice, vulkanTimestampQueryPool, queryOffset, queryCount, sizeof(uint64_t) * queryCount * 2,
                &gpuTimestampManager->timestampsData[queryOffset], sizeof(gpuTimestampManager->timestampsData[0]),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

            //Calcute and cache the elapsed time.
            for (uint32_t i = 0; i < gpuTimestampManager->currentQuery; ++i)
            {
                uint32_t index = (currentFrame * gpuTimestampManager->queriesPerFrame) + i;

                GPUTimestamp& timestamp = gpuTimestampManager->timestamps[index];

                double start = static_cast<double>(gpuTimestampManager->timestampsData[(index * 2)]);
                double end = static_cast<double>(gpuTimestampManager->timestampsData[(index * 2) + 1]);
                double range = end - start;
                double elaspedTime = range * gpuTimestampFrequency;

                timestamp.elapsedMS = elaspedTime;
                timestamp.frameIndex = absoluteFrame;
            }
        }
        else if (gpuTimestampManager->currentQuery)
        {
            vprint("Asymmetrical GPU queries, missing a pop of some markers.\n");
        }

        gpuTimestampManager->reset();
        gpuTimestampReset = true;
    }
    else
    {
        gpuTimestampReset = false;
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized)
    {
        resized = false;
        resizeSwapchain();

        //Advanced frame counters that are skipped during this frame.
        frameCountersAdvanced();

        return;
    }

    //This is called inside resizedSwapchain as well to work correctly.
    frameCountersAdvanced();


    //Resource deletion  using reverse iteration and swap with last element.
    if (resourceDeletionQueue.size > 0)
    {
        for (int32_t i = resourceDeletionQueue.size - 1; i >= 0; --i)
        {
            ResourceUpdate& resourceDeletion = resourceDeletionQueue[i];

            if (resourceDeletion.currentFrame == currentFrame)
            {
                switch (resourceDeletion.type)
                {
                case ResourceDeletion::Types::BUFFER:
                    destroyBufferInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::PIPELINE:
                    destroyPipelineInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::RENDER_PASS:
                    destroyRenderPassInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::DESCRIPTOR_SET:
                    destroyDescriptorSetInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::DESCRIPTOR_SET_LAYOUT:
                    destroyDescriptorSetLayoutInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::SAMPLER:
                    destroySamplerInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::SHADER_STATE:
                    destroyShaderStateInstant(resourceDeletion.handle);
                    break;
                case ResourceDeletion::Types::TEXTURE:
                    destroyTextureInstant(resourceDeletion.handle);
                    break;
                }

                //Mark resource as free
                resourceDeletion.currentFrame = UINT32_MAX;
                //swap element
                resourceDeletionQueue.deleteSwap(i);
            }
        }
    }
}

void GPUDevice::resize(uint16_t width, uint16_t height)
{
    swapchainWidth = width;
    swapchainHeight = height;

    resized = true;
}

void GPUDevice::fillBarrier(RenderPassHandle renderPass, ExecutionBarrier& outBarrier)
{
    RenderPass* vkRenderPass = accessRenderPass(renderPass);

    outBarrier.numImageBarriers = 0;

    if (vkRenderPass)
    {
        const uint32_t rts = vkRenderPass->numRenderTargets;
        for (uint32_t i = 0; i < rts; ++i)
        {
            outBarrier.imageBarriers[outBarrier.numImageBarriers++].texture = vkRenderPass->outputTextures[i];
        }

        if (vkRenderPass->outputDepth.index != INVALID_INDEX)
        {
            outBarrier.imageBarriers[outBarrier.numImageBarriers++].texture = vkRenderPass->outputDepth;
        }
    }
}

//Returns a vertex buffer usable for fullscreen that uses no vertices.
BufferHandle GPUDevice::getFullscreenVertexBuffer() const
{
    return fullscreenVertexBuffer;
}

//Returns what is considered the final pass that writes to the swapchain.
RenderPassHandle GPUDevice::getSwapchainPass() const
{
    return swapchainPass;
}

TextureHandle GPUDevice::getDummyTexture() const
{
    return dummyTexture;
}

BufferHandle GPUDevice::getDummyConstantBuffer() const
{
    return dummyConstantBuffer;
}

const RenderPassOutput& GPUDevice::getSwapchainOutput() const
{
    return swapchainOutput;
}

VkRenderPass GPUDevice::getVulkanRenderPass(const RenderPassOutput& output, const char* name)
{
    //We are hashing the memory output and trying to find a compatible VkRenderPass.
    //In the current form RenderPassOutput should track everything needed including load operations.
    uint64_t hashedMemory = hashBytes((void*)(&output), sizeof(RenderPassOutput));
    VkRenderPass vulkanRenderPass = renderPassCache.get(hashedMemory);
    if (vulkanRenderPass)
    {
        return vulkanRenderPass;
    }
    vulkanRenderPass = vulkanCreateRenderPass(*this, output, name);
    renderPassCache.insert(hashedMemory, vulkanRenderPass);

    return vulkanRenderPass;
}

//Names and markers
void GPUDevice::setResourceName(VkObjectType objectType, uint64_t handle, const char* name)
{
    if (debugUtilsExtensionPresent == false)
    {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = handle;
    nameInfo.pObjectName = name;

    pfnSetDebugUtilsObjectNameEXT(vulkanDevice, &nameInfo);
}

void GPUDevice::pushMarker(VkCommandBuffer commandBuffer, const char* name)
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = 1.0f;
    label.color[1] = 1.0f;
    label.color[2] = 1.0f;
    label.color[3] = 1.0f;
    pfnCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
}

void GPUDevice::popMarker(VkCommandBuffer commandBuffer)
{
    pfnCmdEndDebugUtilsLabelEXT(commandBuffer);
}

//GPU timings
void GPUDevice::setGPUTimestampsEnable(bool flag)
{
    timestampsEnabled = flag;
}

uint32_t GPUDevice::getGPUTimestamps(GPUTimestamp* outTimestamps)
{
    return gpuTimestampManager->resolve(previousFrame, outTimestamps);
}

void GPUDevice::pushGPUTimestamp(CommandBuffer* commandBuffer, const char* name)
{
    if (timestampsEnabled == false)
    {
        return;
    }

    uint32_t queryIndex = gpuTimestampManager->push(currentFrame, name);
    vkCmdWriteTimestamp(commandBuffer->vkCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vulkanTimestampQueryPool, queryIndex);
}

void GPUDevice::popGPUTimestamp(CommandBuffer* commandBuffer)
{
    if (timestampsEnabled == false)
    {
        return;
    }

    uint32_t queryIndex = gpuTimestampManager->pop(currentFrame);
    vkCmdWriteTimestamp(commandBuffer->vkCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, vulkanTimestampQueryPool, queryIndex);
}

//Instant functions
//Real destructor methods - the other enqueue only resources.
void GPUDevice::destroyBufferInstant(uint32_t buffer)
{
    Buffer* buff = reinterpret_cast<Buffer*>(buffers.accessResource(buffer));

    if (buff && buff->parentBuffer.index == INVALID_BUFFER.index)
    {
        vmaDestroyBuffer(VMAAllocator, buff->vkBuffer, buff->vmaAllocation);
    }

    buffers.releaseResource(buffer);
}

void GPUDevice::destroyTextureInstant(uint32_t texture)
{
    Texture* text = reinterpret_cast<Texture*>(textures.accessResource(texture));

    if (text)
    {
        vkDestroyImageView(vulkanDevice, text->vkImageView, vulkanAllocationCallbacks);
        vmaDestroyImage(VMAAllocator, text->vkImage, text->vmaAllocation);
    }
    textures.releaseResource(texture);
}

void GPUDevice::destroyPipelineInstant(uint32_t pipeline)
{
    Pipeline* pipe = reinterpret_cast<Pipeline*>(pipelines.accessResource(pipeline));

    if (pipe)
    {
        vkDestroyPipeline(vulkanDevice, pipe->vkPipeline, vulkanAllocationCallbacks);
        vkDestroyPipelineLayout(vulkanDevice, pipe->vkPipelineLayout, vulkanAllocationCallbacks);
    }
    pipelines.releaseResource(pipeline);
}

void GPUDevice::destroySamplerInstant(uint32_t sampler)
{
    Sampler* samp = reinterpret_cast<Sampler*>(samplers.accessResource(sampler));

    if (samp)
    {
        vkDestroySampler(vulkanDevice, samp->vkSampler, vulkanAllocationCallbacks);
    }
    samplers.releaseResource(sampler);
}

void GPUDevice::destroyDescriptorSetLayoutInstant(uint32_t layout)
{
    DescriptorSetLayout* desSetLayout = reinterpret_cast<DescriptorSetLayout*>(descriptorSetLayouts.accessResource(layout));

    if (desSetLayout)
    {
        vkDestroyDescriptorSetLayout(vulkanDevice, desSetLayout->vkDescriptorSetLayout, vulkanAllocationCallbacks);
        //This contains also vkBinding allocation.
        void_free(desSetLayout->bindings, allocator);
    }
    descriptorSetLayouts.releaseResource(layout);
}

void GPUDevice::destroyDescriptorSetInstant(uint32_t set)
{
    DescriptorSet* descriptorSet = reinterpret_cast<DescriptorSet*>(descriptorSets.accessResource(set));

    if (descriptorSet)
    {
        //Contains the allocation for all the resources, binding and samplers arrays.
        void_free(descriptorSet->resources, allocator);
    }
    descriptorSets.releaseResource(set);
}

void GPUDevice::destroyRenderPassInstant(uint32_t renderPass)
{
    RenderPass* renPass = reinterpret_cast<RenderPass*>(renderPasses.accessResource(renderPass));

    if (renPass)
    {
        if (renPass->numRenderTargets) 
        {
            vkDestroyFramebuffer(vulkanDevice, renPass->vkFrameBuffer, vulkanAllocationCallbacks);
        }
    }
    renderPasses.releaseResource(renderPass);
}

void GPUDevice::destroyShaderStateInstant(uint32_t shader)
{
    ShaderState* shadState = reinterpret_cast<ShaderState*>(shaders.accessResource(shader));
    if (shadState)
    {
        for (uint32_t i = 0; i < shadState->activeShaders; ++i)
        {
            vkDestroyShaderModule(vulkanDevice, shadState->shaderStateInfo[i].module, vulkanAllocationCallbacks);
        }
    }
    shaders.releaseResource(shader);
}

void GPUDevice::updateDescriptorSetInstant(const DescriptorSetUpdate& update)
{
    //Use a dummy descriptor set to delete the vulkan descriptor set handle.
    DescriptorSetHandle dummyDeleteDescriptorSetHandle = { descriptorSets.obtainResource() };
    DescriptorSet* dummyDeleteDescriptorSet = accessDescriptorSet(dummyDeleteDescriptorSetHandle);

    DescriptorSet* descriptorSet = accessDescriptorSet(update.descriptorSet);
    const DescriptorSetLayout* descriptorSetLayout = descriptorSet->layout;

    dummyDeleteDescriptorSet->vkDescriptorSet = descriptorSet->vkDescriptorSet;
    dummyDeleteDescriptorSet->bindings = nullptr;
    dummyDeleteDescriptorSet->resources = nullptr;
    dummyDeleteDescriptorSet->samplers = nullptr;
    dummyDeleteDescriptorSet->numResources = 0;

    destroyDescriptorSet(dummyDeleteDescriptorSetHandle);

    //Allocate the new descriptor set and update its content.
    VkWriteDescriptorSet descriptorWrite[8];
    VkDescriptorBufferInfo bufferInfo[8];
    VkDescriptorImageInfo imageInfo[8];

    Sampler* vkDefaultSampler = accessSampler(defaultSampler);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vulkanDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSet->layout->vkDescriptorSetLayout;
    vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &descriptorSet->vkDescriptorSet);

    uint32_t numResources = descriptorSetLayout->numBindings;
    vulkanFillWriteDescriptorSets(*this, descriptorSetLayout, descriptorSet->vkDescriptorSet, descriptorWrite,
                                    bufferInfo, imageInfo, vkDefaultSampler->vkSampler, numResources, descriptorSet->resources,
                                    descriptorSet->samplers, descriptorSet->bindings);

    vkUpdateDescriptorSets(vulkanDevice, numResources, descriptorWrite, 0, nullptr);
}

//Accesses
ShaderState* GPUDevice::accessShaderState(ShaderStateHandle shader)
{
    return reinterpret_cast<ShaderState*>(shaders.accessResource(shader.index));
}

const ShaderState* GPUDevice::accessShaderState(ShaderStateHandle shader) const
{
    return reinterpret_cast<const ShaderState*>(shaders.accessResource(shader.index));
}

Texture* GPUDevice::accessTexture(TextureHandle texture)
{
    return reinterpret_cast<Texture*>(textures.accessResource(texture.index));
}

const Texture* GPUDevice::accessTexture(TextureHandle texture) const
{
    return reinterpret_cast<const Texture*>(textures.accessResource(texture.index));
}

Buffer* GPUDevice::accessBuffer(BufferHandle buffer)
{
    return reinterpret_cast<Buffer*>(buffers.accessResource(buffer.index));
}

const Buffer* GPUDevice::accessBuffer(BufferHandle buffer) const
{
    return reinterpret_cast<const Buffer*>(buffers.accessResource(buffer.index));
}

Pipeline* GPUDevice::accessPipeline(PipelineHandle pipeline)
{
    return reinterpret_cast<Pipeline*>(pipelines.accessResource(pipeline.index));
}

const Pipeline* GPUDevice::accessPipeline(PipelineHandle pipeline) const
{
    return reinterpret_cast<const Pipeline*>(pipelines.accessResource(pipeline.index));
}

Sampler* GPUDevice::accessSampler(SamplerHandle sampler)
{
    return reinterpret_cast<Sampler*>(samplers.accessResource(sampler.index));
}

const Sampler* GPUDevice::accessSampler(SamplerHandle sampler) const
{
    return reinterpret_cast<const Sampler*>(samplers.accessResource(sampler.index));
}

DescriptorSetLayout* GPUDevice::accessDescriptorSetLayout(DescriptorSetLayoutHandle layout)
{
    return reinterpret_cast<DescriptorSetLayout*>(descriptorSetLayouts.accessResource(layout.index));
}

const DescriptorSetLayout* GPUDevice::accessDescriptorSetLayout(DescriptorSetLayoutHandle layout) const
{
    return reinterpret_cast<const DescriptorSetLayout*>(descriptorSetLayouts.accessResource(layout.index));
}

DescriptorSet* GPUDevice::accessDescriptorSet(DescriptorSetHandle set)
{
    return reinterpret_cast<DescriptorSet*>(descriptorSets.accessResource(set.index));
}

const DescriptorSet* GPUDevice::accessDescriptorSet(DescriptorSetHandle set) const
{
    return reinterpret_cast<const DescriptorSet*>(descriptorSets.accessResource(set.index));
}

RenderPass* GPUDevice::accessRenderPass(RenderPassHandle renderPass)
{
    return reinterpret_cast<RenderPass*>(renderPasses.accessResource(renderPass.index));
}

const RenderPass* GPUDevice::accessRenderPass(RenderPassHandle renderPass) const
{
    return reinterpret_cast<const RenderPass*>(renderPasses.accessResource(renderPass.index));
}
