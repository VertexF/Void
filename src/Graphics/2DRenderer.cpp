#include "2DRenderer.hpp"

#include "Application/Window.hpp"

#include <meshoptimizer.h>
#include "cglm/struct/cam.h"
#include "vender/stb_image.h"

namespace
{
    struct PushConstant 
    {
        VkDeviceAddress quadPostionAddress;
        VkDeviceAddress sceneAddress;
        VkDeviceAddress particleData;
    };

    struct QuadPositionData
    {
        mat4s transform;
        vec4s colour = vec4s{ 1.f, 1.f, 1.f, 1.f };
        vec2s texCoords[4] =
        {
            vec2s{ 0.f, 0.f },
            vec2s{ 1.f, 0.f },
            vec2s{ 1.f, 1.f },
            vec2s{ 0.f, 1.f }
        };

        uint32_t textureID = UINT16_MAX;
        vec3s billboardPosition;
    };

    struct QuadData
    {
        //per-instance.
        mat4s transform;
        uint32_t textureID;
        vec4s colour;

        //per-index.
        vec2s sprintSize;
        vec2s rowAndColumn;
        vec2s offset = { 1, 1 }; //offset-per-sprite
    };

    Array<QuadPositionData> quadData;
}

void Renderer2D::init(GPUDevice& inGPU)
{
    gpu = &inGPU;

    quadData.init(&MemoryService::instance()->systemAllocator, 16);

    //Debug renderer
    PipelineCreation pipelineCreation2D{};
    pipelineCreation2D.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineCreation2D.blendState.addBlendState().setColour(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD);

    //Shader state
    FileReadResult vert2D = fileReadBinary("Assets/Shaders/2DShader.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult frag2D = fileReadBinary("Assets/Shaders/2DShader.frag.spv", &MemoryService::instance()->scratchAllocator);

    pipelineCreation2D.shaders.setName("2DRenderPipeline")
        .addStage(vert2D.data, uint32_t(vert2D.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(frag2D.data, uint32_t(frag2D.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    pipelineCreation2D.pushConstants.createPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0, 24);

    pipelineCreation2D.rasterisation.cullMode = VK_CULL_MODE_NONE;

    //This descriptor set layout will be ran every frame
    pipelineCreation2D.addDescriptorSetLayout(gpu->bindlessDescriptorSetLayoutHandle);

    pipeline2D = gpu->createPipeline(pipelineCreation2D);
}

void Renderer2D::loadTexture(const char* filepath)
{
    TextureHandle textureResource;

    int comp;
    uint8_t mipLevels = 1;

    stbi_set_flip_vertically_on_load(1);
    uint8_t* imageData = stbi_load(filepath, &width, &height, &comp, 4);
    if (imageData == nullptr)
    {
        textureResource = INVALID_TEXTURE;
        VOID_ERROR("Error loading texture %s", filepath);
    }

    TextureCreation creation;
    creation.setData(imageData)
        .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1)
        .setName(filepath);

    textureAlasHandles = gpu->createTexture(creation);

    VOID_ASSERT(textureAlasHandles.index != INVALID_TEXTURE.index);

    free(imageData);
}

void Renderer2D::addQuad(vec3s position, vec2s scale)
{
    const mat4s translationMatrix = glms_translate_make(position);
    const mat4s scaleMatrix = glms_scale_make({ scale.x, scale.y, 1.f });
    const mat4s transform = glms_mat4_mul(translationMatrix, scaleMatrix);

    QuadPositionData data{};
    data.transform = transform;

    quadData.push(data);
    instanceCount++;
}

void Renderer2D::addQuad(vec3s position, vec2s scale, vec2s spriteSize, vec2s rowAndColumn, vec2s offset)
{
    const mat4s translationMatrix = glms_translate_make(position);
    const mat4s scaleMatrix = glms_scale_make({ scale.x, scale.y, 1.f });
    const mat4s transform = glms_mat4_mul(translationMatrix, scaleMatrix);

    vec2s min{};
    min.x = ((rowAndColumn.x * spriteSize.x) / width);
    min.y = ((rowAndColumn.y * spriteSize.y) / height);

    vec2s max{};
    max.x = ((rowAndColumn.x + offset.x) * spriteSize.x) / width;
    max.y = ((rowAndColumn.y + offset.y) * spriteSize.y) / height;

    QuadPositionData data{};
    data.textureID = textureAlasHandles.index;
    data.billboardPosition = vec3s{ 0.f, 1.f, 1.f };
    data.transform = transform;
    data.texCoords[0] = vec2s{ min.x, max.y };
    data.texCoords[1] = vec2s{ max.x, max.y };
    data.texCoords[2] = vec2s{ max.x, min.y };
    data.texCoords[3] = vec2s{ min.x, min.y };

    quadData.push(data);
    instanceCount++;
}

void Renderer2D::loadBuffer()
{
    camera2D.initOrthographic(-1.f, 1.f, (float)Window::instance()->width, (float)Window::instance()->height, 0.5f);
    scene2d.project = glms_ortho(0, (float)Window::instance()->width, 0, (float)Window::instance()->height, 0.f, 100.f);

    BufferCreation bufferCreation{};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        bufferCreation.reset()
            .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(QuadPositionData) * quadData.size)
            .setName("quadPosition")
            .setData(quadData.data);
        particleSetHandle[i] = gpu->createBindlessBuffer(bufferCreation);
    }

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(SceneData2D))
        .setName("sceneData2D")
        .setData(&scene2d);
    sceneBDAHandle = gpu->createBindlessBuffer(bufferCreation);
}

void Renderer2D::drawQuad(CommandBuffer& commandBuffer)
{
    commandBuffer.bindPipeline(pipeline2D);

    camera2D.updateUICamera();

    scene2d.project = camera2D.projection;

    commandBuffer.bindlessDescriptorSet(0);

    Buffer* quadPositionBuffer = gpu->accessBuffer(particleSetHandle[gpu->currentFrame]);
    Buffer* sceneBuffer = gpu->accessBuffer(sceneBDAHandle);

    vmaCopyMemoryToAllocation(gpu->VMAAllocator, &scene2d, sceneBuffer->vmaAllocation, 0, sizeof(SceneData2D));

    PushConstant pushConstants{};
    pushConstants.quadPostionAddress = quadPositionBuffer->bufferAddress;
    pushConstants.sceneAddress = sceneBuffer->bufferAddress;

    vkCmdPushConstants(commandBuffer.vkCommandBuffer, commandBuffer.currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

    commandBuffer.draw(6, instanceCount, 0, 0);
}

void Renderer2D::drawQuad3D(CommandBuffer& commandBuffer, const Camera& camera3D)
{
    commandBuffer.bindPipeline(pipeline2D);

    scene2d.project = camera3D.projection;
    scene2d.view = camera3D.view;

    commandBuffer.bindlessDescriptorSet(0);

    Buffer* quadPositionBuffer = gpu->accessBuffer(particleSetHandle[gpu->currentFrame]);
    Buffer* sceneBuffer = gpu->accessBuffer(sceneBDAHandle);

    vmaCopyMemoryToAllocation(gpu->VMAAllocator, quadData.data, quadPositionBuffer->vmaAllocation, 0, sizeof(QuadPositionData));
    vmaCopyMemoryToAllocation(gpu->VMAAllocator, &scene2d, sceneBuffer->vmaAllocation, 0, sizeof(SceneData2D));

    PushConstant pushConstants{};
    pushConstants.quadPostionAddress = quadPositionBuffer->bufferAddress;
    pushConstants.sceneAddress = sceneBuffer->bufferAddress;

    vkCmdPushConstants(commandBuffer.vkCommandBuffer, commandBuffer.currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

    commandBuffer.draw(6, instanceCount, 0, 0);
}

void Renderer2D::shutdown()
{
    quadData.shutdown();

    gpu->destroyTexture(textureAlasHandles);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        gpu->destroyBuffer(particleSetHandle[i]);
    }
    gpu->destroyBuffer(sceneBDAHandle);

    gpu->destroyPipeline(pipeline2D);
}