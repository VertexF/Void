#include "CommandBuffer.hpp"
#include "GPUDevice.hpp"

#include "Application/Window.hpp"

namespace 
{
    VkAccessFlags toAccessMask(VkPipelineStageFlagBits stage) 
    {
        switch (stage) 
        {
        case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
            return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
            return VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

        case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
        {
            //Formerly known as return RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            VOID_ERROR("TODO: Check if this is valid for a VK_PIPELINE_STAGE_VERTEX_SHADER_BIT to an access mask of '0' AKA  VK_ACCESS_NONE.\n");
            return VK_ACCESS_NONE;
        }

        case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
        {
            //Formerly known as return RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            VOID_ERROR("TODO: Check if this is valid for a VK_PIPELINE_STAGE_VERTEX_SHADER_BIT to an access mask of '0' AKA  VK_ACCESS_NONE.\n");
            return VK_ACCESS_NONE;
        }

        case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        case VK_PIPELINE_STAGE_TRANSFER_BIT:
            return VK_ACCESS_TRANSFER_WRITE_BIT;

        default:
            VOID_ERROR("Pipeline stage to resource state is not supported %d", stage);
            return VK_ACCESS_FLAG_BITS_MAX_ENUM;
        }
    }
}

void CommandBuffer::init(VkQueueFlagBits newType, uint32_t newBufferSize, uint32_t newSubmitSize, bool newBaked)
{
    type = newType;
    bufferSize = newBufferSize;
    submitSize = newSubmitSize;
    baked = newBaked;

    reset();
}

void CommandBuffer::terminate()
{
    isRecording = false;
}

void CommandBuffer::beginRendering() 
{
    VkRenderingAttachmentInfo colourAttachment{};
    colourAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colourAttachment.pNext = nullptr;
    colourAttachment.imageView = device->vulkanSwapchainImageViews[device->vulkanImageIndex];
    colourAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    colourAttachment.resolveImageView = VK_NULL_HANDLE;
    colourAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colourAttachment.clearValue = { 0.7f, 0.7f, 0.7f, 1.f }; //0.7f, 0.9f, 1.f, 1.f = blue

    Texture* depthTexture = device->accessTexture(device->depthTexture);

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.pNext = nullptr;
    depthAttachment.imageView = depthTexture->vkImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    depthAttachment.resolveImageView = VK_NULL_HANDLE;
    depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue = { 0.f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { .extent{.width = Window::instance()->width, .height = Window::instance()->height }};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colourAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(vkCommandBuffer, &renderingInfo);
}

void CommandBuffer::bindPipeline(PipelineHandle pipelineHandle)
{
    Pipeline* pipeline = device->accessPipeline(pipelineHandle);
    vkCmdBindPipeline(vkCommandBuffer, pipeline->vkBindPoint, pipeline->vkPipeline);

    //Cache the pipeline.
    currentPipeline = pipeline;
}

void CommandBuffer::bindVertexBuffer(BufferHandle bufferHandle, uint32_t binding, uint32_t offset)
{
    Buffer* buffer = device->accessBuffer(bufferHandle);
    VkDeviceSize offsets[] = { offset };

    VkBuffer vkBuffer = buffer->vkBuffer;

    vkCmdBindVertexBuffers(vkCommandBuffer, binding, 1, &vkBuffer, offsets);
}

void CommandBuffer::bindIndexBuffer(BufferHandle bufferHandle, uint32_t offset, VkIndexType indexType)
{
    Buffer* buffer = device->accessBuffer(bufferHandle);

    VkBuffer vkBuffer = buffer->vkBuffer;
    vkCmdBindIndexBuffer(vkCommandBuffer, vkBuffer, offset, indexType);
}

void CommandBuffer::bindDescriptorSet(DescriptorSetHandle* bufferHandle, uint32_t numLists, uint32_t* /*offsets*/, uint32_t numOffsets, uint32_t descriptorSetNumber)
{
    uint32_t offsetsCache[8];
    numOffsets = 0;

    for (uint32_t numList = 0; numList < numLists; ++numList) 
    {
        DescriptorSet* descriptorSet = device->accessDescriptorSet(bufferHandle[numList]);
        vkDescriptorSets[numList] = descriptorSet->vkDescriptorSet;

        //Search for dynamic buffers
        const DescriptorSetLayout* descriptorSetLayout = descriptorSet->layout;
        for (uint32_t i = 0; i < descriptorSetLayout->numBindings; ++i)
        {
            const DescriptorBinding& rb = descriptorSetLayout->bindings[i];

            if (rb.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) 
            {
                //Search for the actual buffer offset.
                const uint32_t resourceIndex = descriptorSet->bindings[i];
                //uint32_t descriptorSetHandle = descriptorSet->resources[resourceIndex];
                Buffer* buffer = device->accessBuffer({ resourceIndex });

                offsetsCache[numOffsets++] = buffer->globalOffset;
            }
        }
    }

    vkCmdBindDescriptorSets(vkCommandBuffer, currentPipeline->vkBindPoint, currentPipeline->vkPipelineLayout, 
                            descriptorSetNumber, numLists, vkDescriptorSets, numOffsets, offsetsCache);
}

void CommandBuffer::bindlessDescriptorSet(uint32_t descriptorSetNumber)
{
    vkCmdBindDescriptorSets(vkCommandBuffer, currentPipeline->vkBindPoint, currentPipeline->vkPipelineLayout,
                            descriptorSetNumber, 1, &device->bindlessDescriptorSet, 0, nullptr);
}

void CommandBuffer::setViewport(const Viewport* viewport)
{
    VkViewport vkViewport;

    if (viewport) 
    {
        vkViewport.x = viewport->rect.x * 1.f;
        vkViewport.width = viewport->rect.width * 1.f;
        //We invert the Y with a negative and proper offset. Vulkan has unique Y clipping.
        vkViewport.y = viewport->rect.height * 1.f - viewport->rect.y;
        vkViewport.height = -viewport->rect.height * 1.f;
        vkViewport.minDepth = viewport->minDepth;
        vkViewport.maxDepth = viewport->maxDepth;
    }
    else 
    {
        vkViewport.x = 0.f;
        vkViewport.width = device->swapchainWidth * 1.f;

        //We invert the Y with a negative and proper offset. Vulkan has unique Y clipping.
        vkViewport.y = device->swapchainHeight * 1.f;
        vkViewport.height = -device->swapchainHeight * 1.f;
        
        vkViewport.minDepth = 0.f;
        vkViewport.maxDepth = 1.f;
    }

    vkCmdSetViewport(vkCommandBuffer, 0, 1, &vkViewport);
}

void CommandBuffer::setScissor(const Rect2DInt* rect)
{
    VkRect2D vkScissor;

    if (rect) 
    {
        vkScissor.offset.x = rect->x;
        vkScissor.offset.y = rect->y;
        vkScissor.extent.width = rect->width;
        vkScissor.extent.height = rect->height;
    }
    else 
    {
        vkScissor.offset.x = 0;
        vkScissor.offset.y = 0;
        vkScissor.extent.width = device->swapchainWidth;
        vkScissor.extent.height = device->swapchainHeight;
    }

    vkCmdSetScissor(vkCommandBuffer, 0, 1, &vkScissor);
}

void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    vkCmdDraw(vkCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    vkCmdDrawIndexed(vkCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::drawIndirect(BufferHandle bufferHandle, uint32_t offset, uint32_t stride)
{
    Buffer* buffer = device->accessBuffer(bufferHandle);
    VkBuffer vkBuffer = buffer->vkBuffer;
    VkDeviceSize vkOffset = offset;

    vkCmdDrawIndirect(vkCommandBuffer, vkBuffer, vkOffset, 1, stride);
}

void CommandBuffer::drawIndexedIndirect(BufferHandle bufferHandle, uint32_t drawCount, uint32_t offset, uint32_t stride)
{
    Buffer* buffer = device->accessBuffer(bufferHandle);
    VkBuffer vkBuffer = buffer->vkBuffer;
    VkDeviceSize vkOffset = offset;

    vkCmdDrawIndexedIndirect(vkCommandBuffer, vkBuffer, vkOffset, drawCount, stride);
}

void CommandBuffer::dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
{
    vkCmdDispatch(vkCommandBuffer, groupX, groupY, groupZ);
}

void CommandBuffer::dispatchIndirect(BufferHandle bufferHandle, uint32_t offset)
{
    Buffer* buffer = device->accessBuffer(bufferHandle);
    VkBuffer vkBuffer = buffer->vkBuffer;
    VkDeviceSize vkOffset = offset;

    vkCmdDispatchIndirect(vkCommandBuffer, vkBuffer, vkOffset);
}

void CommandBuffer::fillBuffer(BufferHandle buffer, uint32_t offset, uint32_t size, uint32_t data)
{
    Buffer* vkBuffer = device->accessBuffer(buffer);
    vkCmdFillBuffer(vkCommandBuffer, vkBuffer->vkBuffer, VkDeviceSize(offset), size ? 
                                                                                VkDeviceSize(offset) : 
                                                                                VkDeviceSize(vkBuffer->size), 
                                                                                data);
}

void CommandBuffer::pushMarker(const char* name)
{
    device->pushGPUTimestamp(this, name);

    if (device->debugUtilsExtensionPresent == false) 
    {
        return;
    }

    device->pushMarker(vkCommandBuffer, name);
}

void CommandBuffer::popMarker()
{
    device->popGPUTimestamp(this);

    if (device->debugUtilsExtensionPresent == false) 
    {
        return;
    }

    device->popMarker(vkCommandBuffer);
}

void CommandBuffer::reset()
{
    isRecording = false;
    currentPipeline = nullptr;
    currentCommand = 0;
}