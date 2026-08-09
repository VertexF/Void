#ifndef RENDER_2D_HDR
#define RENDER_2D_HDR

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
class ktxTexture2;
enum ktx_transcode_fmt_e;

struct Renderer2D
{
	void init(GPUDevice& inGPU);
	void addQuad(vec3s position, vec2s scale);
	void addQuad(vec3s position, vec2s scale, vec2s spriteSize, vec2s rowAndColumn, vec2s offset);
	void loadBuffer();
	void drawQuad(CommandBuffer& commandBuffer);
	void drawQuad3D(CommandBuffer& commandBuffer, const Camera& camera3D);
	void shutdown();

	TextureHandle textureAlasHandles;

	SceneData2D scene2d{};

	Camera camera2D;

	GPUDevice* gpu;

	uint32_t width;
	uint32_t height;

	PipelineHandle pipeline2D;
	DescriptorSetLayoutHandle descriptorSetLayout2D;
	BufferHandle particleSetHandle[FRAMES_IN_FLIGHT];
	BufferHandle sceneBDAHandle = INVALID_BUFFER;

	uint32_t instanceCount = 0;
};

#endif // !RENDER_2D_HDR
