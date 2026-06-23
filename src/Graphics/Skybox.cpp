#include "Skybox.hpp"

#include "Foundation/File.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"

#include "GPUDevice.hpp"

#include <vender/stb_image.h>

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

    //TODO: Move this to a place that make sense.
    TextureHandle createACubemap(GPUDevice& gpu, const Array<const char*>& images, Array<uint8_t*>& skyboxImageArray, const char* name)
    {
        int comp;
        int width;
        int height;

        for (uint32_t i = 0; i < images.size; ++i)
        {
            if (images[i])
            {
                stbi_set_flip_vertically_on_load(1);
                //Load 6 images.
                uint8_t* imageData = stbi_load(images[i], &width, &height, &comp, 4);
                if (imageData == nullptr)
                {
                    VOID_ERROR("Error loading texture %s", images[i]);
                    return INVALID_TEXTURE;
                }

                skyboxImageArray.push(imageData);
                //free(imageData);
            }
        }

        //Create the single texture.
        TextureCreation creation{};
        creation.setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE)
            .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(width), 1)
            .setImages(skyboxImageArray, skyboxImageArray.size)
            .setName(name);
        TextureHandle newTexture = gpu.createTexture(creation);

        return newTexture;
    }
}

void initSkybox(GPUDevice& gpu)
{
    //Depth
    PipelineCreation skyboxPipelineCreation{};
    skyboxPipelineCreation.depthStencil.depthEnable = true;
    skyboxPipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //Shader state
    FileReadResult vertSkybox = fileReadBinary("Assets/Shaders/skybox.vert.spv", &MemoryService::instance()->scratchAllocator);
    FileReadResult fragSkybox = fileReadBinary("Assets/Shaders/skybox.frag.spv", &MemoryService::instance()->scratchAllocator);

    skyboxPipelineCreation.shaders.setName("skybox")
        .addStage(vertSkybox.data, uint32_t(vertSkybox.size), VK_SHADER_STAGE_VERTEX_BIT)
        .addStage(fragSkybox.data, uint32_t(fragSkybox.size), VK_SHADER_STAGE_FRAGMENT_BIT)
        .setSPVInput(true);

    //Descriptor set layout.
    DescriptorSetLayoutCreation skyboxSetLayout{};
    skyboxSetLayout
        .addBinding({ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .binding = 0, .count = 1, .stage = VK_SHADER_STAGE_ALL, .name = "SkyboxMaterial" })
        .setSetIndex(0);
    skyboxSetLayout.bindless = false;

    //Setting it into pipeline.
    //This descriptor set layout will be ran every draw calls
    skyboxDescriptorSetLayout = gpu.createDescriptorSetLayout(skyboxSetLayout);
    //This descriptor set layout will be ran every frame
    skyboxPipelineCreation.addDescriptorSetLayout(skyboxDescriptorSetLayout)
        .addDescriptorSetLayout(gpu.bindlessDescriptorSetLayoutHandle);

    skyboxPipeline = gpu.createPipeline(skyboxPipelineCreation);

    Array<uint8_t*> skyboxImageArray;
    skyboxImageArray.init(&MemoryService::instance()->systemAllocator, 6);

    Array<const char*> cubemapsImage;
    cubemapsImage.init(&MemoryService::instance()->scratchAllocator, 6);
    cubemapsImage.push("Assets/Textures/4.png");
    cubemapsImage.push("Assets/Textures/2.png");
    cubemapsImage.push("Assets/Textures/6.png");
    cubemapsImage.push("Assets/Textures/5.png");
    cubemapsImage.push("Assets/Textures/1.png");
    cubemapsImage.push("Assets/Textures/3.png");

    skyboxTextureHandle = createACubemap(gpu, cubemapsImage, skyboxImageArray, "SpaceCubeMap");
    SamplerCreation skyboxSamplerCreation{};
    skyboxSamplerCreation.minFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.magFilter = VK_FILTER_LINEAR;
    skyboxSamplerCreation.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSamplerCreation.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    skyboxSampler = gpu.createSampler(skyboxSamplerCreation);
    gpu.linkTextureSampler(skyboxTextureHandle, skyboxSampler);

    skyboxImageArray.shutdown();

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

void drawSkybox(GPUDevice& gpu, CommandBuffer& gpuCommands, Buffer* globalSceneBuffer, PushConstants pushConstants, UniformData globalSceneData)
{
    //Skybox!
    gpuCommands.bindPipeline(skyboxPipeline);

    //Maybe we can make this non-dymanic after things are working?
    Buffer* skyboxMaterialDataBuffer = gpu.accessBuffer(skyboxMaterialBuffer);

    SkyboxData skyboxData{};
    skyboxData.skyboxTextureIndex = skyboxTextureHandle.index;
    skyboxData.testColour = vec3s{ 0.f, 1.f, 0.f };

    vmaCopyMemoryToAllocation(gpu.VMAAllocator, &skyboxData, skyboxMaterialDataBuffer->vmaAllocation, 0, sizeof(SkyboxData));

    vkCmdPushConstants(gpuCommands.vkCommandBuffer, gpuCommands.currentPipeline->vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

    gpuCommands.bindDescriptorSet(&skyboxDescriptorSet, 1, nullptr, 0, 0);
    gpuCommands.bindlessDescriptorSet(1);

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