#include "Skybox.hpp"

#include "Foundation/File.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"

#include "GPUDevice.hpp"
#include "Textures.hpp"

#include <vender/stb_image.h>

#include <ktx.h>
#include <ktxvulkan.h>

namespace
{
    //TODO: Figure out if you need this stuff.
    PipelineHandle skyboxPipeline;
    BufferHandle skyboxMaterialBuffer;
    DescriptorSetLayoutHandle skyboxDescriptorSetLayout;
    DescriptorSetHandle skyboxDescriptorSet;

    TextureHandle skyboxTextureHandle;
    SamplerHandle skyboxSampler;

    struct SkyboxData
    {
        vec3s testColour;
        uint32_t skyboxTextureIndex;
    };
}

void initSkybox(GPUDevice& gpu)
{
    //Depth
    PipelineCreation skyboxPipelineCreation{};
    skyboxPipelineCreation.depthStencil.setDepth(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //Shader state
    FileReadResult vertSkybox = fileReadBinary("Assets/Shaders/skybox.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragSkybox = fileReadBinary("Assets/Shaders/skybox.frag.spv", &MemoryService::instance()->scratchAllocator);

    skyboxPipelineCreation.shaders.setName("skybox")
        .addStage(vertSkybox.data, uint32_t(vertSkybox.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragSkybox.data, uint32_t(fragSkybox.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    skyboxPipelineCreation.pushConstants.createPushConstants(VK_SHADER_STAGE_VERTEX_BIT, 0, 24);

    //Descriptor set layout.
    DescriptorSetLayoutCreation skyboxSetLayout{};
    skyboxSetLayout
        .addBinding({ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .binding = 0, .count = 1, .stage = VK_SHADER_STAGE_ALL, .name = "SkyboxMaterial" })
        .setSetIndex(1);
    skyboxSetLayout.bindless = false;

    //Setting it into pipeline.
    //This descriptor set layout will be ran every draw calls
    skyboxDescriptorSetLayout = gpu.createDescriptorSetLayout(skyboxSetLayout);
    //This descriptor set layout will be ran every frame
    skyboxPipelineCreation.addDescriptorSetLayout(gpu.bindlessDescriptorSetLayoutHandle)
                          .addDescriptorSetLayout(skyboxDescriptorSetLayout);

    skyboxPipeline = gpu.createPipeline(skyboxPipelineCreation);

    skyboxTextureHandle = createACubemap(gpu, "Assets/Textures/orangeSpaceSkybox.ktx2");

    SamplerCreation skyboxSamplerCreation{};
    skyboxSamplerCreation.minFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.magFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSampler = gpu.createSampler(skyboxSamplerCreation);
    gpu.linkTextureSampler(skyboxTextureHandle, skyboxSampler);

    BufferCreation bufferCreation{};

    bufferCreation.reset()
        .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(SkyboxData))
        .setName("SkyboxData");
    skyboxMaterialBuffer = gpu.createBuffer(bufferCreation);

    DescriptorSetCreation dsCreation{};
    dsCreation.buffer(skyboxMaterialBuffer, 0);
    dsCreation.setLayout(skyboxDescriptorSetLayout);

    skyboxDescriptorSet = gpu.createDescriptorSet(dsCreation);
}

void drawSkybox(GPUDevice& gpu, CommandBuffer& gpuCommands, PushConstants pushConstants)
{
    //Skybox!
    gpuCommands.bindPipeline(skyboxPipeline);

    //Maybe we can make this non-dymanic after things are working?
    Buffer* skyboxMaterialDataBuffer = gpu.accessBuffer(skyboxMaterialBuffer);

    SkyboxData skyboxData{};
    skyboxData.skyboxTextureIndex = skyboxTextureHandle.index;
    skyboxData.testColour = vec3s{ 0.f, 1.f, 0.f };

    vmaCopyMemoryToAllocation(gpu.VMAAllocator, &skyboxData, skyboxMaterialDataBuffer->vmaAllocation, 0, sizeof(SkyboxData));

    vkCmdPushConstants(gpuCommands.vkCommandBuffer, gpuCommands.currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);

    gpuCommands.bindDescriptorSet(&skyboxDescriptorSet, 1, nullptr, 0, 1);
    gpuCommands.bindlessDescriptorSet(0);

    gpuCommands.draw(36, 1, 0, 0);
}

void shutdownSkybox(GPUDevice& gpu)
{
    gpu.destroyDescriptorSet(skyboxDescriptorSet);
    gpu.destroyBuffer(skyboxMaterialBuffer);

    gpu.destroySampler(skyboxSampler);
    gpu.destroyTexture(skyboxTextureHandle);

    gpu.destroyDescriptorSetLayout(skyboxDescriptorSetLayout);
    gpu.destroyPipeline(skyboxPipeline);
}
