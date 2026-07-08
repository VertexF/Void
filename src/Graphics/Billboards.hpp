#ifndef BILLBOARDS_HDR
#define BILLBOARDS_HDR

#include "GPUResources.hpp"
#include "GPUDevice.hpp"
#include "CommandBuffer.hpp"

struct Billboard
{
	void init(GPUDevice& inGPU);
	void loadTexture(TextureAtlas atlas);
	void loadBuffer();
	void drawQuad(CommandBuffer& commandBuffer);
	void shutdown();

	GPUDevice* gpu;

	int width;
	int height;

	PipelineHandle pipeline2D;
	DescriptorSetLayoutHandle descriptorSetLayout2D;
	BufferHandle positionalBDAHandle = INVALID_BUFFER;
	BufferHandle sceneBDAHandle = INVALID_BUFFER;

	uint32_t instanceCount = 0;
	PipelineHandle billboardHandle;
};

#endif // !BILLBOARDS_HDR
