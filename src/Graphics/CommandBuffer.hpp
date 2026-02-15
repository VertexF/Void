#ifndef COMMAND_BUFFER_HDR
#define COMMAND_BUFFER_HDR

#include "GPUDevice.hpp"

struct CommandBuffer 
{
    void init(VkQueueFlagBits newType, uint32_t newBufferSize, uint32_t newSubmitSize, bool newBaked);
    void terminate();

    //Command buffer interface
    void bindPass(RenderPassHandle handle);
    void bindPipeline(PipelineHandle handle);
    void bindVertexBuffer(BufferHandle handle, uint32_t binding, uint32_t offset);
    void bindIndexBuffer(BufferHandle handle, uint32_t offset, VkIndexType indexType);
    void bindDescriptorSet(DescriptorSetHandle* handle, uint32_t numLists, uint32_t* offsets, uint32_t numOffsets, uint32_t descriptorSetNumber);
    void bindlessDescriptorSet(uint32_t descriptorSetNumber);

    void setViewport(const Viewport* viewport);
    void setScissor(const Rect2DInt* rect);
        
    void clear(float red, float green, float blue, float alpha);
    void clearDepthStencil(float depth, uint8_t stencil);

    void draw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, 
                                          uint32_t firstInstance);
    void drawIndirect(BufferHandle handle, uint32_t offset, uint32_t stride);
    void drawIndexedIndirect(BufferHandle handle, uint32_t drawCount, uint32_t offset, uint32_t stride);

    void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ);
    void dispatchIndirect(BufferHandle handle, uint32_t offset);

    void barrier(const ExecutionBarrier& barrier);

    void fillBuffer(BufferHandle buffer, uint32_t offset, uint32_t size, uint32_t data);

    void pushMarker(const char* name);
    void popMarker();

    void reset();

    VkCommandBuffer vkCommandBuffer;
    GPUDevice* device;
    VkDescriptorSet vkDescriptorSets[16];

    RenderPass* currentRenderPass;
    Pipeline* currentPipeline;
    //0 = colour; 1 = depth stencil.
    VkClearValue clears[2];
    bool isRecording;

    uint32_t handle;

    uint32_t currentCommand;
    uint32_t resourceHandle;
    VkQueueFlagBits type = VK_QUEUE_GRAPHICS_BIT;
    uint32_t bufferSize = 0;
    uint32_t submitSize = 0;

    //If baked reset will affect only the read commands.
    bool baked = false;
};

#endif // !COMMAND_BUFFER_HDR
