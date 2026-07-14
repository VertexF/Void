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

    BufferHandle indirectCountBufferHandle = INVALID_BUFFER;
    BufferHandle indirectBufferHandle = INVALID_BUFFER;
    BufferHandle particleDataHandle = INVALID_BUFFER;

    struct ParticleData
    {
        vec3s position;
        uint32_t particleSet;
    };

    constexpr const uint32_t particleCount = 20000;
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

    //Particle compute
    PipelineCreation computeParticlePipelineCreation{};

    //Shader state
    FileReadResult particleComp = fileReadBinary("Assets/Shaders/particles.comp.spv", &MemoryService::instance()->scratchAllocator);

    computeParticlePipelineCreation.shaders.setName("debugRenderer")
        .addStage(particleComp.data, uint32_t(particleComp.size), VK_SHADER_STAGE_COMPUTE_BIT)
        .setSPVInput(true);

    computeParticlePipelineCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, 24);

    particleComputePipeline = gpu->createPipeline(computeParticlePipelineCreation);

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

    setupDrawCalls();
}

void ParticleRenderer::setupDrawCalls()
{
    BufferHandle drawCountHandle = { gpu->buffers.obtainResource() };
    if (drawCountHandle.index == INVALID_INDEX)
    {
        indirectCountBufferHandle = INVALID_BUFFER;
        return;
    }

    Buffer* bufferCount = gpu->accessBuffer(drawCountHandle);
    bufferCount->name = "Indirect Draw Count Buffer";
    bufferCount->size = 4;
    bufferCount->typeFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bufferCount->handle = drawCountHandle;
    bufferCount->globalOffset = 0;

    VkBufferCreateInfo bufferInfoCount{};
    bufferInfoCount.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfoCount.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bufferInfoCount.size = 4;

    VmaAllocationCreateInfo memoryInfoCount{};
    memoryInfoCount.usage = VMA_MEMORY_USAGE_AUTO;
    memoryInfoCount.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VmaAllocationInfo allocationInfoCount{};
    VkResult result = vmaCreateBuffer(gpu->VMAAllocator, &bufferInfoCount, &memoryInfoCount, &bufferCount->vkBuffer, &bufferCount->vmaAllocation, &allocationInfoCount);
    VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Asset Code %u", result)

        gpu->setResourceName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bufferCount->vkBuffer), "Indirect Draw Count Buffer");
    bufferCount->vkDeviceMemory = allocationInfoCount.deviceMemory;

    VkBufferDeviceAddressInfo bufferBDAInfoCount{};
    bufferBDAInfoCount.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferBDAInfoCount.buffer = bufferCount->vkBuffer;

    bufferCount->bufferAddress = vkGetBufferDeviceAddress(gpu->vulkanDevice, &bufferBDAInfoCount);

    indirectCountBufferHandle = drawCountHandle;

    BufferHandle drawHandle = { gpu->buffers.obtainResource() };
    if (drawHandle.index == INVALID_INDEX)
    {
        indirectBufferHandle = INVALID_BUFFER;
        return;
    }

    Buffer* bufferIndirect = gpu->accessBuffer(drawHandle);
    bufferIndirect->name = "Indirect Draw Buffer";
    //I don't know why yet you would want to make a draw command this large.
    bufferIndirect->size = 128 * 1024 * 1024;
    bufferIndirect->typeFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    bufferIndirect->handle = drawHandle;
    bufferIndirect->globalOffset = 0;

    VkBufferCreateInfo bufferInfoIndirect{};
    bufferInfoIndirect.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfoIndirect.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    //I don't know why yet you would want to make a draw command this large.
    bufferInfoIndirect.size = 128 * 1024 * 1024;

    VmaAllocationCreateInfo memoryInfoIndirect{};
    memoryInfoIndirect.usage = VMA_MEMORY_USAGE_AUTO;
    memoryInfoIndirect.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VmaAllocationInfo allocationInfoIndirect{};
    result = vmaCreateBuffer(gpu->VMAAllocator, &bufferInfoIndirect, &memoryInfoIndirect, &bufferIndirect->vkBuffer, &bufferIndirect->vmaAllocation, &allocationInfoIndirect);
    VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Asset Code %u", result)

        gpu->setResourceName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bufferIndirect->vkBuffer), "Indirect Draw Buffer");
    bufferIndirect->vkDeviceMemory = allocationInfoIndirect.deviceMemory;

    VkBufferDeviceAddressInfo bufferBDAInfoIndirect{};
    bufferBDAInfoIndirect.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferBDAInfoIndirect.buffer = bufferIndirect->vkBuffer;

    bufferIndirect->bufferAddress = vkGetBufferDeviceAddress(gpu->vulkanDevice, &bufferBDAInfoIndirect);

    indirectBufferHandle = drawHandle;

    BufferHandle particleHandle = { gpu->buffers.obtainResource() };
    if (particleHandle.index == INVALID_INDEX)
    {
        indirectCountBufferHandle = INVALID_BUFFER;
        return;
    }

    Buffer* partcleBufferData = gpu->accessBuffer(particleHandle);
    partcleBufferData->name = "Particle buffer";
    partcleBufferData->size = sizeof(ParticleData) * particleCount;
    partcleBufferData->typeFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    partcleBufferData->handle = particleHandle;
    partcleBufferData->globalOffset = 0;

    VkBufferCreateInfo bufferInfoParticle{};
    bufferInfoParticle.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfoParticle.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfoParticle.size = sizeof(ParticleData) * particleCount;

    VmaAllocationCreateInfo memoryInfoParticle{};
    memoryInfoParticle.usage = VMA_MEMORY_USAGE_AUTO;
    memoryInfoParticle.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VmaAllocationInfo allocationInfoParticle{};
    result = vmaCreateBuffer(gpu->VMAAllocator, &bufferInfoParticle, &memoryInfoParticle, &partcleBufferData->vkBuffer, &partcleBufferData->vmaAllocation, &allocationInfoParticle);
    VOID_ASSERTM(result == VK_SUCCESS, "Vulkan Asset Code %u", result)

        gpu->setResourceName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(partcleBufferData->vkBuffer), "Particle Buffer");
    partcleBufferData->vkDeviceMemory = allocationInfoParticle.deviceMemory;

    VkBufferDeviceAddressInfo bufferBDAInfoParticle{};
    bufferBDAInfoParticle.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferBDAInfoParticle.buffer = partcleBufferData->vkBuffer;

    partcleBufferData->bufferAddress = vkGetBufferDeviceAddress(gpu->vulkanDevice, &bufferBDAInfoParticle);

    particleDataHandle = particleHandle;
}

void ParticleRenderer::runParticleCompute(CommandBuffer* commandBuffer)
{
    Buffer* indirectBufferCount = gpu->accessBuffer(indirectCountBufferHandle);
    Buffer* indirectBuffer = gpu->accessBuffer(indirectBufferHandle);
    Buffer* particleBuffer = gpu->accessBuffer(particleDataHandle);

    VkBufferMemoryBarrier2 bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    VkDependencyInfo dependencyInfoCount{};
    dependencyInfoCount.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfoCount.bufferMemoryBarrierCount = 1;
    dependencyInfoCount.pBufferMemoryBarriers = &bufferBarrier;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyInfoCount);

    vkCmdFillBuffer(commandBuffer->vkCommandBuffer, indirectBufferCount->vkBuffer, 0, 4, 0);

    VkBufferMemoryBarrier2 bufferComputeBarrier{};
    bufferComputeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    bufferComputeBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    bufferComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferComputeBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeBarrier.buffer = indirectBuffer->vkBuffer;
    bufferComputeBarrier.offset = 0;
    bufferComputeBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferComputeCountBarrier{};
    bufferComputeCountBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferComputeCountBarrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferComputeCountBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferComputeCountBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferComputeCountBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferComputeCountBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeCountBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferComputeCountBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferComputeCountBarrier.offset = 0;
    bufferComputeCountBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 particleComputeBarrier{};
    particleComputeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    particleComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    particleComputeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    particleComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    particleComputeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    particleComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleComputeBarrier.buffer = particleBuffer->vkBuffer;
    particleComputeBarrier.offset = 0;
    particleComputeBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 computeBarriers[] =
    {
        bufferComputeBarrier,
        bufferComputeCountBarrier,
        particleComputeBarrier
    };

    VkDependencyInfo dependencyComputeInfo{};
    dependencyComputeInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyComputeInfo.bufferMemoryBarrierCount = 3;
    dependencyComputeInfo.pBufferMemoryBarriers = computeBarriers;

    vkCmdPipelineBarrier2(commandBuffer->vkCommandBuffer, &dependencyComputeInfo);

    ComputePushConstant pushConstants{};
    uint32_t drawCount = 1;
    uint32_t computeLocalXID = 64;
    commandBuffer->bindPipeline(particleComputePipeline);

    pushConstants.indirectAddress = indirectBuffer->bufferAddress;
    pushConstants.indirectCountAddress = indirectBufferCount->bufferAddress;
    pushConstants.particleData = particleBuffer->bufferAddress;

    vkCmdPushConstants(commandBuffer->vkCommandBuffer, commandBuffer->currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), &pushConstants);

    uint32_t numberOfXThreads = (drawCount + computeLocalXID - 1) / computeLocalXID;

    vkCmdDispatch(commandBuffer->vkCommandBuffer, particleCount, 1, 1);

    VkBufferMemoryBarrier2 bufferIndirectBarrier{};
    bufferIndirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferIndirectBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferIndirectBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferIndirectBarrier.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    bufferIndirectBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT; //current we don't read thin the sahder.*/ 
    bufferIndirectBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectBarrier.buffer = indirectBuffer->vkBuffer;
    bufferIndirectBarrier.offset = 0;
    bufferIndirectBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 bufferIndirectCountBarrier{};
    bufferIndirectCountBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferIndirectCountBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferIndirectCountBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferIndirectCountBarrier.dstStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    bufferIndirectCountBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    bufferIndirectCountBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectCountBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferIndirectCountBarrier.buffer = indirectBufferCount->vkBuffer;
    bufferIndirectCountBarrier.offset = 0;
    bufferIndirectCountBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 particleBarrier{};
    particleBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    particleBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    particleBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    particleBarrier.dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    particleBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    particleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    particleBarrier.buffer = particleBuffer->vkBuffer;
    particleBarrier.offset = 0;
    particleBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 barriers[] =
    {
        bufferIndirectBarrier,
        bufferIndirectCountBarrier,
        particleBarrier
    };

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = 3;
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

    vkCmdDrawIndirectCount(commandBuffer.vkCommandBuffer, indirectBuffer->vkBuffer, 0, indirectCountBuffer->vkBuffer, 0, 101, sizeof(VkDrawIndirectCommand));
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

    gpu->destroyPipeline(particlePipeline);
    gpu->destroyPipeline(particleComputePipeline);
}