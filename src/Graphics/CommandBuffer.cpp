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
    colourAttachment.imageView = device->vulkanSwapchainImageViews[device->currentFrame];
    colourAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    colourAttachment.resolveImageView = VK_NULL_HANDLE;
    colourAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colourAttachment.clearValue = clears[0];

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
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue = clears[1];

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
    //TODO: Do I need to make global vertex buffer stuff?
    if (buffer->parentBuffer.index != INVALID_INDEX)
    {
        Buffer* parentBuffer = device->accessBuffer(buffer->parentBuffer);
        vkBuffer = parentBuffer->vkBuffer;
        offsets[0] = buffer->globalOffset;
    }

    vkCmdBindVertexBuffers(vkCommandBuffer, binding, 1, &vkBuffer, offsets);
}

void CommandBuffer::bindIndexBuffer(BufferHandle bufferHandle, uint32_t offset, VkIndexType indexType)
{
    Buffer* buffer = device->accessBuffer(bufferHandle);

    VkBuffer vkBuffer = buffer->vkBuffer;
    //TODO: Do I need to make global vertex buffer stuff?
    if (buffer->parentBuffer.index != INVALID_INDEX)
    {
        Buffer* parentBuffer = device->accessBuffer(buffer->parentBuffer);
        vkBuffer = parentBuffer->vkBuffer;
        offset = buffer->globalOffset;
    }

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

void CommandBuffer::clear(float red, float green, float blue, float alpha)
{
    clears[0].color = { red, green, blue, alpha };
}

void CommandBuffer::clearDepthStencil(float depth, uint8_t stencil)
{
    clears[1].depthStencil.depth = depth;
    clears[1].depthStencil.stencil = stencil;
}

void CommandBuffer::draw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
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

void CommandBuffer::barrier(const ExecutionBarrier& barrier)
{
    static VkImageMemoryBarrier imageBarriers[8];
    //TODO: subpass
    if (barrier.newBarrierExperimental != UINT32_MAX) 
    {
        VkPipelineStageFlags sourceStageMask = 0;
        VkPipelineStageFlags destinationStageMask = 0;
        VkAccessFlags sourceAccessFlags = VK_ACCESS_NONE_KHR;
        VkAccessFlags destinationAccessFlags = VK_ACCESS_NONE_KHR;

        for (uint32_t i = 0; i < barrier.numImageBarriers; ++i) 
        {
            Texture* textureVulkan = device->accessTexture(barrier.imageBarriers[i].texture);

            VkImageMemoryBarrier& vkBarrier = imageBarriers[i];
            const bool isColour = !TextureFormat::hasDepthOrStencil(textureVulkan->vkFormat);

            VkImageMemoryBarrier* pImageBarrier = &vkBarrier;
            pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            pImageBarrier->pNext = nullptr;

            VkAccessFlags sourceAccesMask = barrier.sourcePipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ? 
                                                                            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT :
                                                                            0;

            VkAccessFlags destAccesMask = barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ?
                                                                                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT :
                                                                                0;
            VkImageLayout sourceImageLayout = barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ? 
                                                                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : 
                                                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkImageLayout desinationImageLayout = barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ?
                                                                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
                                                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (isColour == false) 
            {
                sourceAccesMask = barrier.sourcePipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ?
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT :
                    0;

                destAccesMask = barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ?
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT :
                    0;

                sourceImageLayout = barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ?
                                                                        VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL :
                                                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

                desinationImageLayout = barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT ?
                                                                            VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL :
                                                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }

            pImageBarrier->srcAccessMask = sourceAccesMask;
            pImageBarrier->dstAccessMask = destAccesMask;
            pImageBarrier->oldLayout = sourceImageLayout;
            pImageBarrier->newLayout = desinationImageLayout;

            pImageBarrier->image = textureVulkan->vkImage;
            pImageBarrier->subresourceRange.aspectMask = isColour ? 
                                                            VK_IMAGE_ASPECT_COLOR_BIT : 
                                                            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            pImageBarrier->subresourceRange.baseMipLevel = 0;
            pImageBarrier->subresourceRange.levelCount = 1;
            pImageBarrier->subresourceRange.baseArrayLayer = 0;
            pImageBarrier->subresourceRange.layerCount = 1;

            pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            
            sourceAccessFlags |= pImageBarrier->srcAccessMask;
            destinationAccessFlags |= pImageBarrier->dstAccessMask;
            
            vkBarrier.oldLayout = textureVulkan->vkImageLayout;
            textureVulkan->vkImageLayout = vkBarrier.newLayout;
        }

        static VkBufferMemoryBarrier bufferMemoryBarriers[8];
        for (uint32_t i = 0; i < barrier.numMemoryBarriers; ++i) 
        {
            VkBufferMemoryBarrier& vkBuffer = bufferMemoryBarriers[i];
            vkBuffer.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                
            Buffer* buffer = device->accessBuffer(barrier.memoryBarriers[i].buffer);

            vkBuffer.buffer = buffer->vkBuffer;
            vkBuffer.offset = 0;
            vkBuffer.size = buffer->size;

            vkBuffer.srcAccessMask = toAccessMask(barrier.sourcePipelineStage);
            vkBuffer.dstAccessMask = toAccessMask(barrier.destinationPipelineStage);

            sourceAccessFlags |= vkBuffer.srcAccessMask;
            destinationAccessFlags |= vkBuffer.dstAccessMask;

            vkBuffer.srcQueueFamilyIndex = 0;
            vkBuffer.dstQueueFamilyIndex = 0;
        }

        sourceStageMask = utilDeterminePipelineStageFlags(sourceAccessFlags, 
                                                            barrier.sourcePipelineStage & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT ?
                                                            VK_QUEUE_COMPUTE_BIT :
                                                            VK_QUEUE_GRAPHICS_BIT);
        destinationStageMask = utilDeterminePipelineStageFlags(destinationAccessFlags, 
                                                                barrier.destinationPipelineStage & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT ?
                                                                VK_QUEUE_COMPUTE_BIT :
                                                                VK_QUEUE_GRAPHICS_BIT);

        vkCmdPipelineBarrier(vkCommandBuffer, sourceStageMask, destinationAccessFlags, 0, 0, nullptr, 
                                barrier.numMemoryBarriers, bufferMemoryBarriers, barrier.numImageBarriers, imageBarriers);

        return;
    }

    VkImageLayout newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageLayout newDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAccessFlags sourceAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags sourceBufferAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags sourceDepthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAccessFlags destinationAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags destinationBufferAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags destinationDepthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    switch (barrier.destinationPipelineStage) 
    {
    case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
        break;
    case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
        newLayout = VK_IMAGE_LAYOUT_GENERAL;
        break;
    case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
        newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        newDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        destinationAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        destinationDepthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
        destinationBufferAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        break;
    }

    switch (barrier.sourcePipelineStage) 
    {
    case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
        break;
    case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
        break;
    case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
        sourceAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceDepthAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
        sourceBufferAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        break;
    }

    bool hasDepth = false;

    for (uint32_t i = 0; i < barrier.numImageBarriers; ++i) 
    {
        Texture* textureVulkan = device->accessTexture(barrier.imageBarriers[i].texture);

        VkImageMemoryBarrier& vkBarrier = imageBarriers[i];
        vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        const bool isColour = !TextureFormat::hasDepthOrStencil(textureVulkan->vkFormat);
        hasDepth = hasDepth || !isColour;

        vkBarrier.image = textureVulkan->vkImage;
        vkBarrier.subresourceRange.aspectMask = isColour ? 
                                                VK_IMAGE_ASPECT_COLOR_BIT : 
                                                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        vkBarrier.subresourceRange.baseMipLevel = 0;
        vkBarrier.subresourceRange.levelCount = 1;
        vkBarrier.subresourceRange.baseArrayLayer = 0;
        vkBarrier.subresourceRange.layerCount = 1;
            
        vkBarrier.oldLayout = textureVulkan->vkImageLayout;

        //Transitions over to..
        vkBarrier.newLayout = isColour ? newLayout : newDepthLayout;

        vkBarrier.srcAccessMask = isColour ? sourceAccessMask : sourceDepthAccessMask;
        vkBarrier.dstAccessMask = isColour ? destinationAccessMask : destinationDepthAccessMask;

        textureVulkan->vkImageLayout = vkBarrier.newLayout;
    }

    VkPipelineStageFlags sourceStageMask = barrier.sourcePipelineStage;
    VkPipelineStageFlags destinationStageMask = barrier.destinationPipelineStage;

    if (hasDepth) 
    {
        sourceStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        destinationStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    static VkBufferMemoryBarrier bufferMemoryBarriers[8];
    for (uint32_t i = 0; i < barrier.numMemoryBarriers; ++i) 
    {
        VkBufferMemoryBarrier& vkBarrier = bufferMemoryBarriers[i];
        vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

        Buffer* buffer = device->accessBuffer(barrier.memoryBarriers[i].buffer);

        vkBarrier.buffer = buffer->vkBuffer;
        vkBarrier.offset = 0;
        vkBarrier.size = buffer->size;
        vkBarrier.srcAccessMask = sourceBufferAccessMask;
        vkBarrier.dstAccessMask = destinationBufferAccessMask;

        vkBarrier.srcQueueFamilyIndex = 0;
        vkBarrier.dstQueueFamilyIndex = 0;
    }
        
    vkCmdPipelineBarrier(vkCommandBuffer, sourceStageMask, destinationStageMask, 0, 0, nullptr, 
                            barrier.numMemoryBarriers, bufferMemoryBarriers, barrier.numImageBarriers, imageBarriers);
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