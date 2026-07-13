#include "ParticleRenderer.hpp"

#include "Application/Window.hpp"

#include <meshoptimizer.h>
#include "cglm/struct/cam.h"
#include "vender/stb_image.h"

namespace
{
    struct PushConstant 
    {
        VkDeviceAddress particleSetsAddress;
        VkDeviceAddress sceneAddress;
        VkDeviceAddress particleData;
    };

    struct ParticleSet
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
        float padd[3];
    };

    Array<ParticleSet> particleSets;
}

void ParticleRenderer::init(GPUDevice& inGPU)
{
    gpu = &inGPU;

    particleSets.init(&MemoryService::instance()->systemAllocator, 16);

    //Debug renderer
    PipelineCreation particlePipelineCreation{};
    particlePipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    particlePipelineCreation.blendState.addBlendState().setColour(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD);

    //Shader state
    FileReadResult vert2D = fileReadBinary("Assets/Shaders/particleNew.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult frag2D = fileReadBinary("Assets/Shaders/2DShader.frag.spv", &MemoryService::instance()->scratchAllocator);

    particlePipelineCreation.shaders.setName("particlePipeline")
        .addStage(vert2D.data, uint32_t(vert2D.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(frag2D.data, uint32_t(frag2D.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    particlePipelineCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0, 24);

    particlePipelineCreation.rasterisation.cullMode = VK_CULL_MODE_NONE;

    //This descriptor set layout will be ran every frame
    particlePipelineCreation.addDescriptorSetLayout(gpu->bindlessDescriptorSetLayoutHandle);

    particlePipeline = gpu->createPipeline(particlePipelineCreation);

    loadTexture("Assets/Textures/particles.png");

    vec2s spriteOffset = { .x = 1, .y = 1 };
    vec2s buttonSize = { 256.f, 256.f };
    vec2s subSpriteSize = { 64.f, 64.f };

    addParticleSet({ 0.f, 0.f, 0.f }, buttonSize, subSpriteSize, { 0, 3 }, spriteOffset);
    addParticleSet({ 0.f, 0.f, 0.f }, buttonSize, subSpriteSize, { 1, 3 }, spriteOffset);
    loadBuffer();
}

void ParticleRenderer::loadTexture(const char* filepath)
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

void ParticleRenderer::addParticleSet(vec3s position, vec2s scale, vec2s spriteSize, vec2s rowAndColumn, vec2s offset)
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

    ParticleSet data{};
    data.textureID = textureAlasHandles.index;
    data.transform = transform;
    data.texCoords[0] = vec2s{ min.x, max.y };
    data.texCoords[1] = vec2s{ max.x, max.y };
    data.texCoords[2] = vec2s{ max.x, min.y };
    data.texCoords[3] = vec2s{ min.x, min.y };

    particleSets.push(data);
}

void ParticleRenderer::loadBuffer()
{
    BufferCreation bufferCreation{};
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(ParticleSet) * particleSets.size)
        .setName("quadPosition")
        .setData(particleSets.data);
    particleSetsHandle = gpu->createBindlessBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(SceneData2D))
        .setName("sceneData2D");
    sceneBDAHandle = gpu->createBindlessBuffer(bufferCreation);
}

void ParticleRenderer::drawParticles(CommandBuffer& commandBuffer, const Camera& camera3D, BufferHandle indirect, BufferHandle indirectCount, BufferHandle particleDataHandle)
{
    Buffer* indirectBuffer = gpu->accessBuffer(indirect);
    Buffer* indirectCountBuffer = gpu->accessBuffer(indirectCount);

    commandBuffer.bindPipeline(particlePipeline);

    scene2d.project = camera3D.projection;
    scene2d.view = camera3D.view;

    commandBuffer.bindlessDescriptorSet(0);

    Buffer* particleSetBuffer = gpu->accessBuffer(particleSetsHandle);
    Buffer* sceneBuffer = gpu->accessBuffer(sceneBDAHandle);
    Buffer* particleDataBuffer = gpu->accessBuffer(particleDataHandle);

    vmaCopyMemoryToAllocation(gpu->VMAAllocator, &scene2d, sceneBuffer->vmaAllocation, 0, sizeof(SceneData2D));

    PushConstant pushConstants{};
    pushConstants.particleSetsAddress = particleSetBuffer->bufferAddress;
    pushConstants.sceneAddress = sceneBuffer->bufferAddress;
    pushConstants.particleData = particleDataBuffer->bufferAddress;

    vkCmdPushConstants(commandBuffer.vkCommandBuffer, commandBuffer.currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

    vkCmdDrawIndirectCount(commandBuffer.vkCommandBuffer, indirectBuffer->vkBuffer, 0, indirectCountBuffer->vkBuffer, 0, 1, sizeof(VkDrawIndirectCommand));
}

void ParticleRenderer::shutdown()
{
    particleSets.shutdown();

    gpu->destroyTexture(textureAlasHandles);
    gpu->destroyBuffer(particleSetsHandle);
    gpu->destroyBuffer(sceneBDAHandle);

    gpu->destroyPipeline(particlePipeline);
}