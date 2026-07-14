#ifndef PARTICLE_RENDERER_HDR
#define PARTICLE_RENDERER_HDR

#include "GPUDevice.hpp"
#include "GPUResources.hpp"
#include "CommandBuffer.hpp"
#include "ShaderData.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include "Foundation/File.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/Array.hpp"
#include "Foundation/Camera.hpp"

#include "Application/Input.hpp"
#include "Application/Window.hpp"

#include <SDL3/SDL_mouse.h>

struct GPUDevice;

struct ParticleRenderer
{
	void init(GPUDevice& inGPU);
	void loadTexture(const char* filepath);
	void addParticleSet(vec3s position, vec2s scale, vec2s spriteSize, vec2s rowAndColumn, vec2s offset);
	void loadBuffer();
	void setupDrawCalls();
	void runParticleCompute(CommandBuffer* commandBuffer);
	void drawParticles(CommandBuffer& commandBuffer, const Camera& camera3D);
	void shutdown();

	TextureHandle textureAlasHandles;

	SceneData2D scene2d{};

	GPUDevice* gpu;

	int width;
	int height;

	PipelineHandle particlePipeline;
	PipelineHandle particleComputePipeline;
	DescriptorSetLayoutHandle descriptorSetLayout2D;
	BufferHandle particleSetsHandle;
	BufferHandle sceneBDAHandle = INVALID_BUFFER;
};

#endif // !PARTICLE_RENDERER_HDR
