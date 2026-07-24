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
        float maxAge = 0;
        float padd[2];
    };

    Array<ParticleSet> particleSets;

    BufferHandle indirectCountBufferHandle = INVALID_BUFFER;
    BufferHandle indirectBufferHandle = INVALID_BUFFER;

    BufferHandle particleSetsHandle = INVALID_BUFFER;
    BufferHandle particleDataHandle = INVALID_BUFFER;

    BufferHandle activityBufferHandle = INVALID_BUFFER;
    BufferHandle sceneBDAHandle = INVALID_BUFFER;

    struct ParticleData
    {
        vec4s positionAndAge;
        //This is the bitfield "buffer" this is what the GPU Zen 3 makes into it's own buffer for faster access for light clustering or whatever.
        uint32_t particleSet;
        float padd[3];
    };

    constexpr const uint32_t particleCount = 40000;
    constexpr const uint32_t maxDrawCount = 101;
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
    FileReadResult vert2D = fileReadBinary("Assets/Shaders/particle.vert.spv", &MemoryService::instance()->scratchAllocator);
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

    //Particle draw call pipeline
    PipelineCreation drawCallCreation{};

    //Shader state
    FileReadResult particleDrawShader = fileReadBinary("Assets/Shaders/particleDrawCall.comp.spv", &MemoryService::instance()->scratchAllocator);

    drawCallCreation.shaders.setName("particleDrawShader")
        .addStage(particleDrawShader.data, uint32_t(particleDrawShader.size), VK_SHADER_STAGE_COMPUTE_BIT)
        .setSPVInput(true);

    drawCallCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticleDrawCallPushConstant));

    particleDrawCall = gpu->createPipeline(drawCallCreation);

    //Particle compute
    PipelineCreation particleUpdateCreation{};

    //Shader state
    FileReadResult particleUpdateShader = fileReadBinary("Assets/Shaders/particles.comp.spv", &MemoryService::instance()->scratchAllocator);

    particleUpdateCreation.shaders.setName("particleUpdateShader")
        .addStage(particleUpdateShader.data, uint32_t(particleUpdateShader.size), VK_SHADER_STAGE_COMPUTE_BIT)
        .setSPVInput(true);

    particleUpdateCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ParticlePushConstant));

    particleData = gpu->createPipeline(particleUpdateCreation);

    loadTexture("Assets/Textures/particles.png");

    vec2s spriteOffset = { .x = 1, .y = 1 };
    vec3s particleScale = { 0.1f, 0.1f, 0.1f };
    vec2s subSpriteSize = { 64.f, 64.f };

    addParticleSet({ 0.f, 8.f, 0.f }, particleScale, subSpriteSize, { 0, 3 }, spriteOffset);
    addParticleSet({ 0.f, 0.f, 0.f }, particleScale, subSpriteSize, { 2, 3 }, spriteOffset);
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

void ParticleRenderer::addParticleSet(vec3s position, vec3s scale, vec2s spriteSize, vec2s rowAndColumn, vec2s offset)
{
    const mat4s translationMatrix = glms_translate_make(position);
    const mat4s scaleMatrix = glms_scale_make(scale);
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
    data.colour = vec4s{ 1.f, 1.f, 1.f, 1.f };
    data.texCoords[0] = vec2s{ min.x, max.y };
    data.texCoords[1] = vec2s{ max.x, max.y };
    data.texCoords[2] = vec2s{ max.x, min.y };
    data.texCoords[3] = vec2s{ min.x, min.y };
    data.maxAge = 0.03f;

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
    
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, sizeof(uint32_t))
        .setName("Indirect Draw Count Buffer");
    indirectCountBufferHandle = gpu->createBindlessGPUBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, sizeof(VkDrawIndirectCommand) * maxDrawCount)
        .setName("Indirect Draw Buffer");
    indirectBufferHandle = gpu->createBindlessGPUBuffer(bufferCreation);

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(ParticleData) * particleCount)
        .setName("Particle buffer");
    particleDataHandle = gpu->createBindlessGPUBuffer(bufferCreation);

    uint32_t particleDataCount = particleCount;
    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(uint32_t))
        .setData(&particleDataCount)
        .setName("Activity buffer");
    activityBufferHandle = gpu->createBindlessBuffer(bufferCreation);
}

void ParticleRenderer::updateParticles(CommandBuffer* commandBuffer, uint32_t particleSet, float deltaTime, vec3s directionVec, mat4s view)
{
    VOID_ERROR("You need to transfer the memory types themselves as well as the pipelining.");

    Buffer* particleBuffer = gpu->accessBuffer(particleDataHandle);
    Buffer* activityBuffer = gpu->accessBuffer(activityBufferHandle);
    Buffer* particleSetBuffer = gpu->accessBuffer(particleSetsHandle);

    particleSets[0].transform = glms_translate_make(directionVec);//glms_mat4_mul(glms_translate_make(directionVec), glms_quat_mat4(camerRot));

    vmaCopyMemoryToAllocation(gpu->VMAAllocator, particleSets.data, particleSetBuffer->vmaAllocation, 0, sizeof(ParticleSet) * particleSets.size);

    VkBufferMemoryBarrier2 particleComputeBarrier{};
    particleComputeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    particleComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    particleComputeBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    particleComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    particleComputeBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    particleComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleComputeBarrier.buffer = particleBuffer->vkBuffer;
    particleComputeBarrier.offset = 0;
    particleComputeBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 particleActivityBarrier{};
    particleActivityBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    particleActivityBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    particleActivityBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    particleActivityBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    particleActivityBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
    particleActivityBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleActivityBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleActivityBarrier.buffer = activityBuffer->vkBuffer;
    particleActivityBarrier.offset = 0;
    particleActivityBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 computeBarriers[] =
    {
        particleComputeBarrier,
        particleActivityBarrier
    };

    VkDependencyInfo dependencyComputeInfo{};
    dependencyComputeInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyComputeInfo.bufferMemoryBarrierCount = 2;
    dependencyComputeInfo.pBufferMemoryBarriers = computeBarriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyComputeInfo);

    commandBuffer->bindPipeline(particleData);

    ParticlePushConstant pushConstants{};
    pushConstants.particleData = particleBuffer->bufferAddress;
    pushConstants.activeBufferData = activityBuffer->bufferAddress;
    pushConstants.particleSetData = particleSetBuffer->bufferAddress;
    pushConstants.delta = deltaTime;
    pushConstants.particleSet = particleSet;
    pushConstants.offset = 0;
    pushConstants.view = view;

    vkCmdPushConstants(commandBuffer->vkCommandBuffer, commandBuffer->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);

    vkCmdDispatch(commandBuffer->vkCommandBuffer, 500, 1, 1);

    ParticlePushConstant pushConstants1{};
    pushConstants1.particleData = particleBuffer->bufferAddress;
    pushConstants1.activeBufferData = activityBuffer->bufferAddress;
    pushConstants1.particleSetData = particleSetBuffer->bufferAddress;
    pushConstants1.delta = deltaTime;
    pushConstants1.particleSet = 1;
    pushConstants1.offset = 500;
    pushConstants1.view = view;

    vkCmdPushConstants(commandBuffer->vkCommandBuffer, commandBuffer->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants1), &pushConstants1);

    vkCmdDispatch(commandBuffer->vkCommandBuffer, 500, 1, 1);
}

void ParticleRenderer::createParticleDrawCalls(CommandBuffer* commandBuffer)
{
    Buffer* indirectBufferCount = gpu->accessBuffer(indirectCountBufferHandle);
    Buffer* indirectBuffer = gpu->accessBuffer(indirectBufferHandle);

    Buffer* particleBuffer = gpu->accessBuffer(particleDataHandle);
    Buffer* activityBuffer = gpu->accessBuffer(activityBufferHandle);

    VkBufferMemoryBarrier2 particleBarrier{};
    particleBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    particleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    particleBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    particleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    particleBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    particleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleBarrier.buffer = particleBuffer->vkBuffer;
    particleBarrier.offset = 0;
    particleBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 particleActivityBarrier2{};
    particleActivityBarrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    particleActivityBarrier2.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    particleActivityBarrier2.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    particleActivityBarrier2.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    particleActivityBarrier2.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    particleActivityBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleActivityBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleActivityBarrier2.buffer = activityBuffer->vkBuffer;
    particleActivityBarrier2.offset = 0;
    particleActivityBarrier2.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    bufferBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 beginningBarriers[] =
    {
        particleBarrier,
        particleActivityBarrier2,
        bufferBarrier
    };

    VkDependencyInfo beginningDependencyInfo{};
    beginningDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    beginningDependencyInfo.bufferMemoryBarrierCount = 3;
    beginningDependencyInfo.pBufferMemoryBarriers = beginningBarriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &beginningDependencyInfo);

    vkCmdFillBuffer(commandBuffer->vkCommandBuffer, indirectBufferCount->vkBuffer, 0, 4, 0);

    VkBufferMemoryBarrier2 bufferComputeBarrier{};
    bufferComputeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    bufferComputeBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
    bufferComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bufferComputeBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeBarrier.buffer = indirectBuffer->vkBuffer;
    bufferComputeBarrier.offset = 0;
    bufferComputeBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferComputeCountBarrier{};
    bufferComputeCountBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferComputeCountBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    bufferComputeCountBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bufferComputeCountBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bufferComputeCountBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferComputeCountBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeCountBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeCountBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferComputeCountBarrier.offset = 0;
    bufferComputeCountBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 computeBarriers[] =
    {
        bufferComputeBarrier,
        bufferComputeCountBarrier
    };

    VkDependencyInfo dependencyComputeInfo{};
    dependencyComputeInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyComputeInfo.bufferMemoryBarrierCount = 2;
    dependencyComputeInfo.pBufferMemoryBarriers = computeBarriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyComputeInfo);

    ParticleDrawCallPushConstant pushConstants{};
    commandBuffer->bindPipeline(particleDrawCall);

    pushConstants.indirectAddress = indirectBuffer->bufferAddress;
    pushConstants.indirectCountAddress = indirectBufferCount->bufferAddress;
    pushConstants.activeBufferData = activityBuffer->bufferAddress;

    vkCmdPushConstants(commandBuffer->vkCommandBuffer, commandBuffer->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);

    vkCmdDispatch(commandBuffer->vkCommandBuffer, particleCount, 1, 1);

    VkBufferMemoryBarrier2 bufferIndirectBarrier{};
    bufferIndirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferIndirectBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bufferIndirectBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferIndirectBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    bufferIndirectBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT; //current we don't read thin the shader.*/ 
    bufferIndirectBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectBarrier.buffer = indirectBuffer->vkBuffer;
    bufferIndirectBarrier.offset = 0;
    bufferIndirectBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferIndirectCountBarrier{};
    bufferIndirectCountBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferIndirectCountBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bufferIndirectCountBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferIndirectCountBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    bufferIndirectCountBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    bufferIndirectCountBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectCountBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectCountBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferIndirectCountBarrier.offset = 0;
    bufferIndirectCountBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 barriers[] =
    {
        bufferIndirectBarrier,
        bufferIndirectCountBarrier
    };

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = 2;
    dependencyInfo.pBufferMemoryBarriers = barriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyInfo);
}

void ParticleRenderer::drawParticles(CommandBuffer& commandBuffer, const Camera& camera3D)
{
    Buffer* indirectBuffer = gpu->accessBuffer(indirectBufferHandle);
    Buffer* indirectCountBuffer = gpu->accessBuffer(indirectCountBufferHandle);

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

    vkCmdDrawIndirectCount(commandBuffer.vkCommandBuffer, indirectBuffer->vkBuffer, 0, indirectCountBuffer->vkBuffer, 0, maxDrawCount, sizeof(VkDrawIndirectCommand));
}

void ParticleRenderer::shutdown()
{
    particleSets.shutdown();

    gpu->destroyTexture(textureAlasHandles);
    gpu->destroyBuffer(particleSetsHandle);
    gpu->destroyBuffer(sceneBDAHandle);
    gpu->destroyBuffer(indirectBufferHandle);
    gpu->destroyBuffer(indirectCountBufferHandle);
    gpu->destroyBuffer(particleDataHandle);
    gpu->destroyBuffer(activityBufferHandle);

    gpu->destroyPipeline(particlePipeline);
    gpu->destroyPipeline(particleDrawCall);
    gpu->destroyPipeline(particleData);
}