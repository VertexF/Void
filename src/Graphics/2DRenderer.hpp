#ifndef RENDER_2D_HDR
#define RENDER_2D_HDR

#include "GPUDevice.hpp"
#include "GPUResources.hpp"
#include "CommandBuffer.hpp"

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

#include "ShaderData.hpp"

#include <SDL3/SDL_mouse.h>

struct SceneData2D
{
	mat4s ortho;
};

enum TextureAtlas
{
	ATLAS_TEST,

	ATLAS_COUNT
};

static const char* sAtlasPaths[COUNT] =
{
	"Assets/Textures/mainMenuUI.png"
};

struct GPUDevice;

struct Renderer2D
{
	void init(GPUDevice& inGPU);
	void loadTexture(TextureAtlas atlas);
	void addQuad(vec3s position, vec2s scale);
	void addQuad(vec3s position, vec2s scale, vec2s spriteSize, vec2s rowAndColumn, vec2s offset, TextureAtlas atlas);
	void loadBuffer();
	void drawQuad(CommandBuffer& commandBuffer);
	void shutdown();

	SceneData2D scene2d{};

	Camera camera2D;

	GPUDevice* gpu;

	int width;
	int height;

	PipelineHandle pipeline2D;
	DescriptorSetLayoutHandle descriptorSetLayout2D;
	BufferHandle positionalBDAHandle = INVALID_BUFFER;
	BufferHandle sceneBDAHandle = INVALID_BUFFER;

	uint32_t instanceCount = 0;
};

#endif // !RENDER_2D_HDR