#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/Keys.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/VoidImgui.hpp"
#include "Graphics/GPUProfiler.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include "vender/imgui/imgui.h"
//#include "vender/tracy/tracy/Tracy.hpp"

#include "Foundation/File.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/ResourceManager.hpp"
#include "Foundation/Time.hpp"

#include <cgltf.h>
#include <tlsf.h>

#include <stdlib.h>
#include <SDL3/SDL.h>
#include <stb_image.h>


//static const char* DEFAULT_3D_MODEL = "Assets/Models/2.0/Sponza/glTF/Sponza.gltf";
static const char* DEFAULT_3D_MODEL = "Assets/Models/out/Sponza5.glb";

//I might try to remove this later.
#define InjectDefault3DModel() \
if (fileExists(DEFAULT_3D_MODEL)) \
{\
    argc = 2;\
    argv[1] = const_cast<char*>(DEFAULT_3D_MODEL);\
}\
else \
{\
    vprint("Could not find file.");\
    exit(-1);\
}\

namespace
{
    //TODO: Figure out if you need this stuff.
    PipelineHandle cubePipeline;
    BufferHandle cubeCB;
    DescriptorSetLayoutHandle cubeDSL;
    //DescriptorSetLayoutHandle vertexDSL;

    enum MaterialFeatures
    {
        MaterialFeatures_ColourTexture = 1 << 0,
        MaterialFeatures_NormalTexture = 1 << 1,
        MaterialFeatures_RoughnessTexture = 1 << 2,
        MaterialFeatures_OcclusionTexture = 1 << 3,
        MaterialFeatures_EmissiveTexture = 1 << 4,

        MaterialFeatures_TangentVertexAttribute = 1 << 5,
        MaterialFeatures_TexcoordVertexAttribute = 1 << 6,
    };

    struct alignas(16) MaterialData
    {
        vec4s baseColourFactor;
        mat4s model;
        mat4s modelInv;

        vec3s emissiveFactor;
        float metallicFactor;

        float roughnessFactor;
        float occlusionFactor;
        uint32_t flags;
    };

    struct alignas(16) Vertices
    {
        float tangent[4];
        float position[3];
        float normals[3];
        float texCoord0[2];
    };

    struct MeshDraw
    {
        BufferHandle vertexBuffer;
        BufferHandle indexBuffer;
        BufferHandle materialBuffer;
        MaterialData materialData;

        uint32_t indexOffset;

        uint32_t count;

        VkIndexType indexType;

        DescriptorSetHandle descriptorSet;
    };

    struct alignas(16) UniformData
    {
        mat4s globalModel;
        mat4s viewPerspective;
        vec4s eye;
        vec4s light;
    };

    struct Transform
    {
        vec3s scale;
        versors rotation;
        vec3s translation;

        void reset() 
        {
            scale = vec3s{ 1.f, 1.f, 1.f };
            rotation = glms_quat_identity();
            translation = vec3s{ 1.f, 1.f, 1.f };
        }

        mat4s calculateMatrix() const
        {
            const mat4s translationMatrix = glms_translate_make(translation);
            const mat4s scaleMatrix = glms_scale_make(scale);
            const mat4s localMatrix = glms_mat4_mul(glms_mat4_mul(translationMatrix, glms_quat_mat4(rotation)), scaleMatrix);

            return localMatrix;
        }
    };
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        vprint("Setting the Sponza GLTF model.\n");
        InjectDefault3DModel();
    }

    //Init services
    MemoryService::instance()->init(void_giga(1ull), void_mega(8));
    timeServiceInit();

    HeapAllocator* allocator = &MemoryService::instance()->systemAllocator;
    StackAllocator scratchAllocator = MemoryService::instance()->scratchAllocator;

    Window::instance()->init(1280, 800, "Void Engine");

    InputHandler inputHandler{};
    inputHandler.init(allocator);

    DeviceCreation deviceCreation;
    deviceCreation.setWindow(Window::instance()->width, Window::instance()->height, Window::instance()->platformHandle)
        .setAllocator(allocator)
        .setLinearAllocator(&scratchAllocator);

    GPUDevice gpu;
    gpu.init(deviceCreation);

    ResourceManager resourceManager;
    resourceManager.init(allocator);

    GPUProfiler gpuProfiler;
    gpuProfiler.init(allocator, 100);

    Renderer renderer;
    renderer.init({ &gpu, allocator });
    renderer.setLoaders(&resourceManager);

    ImguiService* imgui = ImguiService::instance();
    ImguiServiceConfiguration imguiConfig = { &gpu, Window::instance()->platformHandle };
    imgui->init(&imguiConfig);

    //Window::instance()->setFullscreen(true);

    Directory cwd{};
    directoryCurrent(&cwd);

    char GLTFBasePath[512]{};
    memcpy(GLTFBasePath, argv[1], strlen(argv[1]));
    fileDirectoryFromPath(GLTFBasePath);

    directoryChange(GLTFBasePath);

    char GLTFFile[512]{};
    memcpy(GLTFFile, argv[1], strlen(argv[1]));
    fileNameFromPath(GLTFFile);

    cgltf_data* cgltfData = nullptr;

    cgltf_options options{};
    options.memory.alloc_func = tlsf_malloc;
    options.memory.free_func = tlsf_free;
    options.memory.user_data = allocator->TLSFHandle;
    cgltf_result result = cgltf_parse_file(&options, GLTFFile, &cgltfData);
    if (result != cgltf_result_success)
    {
        VOID_ERROR("File could not be found or loaded.");
    }

    result = cgltf_load_buffers(&options, cgltfData, GLTFFile);
    if (result != cgltf_result_success)
    {
        VOID_ERROR("Could not load buffers from the gltf mdoel");
    }

    result = cgltf_validate(cgltfData);
    if (result != cgltf_result_success)
    {
        VOID_ERROR("The gltf model is invalid");
    }

    Array<TextureResource> images2;
    images2.init(allocator, uint32_t(cgltfData->images_count));

    //GLB version.
    for (uint32_t imageIndex = 0; imageIndex < cgltfData->images_count; ++imageIndex)
    {
        cgltf_image image = cgltfData->images[imageIndex];

        if (image.uri != nullptr)
        {
            TextureResource* textureResource = renderer.createTexture(image.uri, image.uri);

            VOID_ASSERT(textureResource != nullptr);

            images2.push(*textureResource);
        }
        else 
        {
            int comp = 0;
            int width = 0;
            int height = 0;
            //uint8_t mipLevels = 1;

            uint8_t* rawBufferData = reinterpret_cast<uint8_t*>(image.buffer_view->buffer->data) + image.buffer_view->offset;
            stbi_info_from_memory(rawBufferData, int(image.buffer_view->size), &width, &height, &comp);

            //TODO: Add mipmap support later.
            //uint32_t w = width;
            //uint32_t h = height;

            //while (w > 1 && h > 1)
            //{
            //    w /= 2;
            //    h /= 2;

            //    ++mipLevels;
            //}

            int x;
            int y;
            uint8_t* textureData = stbi_load_from_memory(rawBufferData, int(image.buffer_view->size), &x, &y, &comp, 4);

            TextureCreation textureCreation{};
            textureCreation.setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
                .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1)
                .setData(textureData)
                .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .setName(nullptr);

            TextureResource* textureResource = renderer.createTexture(textureCreation);
            VOID_ASSERT(textureResource != nullptr);

            images2.push(*textureResource);

            stbi_image_free(textureData);
        }
    }

    //NOTE: resource working directory
    directoryChange(cwd.path);

    TextureCreation textureCreation{};
    uint32_t zeroValue = 0;
    textureCreation.setName("dummyTexture")
        .setSize(1, 1, 1)
        .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setData(&zeroValue);
    TextureHandle dummyTexture = gpu.createTexture(textureCreation);

    SamplerCreation samplerCreation{};
    samplerCreation.minFilter = VK_FILTER_LINEAR;
    samplerCreation.magFilter = VK_FILTER_LINEAR;
    samplerCreation.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreation.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerHandle dummySampler = gpu.createSampler(samplerCreation);

    StringBuffer resourceNameBuffer;
    resourceNameBuffer.init(void_kilo(64), allocator);

    Array<SamplerResource> samplers2;
    samplers2.init(allocator, uint32_t(cgltfData->samplers_count));

    for (uint32_t samplerIndex = 0; samplerIndex < cgltfData->samplers_count; ++samplerIndex)
    {
        cgltf_sampler sampler = cgltfData->samplers[samplerIndex];

        char* samplerName = resourceNameBuffer.appendUseF("Sampler_%u", samplerIndex);

        SamplerCreation creation;
        creation.minFilter = sampler.min_filter == cgltf_filter_type_linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        creation.magFilter = sampler.mag_filter == cgltf_filter_type_linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        creation.name = samplerName;

        SamplerResource* samplerResource = renderer.createSampler(creation);
        VOID_ASSERT(samplerResource != nullptr);

        samplers2.push(*samplerResource);
    }

    //NOTE: resource working directory
    directoryChange(cwd.path);

    Array<MeshDraw> meshDraws;
    meshDraws.init(allocator, uint32_t(cgltfData->meshes_count));

    //We have no idea if it's 
    Array<void*> meshIndices;
    meshIndices.init(allocator, 256);

    cgltf_component_type componentType = cgltf_component_type_max_enum;
    Array<Vertices> vertices;
    vertices.init(allocator, 256);

    BufferHandle currentIndexBuffer = INVALID_BUFFER;
    {
        //Create pipeline state
        PipelineCreation pipelineCreation;

        //Render pass
        pipelineCreation.renderPass = gpu.getSwapchainOutput();
        //Depth
        pipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

        //Shader state
        FileReadResult vertexShaderCode = fileReadBinary("Assets/Shaders/coreShader.vert.spv", &MemoryService::instance()->scratchAllocator);
        FileReadResult fragShaderCode = fileReadBinary("Assets/Shaders/coreShader.frag.spv", &MemoryService::instance()->scratchAllocator);

        pipelineCreation.shaders.setName("Cube")
            .addStage(vertexShaderCode.data, uint32_t(vertexShaderCode.size), VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragShaderCode.data, uint32_t(fragShaderCode.size), VK_SHADER_STAGE_FRAGMENT_BIT)
            .setSPVInput(true);

        //Descriptor set layout.
        DescriptorSetLayoutCreation cubeRLLCreation{};
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, VK_SHADER_STAGE_ALL, "LocalConstants" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 1, VK_SHADER_STAGE_ALL, "MaterialConstant" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "diffuseTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "roughnessMetalnessTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "occlusionTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "emissiveTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "normalTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, 1, VK_SHADER_STAGE_VERTEX_BIT, "Vertices" });
        //Setting it into pipeline.
        cubeDSL = gpu.createDescriptorSetLayout(cubeRLLCreation);
        pipelineCreation.addDescriptorSetLayout(cubeDSL);

        cubePipeline = gpu.createPipeline(pipelineCreation);

        //Constant buffer
        BufferCreation uniformBufferCreation;
        uniformBufferCreation.reset()
            .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT , sizeof(UniformData))
            .setName("cubeCB");
        cubeCB = gpu.createBuffer(uniformBufferCreation);

        //These two are tightly coupled. nodeparent describes the relationship between the children and parents.
        Array<int32_t> nodeParents;
        uint32_t nodeCount = uint32_t(cgltfData->nodes_count);
        nodeParents.init(allocator, nodeCount);
        Array<cgltf_node> nodeStack;
        nodeStack.init(allocator, nodeCount);

        Array<mat4s> nodeMatrix;
        nodeMatrix.init(allocator, nodeCount, nodeCount);

        //Adding all the root nodes to the array.
        for (uint32_t sceneIndex = 0; sceneIndex < uint32_t(cgltfData->scenes_count); ++sceneIndex)
        {
            cgltf_scene cgltfscene = cgltfData->scenes[sceneIndex];
            for (uint32_t parentIndex = 0; parentIndex < cgltfscene.nodes_count; ++parentIndex)
            {
                cgltf_node* parentNode = cgltfscene.nodes[parentIndex];
                nodeParents.push(-1);
                nodeStack.push(*parentNode);
            }
        }

        mat4s finalMatrix = glms_mat4_identity();
        for (uint32_t sceneIndex = 0; sceneIndex < uint32_t(cgltfData->scenes_count); ++sceneIndex)
        {
            for (uint32_t nodeIndex = 0; nodeIndex < nodeStack.size; ++nodeIndex)
            {
                cgltf_node currentNode = nodeStack[nodeIndex];

                mat4s localMatrix = glms_mat4_identity();

                if (currentNode.has_matrix)
                {
                    //CGLM and glTF have the same matrix layout, just memcpy it.
                    memcpy(&localMatrix, currentNode.matrix, sizeof(mat4s));
                }
                else
                {
                    vec3s nodeScale = { 1.f, 1.f, 1.f };
                    if (currentNode.has_scale)
                    {
                        nodeScale = vec3s{ currentNode.scale[0], currentNode.scale[1], currentNode.scale[2] };
                    }

                    vec3s nodeTranslation = { 0.f, 0.f, 0.f };
                    if (currentNode.has_translation)
                    {
                        nodeTranslation = vec3s{ currentNode.translation[0], currentNode.translation[1], currentNode.translation[2] };
                    }

                    //Rotation is written as a plain quaterion.
                    versors nodeRotation = glms_quat_identity();
                    if (currentNode.has_rotation)
                    {
                        nodeRotation = glms_quat_init(currentNode.rotation[0], currentNode.rotation[1], currentNode.rotation[2], currentNode.rotation[3]);
                    }

                    Transform transform;
                    transform.reset();
                    transform.translation = nodeTranslation;
                    transform.scale = nodeScale;
                    transform.rotation = nodeRotation;

                    localMatrix = transform.calculateMatrix();
                }

                nodeMatrix[nodeIndex] = localMatrix;

                for (uint32_t childIndex = 0; childIndex < currentNode.children_count; ++childIndex)
                {
                    cgltf_node childNode = *currentNode.children[nodeIndex];
                    nodeStack.push(childNode);
                    nodeParents.push(nodeIndex);
                }

                finalMatrix = localMatrix;
                int32_t parentNodeIndex = nodeParents[nodeIndex];
                while (parentNodeIndex != -1)
                {
                    finalMatrix = glms_mat4_mul(nodeMatrix[parentNodeIndex], finalMatrix);
                    parentNodeIndex = nodeParents[parentNodeIndex];
                }

                cgltf_mesh* mesh2 = nodeStack[nodeIndex].mesh;

                //Final SRT composition
                for (uint32_t primitiveIndex = 0; primitiveIndex < mesh2->primitives_count; ++primitiveIndex)
                {
                    MeshDraw meshDraw{};

                    meshDraw.materialData.model = finalMatrix;

                    cgltf_primitive meshPrimitive = mesh2->primitives[primitiveIndex];

                    //We are now correctly parsing indices. We always expect with the cgltf_accessor_unpack_indices that the index offset to 0.
                    meshDraw.indexOffset = 0;
                    uint32_t indexCount = uint32_t(meshPrimitive.indices->count);
                    meshDraw.count = indexCount;
                    componentType = meshPrimitive.indices->component_type;

                    size_t stackPrimitveMarker = scratchAllocator.getMarker();

                    uint32_t indexCompenentSize = (uint32_t)cgltf_component_size(meshPrimitive.indices->component_type);
                    Array<uint32_t> indices;
                    indices.init(&scratchAllocator, indexCount, indexCount);
                    cgltf_accessor_unpack_indices(meshPrimitive.indices, indices.data, indexCompenentSize, indices.size);

                    BufferCreation bufferCreation{};
                    bufferCreation.set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, uint32_t(indices.size * meshPrimitive.indices->stride))
                                  .setName("indices")
                                  .setData(indices.data);
                    currentIndexBuffer = gpu.createBuffer(bufferCreation);

                    meshDraw.indexBuffer = currentIndexBuffer;

                    cgltf_material* material = meshPrimitive.material;
                    VOID_ASSERTM(material != nullptr, "The model mesh materials can't be null.\n");

                    //Descriptor set
                    DescriptorSetCreation dsCreation{};
                    dsCreation.buffer(cubeCB, 0);

                    bufferCreation.reset()
                                  .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(MaterialData))
                                  .setName("material");
                    meshDraw.materialBuffer = gpu.createBuffer(bufferCreation);
                    dsCreation.buffer(meshDraw.materialBuffer, 1);

                    if (material->has_pbr_metallic_roughness)
                    {
                        if (material->pbr_metallic_roughness.base_color_factor[0] != 0)
                        {
                            meshDraw.materialData.baseColourFactor.x = material->pbr_metallic_roughness.base_color_factor[0];
                            meshDraw.materialData.baseColourFactor.y = material->pbr_metallic_roughness.base_color_factor[1];
                            meshDraw.materialData.baseColourFactor.z = material->pbr_metallic_roughness.base_color_factor[2];
                            meshDraw.materialData.baseColourFactor.w = material->pbr_metallic_roughness.base_color_factor[3];
                        }
                        else
                        {
                            meshDraw.materialData.baseColourFactor = { 1.f, 1.f, 1.f, 1.f };
                        }

                        if (material->pbr_metallic_roughness.base_color_texture.texture != nullptr)
                        {
                            cgltf_texture* textureInfo = material->pbr_metallic_roughness.base_color_texture.texture;
                            SamplerHandle samplerHandle = dummySampler;

                            uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                            TextureResource& textureGPU = images2[imageIndex];

                            if (textureInfo->sampler)
                            {
                                uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                                SamplerResource& samplerGPU = samplers2[sampleIndex];
                                gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                                samplerHandle = samplerGPU.handle;
                            }

                            dsCreation.textureSampler(textureGPU.handle, samplerHandle, 2);

                            meshDraw.materialData.flags |= MaterialFeatures_ColourTexture;
                        }
                        else
                        {
                            dsCreation.textureSampler(dummyTexture, dummySampler, 2);
                        }

                        if (material->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr)
                        {
                            cgltf_texture* textureInfo = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                            SamplerHandle samplerHandle = dummySampler;

                            uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                            TextureResource& textureGPU = images2[imageIndex];

                            if (textureInfo->sampler)
                            {
                                uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                                SamplerResource& samplerGPU = samplers2[sampleIndex];
                                gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                                samplerHandle = samplerGPU.handle;
                            }

                            dsCreation.textureSampler(textureGPU.handle, samplerHandle, 3);

                            meshDraw.materialData.flags |= MaterialFeatures_RoughnessTexture;
                        }
                        else
                        {
                            dsCreation.textureSampler(dummyTexture, dummySampler, 3);
                        }

                        if (material->has_pbr_metallic_roughness)
                        {
                            meshDraw.materialData.metallicFactor = material->pbr_metallic_roughness.metallic_factor;
                        }
                        else
                        {
                            meshDraw.materialData.metallicFactor = 1.0f;
                        }

                        if (material->has_pbr_metallic_roughness)
                        {
                            meshDraw.materialData.roughnessFactor = material->pbr_metallic_roughness.roughness_factor;
                        }
                        else
                        {
                            meshDraw.materialData.roughnessFactor = 1.0f;
                        }
                    }

                    if (material->occlusion_texture.texture != nullptr)
                    {
                        cgltf_texture* textureInfo = material->occlusion_texture.texture;
                        SamplerHandle samplerHandle = dummySampler;

                        uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                        TextureResource& textureGPU = images2[imageIndex];

                        if (textureInfo->sampler)
                        {
                            uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                            SamplerResource& samplerGPU = samplers2[sampleIndex];
                            gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                            samplerHandle = samplerGPU.handle;
                        }

                        dsCreation.textureSampler(textureGPU.handle, samplerHandle, 4);

                        meshDraw.materialData.occlusionFactor = material->occlusion_texture.scale !=
                            FLT_MAX ?
                            material->occlusion_texture.scale :
                            1.f;

                        meshDraw.materialData.flags |= MaterialFeatures_OcclusionTexture;
                    }
                    else
                    {
                        meshDraw.materialData.occlusionFactor = 1.f;
                        dsCreation.textureSampler(dummyTexture, dummySampler, 4);
                    }

                    if (material->emissive_texture.texture != nullptr)
                    {
                        cgltf_texture* textureInfo = material->emissive_texture.texture;
                        SamplerHandle samplerHandle = dummySampler;

                        uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                        TextureResource& textureGPU = images2[imageIndex];

                        if (textureInfo->sampler)
                        {
                            uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                            SamplerResource& samplerGPU = samplers2[sampleIndex];
                            gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                            samplerHandle = samplerGPU.handle;
                        }

                        dsCreation.textureSampler(textureGPU.handle, samplerHandle, 5);

                        meshDraw.materialData.flags |= MaterialFeatures_EmissiveTexture;

                        //TODO: Is this always tide to the emissive texture?
                        meshDraw.materialData.emissiveFactor = vec3s
                        {
                            material->emissive_factor[0],
                            material->emissive_factor[1],
                            material->emissive_factor[2]
                        };
                    }
                    else
                    {
                        dsCreation.textureSampler(dummyTexture, dummySampler, 5);
                    }

                    if (material->normal_texture.texture != nullptr)
                    {
                        cgltf_texture* textureInfo = material->normal_texture.texture;
                        SamplerHandle samplerHandle = dummySampler;

                        uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                        TextureResource& textureGPU = images2[imageIndex];

                        if (textureInfo->sampler)
                        {
                            uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                            SamplerResource& samplerGPU = samplers2[sampleIndex];
                            gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                            samplerHandle = samplerGPU.handle;
                        }

                        dsCreation.textureSampler(textureGPU.handle, samplerHandle, 6);

                        meshDraw.materialData.flags |= MaterialFeatures_NormalTexture;
                    }
                    else
                    {
                        dsCreation.textureSampler(dummyTexture, dummySampler, 6);
                    }

                    const cgltf_accessor* positionAccessor = cgltf_find_accessor(&meshPrimitive, cgltf_attribute_type_position, 0);
                    const cgltf_accessor* normalAccessor = cgltf_find_accessor(&meshPrimitive, cgltf_attribute_type_normal, 0);
                    const cgltf_accessor* tangentAccessor = cgltf_find_accessor(&meshPrimitive, cgltf_attribute_type_tangent, 0);
                    const cgltf_accessor* textureAccessor = cgltf_find_accessor(&meshPrimitive, cgltf_attribute_type_texcoord, 0);

                    uint32_t vertexCount = uint32_t(positionAccessor->count);
                    Array<Vertices> vertex;
                    vertex.init(&scratchAllocator, vertexCount, vertexCount);
                    if (positionAccessor)
                    {
                        Array<float> scratch;
                        uint32_t accessFloatSize = (uint32_t)cgltf_num_components(normalAccessor->type);
                        scratch.init(&scratchAllocator, vertexCount * accessFloatSize, vertexCount * accessFloatSize);
                        VOID_ASSERT(cgltf_num_components(positionAccessor->type) == 3);
                        cgltf_accessor_unpack_floats(positionAccessor, scratch.data, positionAccessor->count * accessFloatSize);

                        for (uint32_t j = 0; j < vertexCount; ++j)
                        {
                            vertex[j].position[0] = scratch[j * 3 + 0];
                            vertex[j].position[1] = scratch[j * 3 + 1];
                            vertex[j].position[2] = scratch[j * 3 + 2];
                        }
                    }
                    else
                    {
                        VOID_ERROR("No position data found.");
                        continue;
                    }

                    if (normalAccessor)
                    {
                        Array<float> scratch;
                        uint32_t normalCount = (uint32_t)normalAccessor->count;
                        uint32_t accessFloatSize = (uint32_t)cgltf_num_components(normalAccessor->type);
                        scratch.init(&scratchAllocator, normalCount* accessFloatSize, normalCount* accessFloatSize);
                        VOID_ASSERT(cgltf_num_components(normalAccessor->type) == 3);
                        cgltf_accessor_unpack_floats(normalAccessor, scratch.data, normalAccessor->count * accessFloatSize);

                        for (uint32_t j = 0; j < vertexCount; ++j)
                        {
                            vertex[j].normals[0] = scratch[j * 3 + 0];
                            vertex[j].normals[1] = scratch[j * 3 + 1];
                            vertex[j].normals[2] = scratch[j * 3 + 2];
                        }
                    }
                    else
                    {
                        VOID_ERROR("The model needs normals.");
                    }

                    if (tangentAccessor)
                    {
                        Array<float> scratch;
                        uint32_t tangentCount = uint32_t(tangentAccessor->count);
                        uint32_t accessFloatSize = (uint32_t)cgltf_num_components(tangentAccessor->type);
                        scratch.init(&scratchAllocator, tangentCount* accessFloatSize, tangentCount * accessFloatSize);
                        VOID_ASSERT(cgltf_num_components(tangentAccessor->type) == 4);
                        cgltf_accessor_unpack_floats(tangentAccessor, scratch.data, tangentAccessor->count * accessFloatSize);

                        for (uint32_t j = 0; j < vertexCount; ++j)
                        {
                            vertex[j].tangent[0] = scratch[j * 4 + 0];
                            vertex[j].tangent[1] = scratch[j * 4 + 1];
                            vertex[j].tangent[2] = scratch[j * 4 + 2];
                            vertex[j].tangent[3] = scratch[j * 4 + 3];
                        }

                        meshDraw.materialData.flags |= MaterialFeatures_TangentVertexAttribute;
                    }

                    if (textureAccessor)
                    {
                        Array<float> scratch;
                        uint32_t textureCount = (uint32_t)textureAccessor->count;
                        uint32_t accessFloatSize = (uint32_t)cgltf_num_components(textureAccessor->type);
                        scratch.init(&scratchAllocator, textureCount * accessFloatSize, textureCount * accessFloatSize);
                        VOID_ASSERT(cgltf_num_components(textureAccessor->type) == 2);
                        cgltf_accessor_unpack_floats(textureAccessor, scratch.data, textureAccessor->count * accessFloatSize);

                        for (uint32_t j = 0; j < vertexCount; ++j)
                        {
                            vertex[j].texCoord0[0] = scratch[j * 2 + 0];
                            vertex[j].texCoord0[1] = scratch[j * 2 + 1];
                        };

                        meshDraw.materialData.flags |= MaterialFeatures_TexcoordVertexAttribute;
                    }

                    bufferCreation.reset()
                        .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Vertices) * vertex.size)
                        .setName("Vertices")
                        .setData(vertex.data);
                    meshDraw.vertexBuffer = gpu.createBuffer(bufferCreation);

                    dsCreation.buffer(meshDraw.vertexBuffer, 7)
                        .setLayout(cubeDSL);

                    scratchAllocator.freeMarker(stackPrimitveMarker);

                    meshDraw.descriptorSet = gpu.createDescriptorSet(dsCreation);
                    meshDraws.push(meshDraw);
                }
            }
        }

        nodeParents.shutdown();
        nodeStack.shutdown();
        nodeMatrix.shutdown();
    }

    cgltf_free(cgltfData);

    //DescriptorSetCreation dsCreation{};
    //bufferCreation.reset()
    //    .set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ResourceType::Type::IMMUTABLE, sizeof(meshIndices)* meshIndices.size)
    //    //.setData(meshIndices.data)
    //    .setName("Indices");

    //if (componentType == cgltf_component_type_r_16u)
    //{
    //    bufferCreation.setData(reinterpret_cast<uint16_t*>(meshIndices.data));
    //}
    //else
    //{
    //    bufferCreation.setData(reinterpret_cast<uint32_t*>(meshIndices.data));
    //}

    //BufferHandle indexBufferHandle = gpu.createBuffer(bufferCreation);

    int64_t beginFrameTick = timeNow();

    vec3s eye = vec3s{ 0.f, 2.5f, 2.f };
    vec3s look = vec3s{ 0.f, 0.f, -1.f };
    vec3s right = vec3s{ 1.f, 0.f, 0.f };

    GameCamera gameCamera;
    gameCamera.internal3DCamera.initPerspective(0.01f, 1000.f, 60.f, (float)Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(true, 7.f, 3.0f, 0.1f);

    float modelScale = 1.f;
    bool fullscreen = false;
    while (Window::instance()->exitRequested == false)
    {
        //ZoneScoped;
        inputHandler.onEvent(&gpu);
        if (inputHandler.isKeyDown(Keys::KEY_ESCAPE))
        {
            Window::instance()->exitRequested = true;
        }
        else if (inputHandler.isKeyJustReleased(Keys::KEY_F))
        {
            fullscreen = !fullscreen;
            Window::instance()->setFullscreen(fullscreen);
        }
 
        //New Frame
        if (Window::instance()->minimised == false)
        {
            //On Windows with an Nvidia graphics card and using SDL3 the synchronisation drifts when a window events when I'm using synchronisation1. 
            //This seem to be happening because the presentation engine or something internal driver thing is causing the frame to continue to push frames to the screen.
            //Meaning that when we restart the event loop for rendering after an Window event the currentFrame mis-matches with what the actual currentFrame is.
            //This happens when submitting the main queue for some work. The only fix I could find is idling the main queue at the beginning of every frame.
            //For some reason this causes everything to remain in sync when an Window even happens.
            vkQueueWaitIdle(gpu.vulkanQueue);

            //This is only false when we can't recreate the swapchain because of 0 height due to VK_ERROR_OUT_OF_DATE_KHR constantly being hit.
            //We still need to acquire an image to re-check if can now correctly fetch a swapchain image. 
            if (gpu.newFrame() == false) 
            {
                continue;
            }

            if (Window::instance()->resizeRequested)
            {
                Window::instance()->resizeRequested = false;

                gpu.resize(Window::instance()->width, Window::instance()->height);
                gameCamera.internal3DCamera.setAspectRatio(Window::instance()->width * 1.f / Window::instance()->height);
            }
            //NOTE: This must be after the OS messages.
            imgui->newFrame();

            if (ImGui::Begin("Void ImGui"))
            {
                ImGui::InputFloat("Model Scale", &modelScale, 0.001f);
            }
            ImGui::End();

            if (ImGui::Begin("GPU"))
            {
                gpuProfiler.imguiDraw();
            }
            ImGui::End();

            mat4s globalModel{};

            //Moves key pressed events stores then in a key-pressed array. This allows us to know if a key is being held down, rather than just pressed. 
            inputHandler.newFrame();
            //Saves the mouse position in screen coordinates and handles events that are for re-mapped key bindings 
            inputHandler.update();

            //I want the physics delta outside of the loop for now.
            const int64_t currentTick = timeNow();
            float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
            beginFrameTick = currentTick;

            //Update rotating cube data.
            MapBufferParameters cbMap = { cubeCB, 0, 0 };
            void* cbData = gpu.mapBuffer(cbMap);
            if (cbData)
            {
                globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

                inputHandler.newFrame();
                inputHandler.update();
                gameCamera.update(&inputHandler, (float)Window::instance()->width, (float)Window::instance()->height, deltaTime);
                Window::instance()->centerMouse(inputHandler.isMouseDragging(MouseButtons::MOUSE_BUTTON_RIGHT));

                //TODO: Match these name with what's in the shader.
                UniformData uniformData{};
                uniformData.viewPerspective = gameCamera.internal3DCamera.viewProjection;
                uniformData.globalModel = globalModel;
                //eye not used in shader.
                uniformData.eye = vec4s{ eye.x, eye.y, eye.z, 1.f };
                uniformData.light = vec4s{ 2.f, 2.f, 0.f, 1.f };

                memcpy(cbData, &uniformData, sizeof(UniformData));

                gpu.unmapBuffer(cbMap);
            }

            CommandBuffer* gpuCommands = gpu.getCommandBuffer(VK_QUEUE_GRAPHICS_BIT, true);
            gpuCommands->pushMarker("Frame");

            gpuCommands->clear(0.7f, 0.9f, 1.f, 1.f);
            gpuCommands->clearDepthStencil(0.f, 0);
            gpuCommands->bindPass(gpu.getSwapchainPass());
            gpuCommands->bindPipeline(cubePipeline);
            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            for (uint32_t meshIndex = 0; meshIndex < meshDraws.size; ++meshIndex)
            {
                MeshDraw meshDraw = meshDraws[meshIndex];
                meshDraw.materialData.modelInv = glms_mat4_inv(glms_mat4_transpose(glms_mat4_mul(globalModel, meshDraw.materialData.model)));

                MapBufferParameters materialMap = { meshDraw.materialBuffer, 0, 0 };
                MaterialData* materialBufferData = reinterpret_cast<MaterialData*>(gpu.mapBuffer(materialMap));

                memcpy(materialBufferData, &meshDraw.materialData, sizeof(MaterialData));

                gpu.unmapBuffer(materialMap);

                gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, componentType == cgltf_component_type_r_32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
                //gpuCommands->bindIndexBuffer(indexBufferHandle, meshDraw.indexOffset, componentType == cgltf_component_type_r_32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
                gpuCommands->bindDescriptorSet(&meshDraw.descriptorSet, 1, nullptr, 0);

                gpuCommands->drawIndexed(meshDraw.count, 1, 0, 0, 0);
            }

            imgui->render(*gpuCommands);

            gpuCommands->popMarker();

            gpuProfiler.update(gpu);

            gpu.queueCommandBuffer(gpuCommands);
            gpu.present();
        }
        else
        {
            ImGui::Render();
        }

        //FrameMark;
    }

    vkDeviceWaitIdle(gpu.vulkanDevice);

    for (uint32_t meshIndex = 0; meshIndex < meshDraws.size; ++meshIndex)
    {
        MeshDraw& meshDraw = meshDraws[meshIndex];
        gpu.destroyDescriptorSet(meshDraw.descriptorSet);
        gpu.destroyBuffer(meshDraw.materialBuffer);
        gpu.destroyBuffer(meshDraw.vertexBuffer);
        gpu.destroyBuffer(meshDraw.indexBuffer);
    }

    gpu.destroyTexture(dummyTexture);
    gpu.destroySampler(dummySampler);

    meshDraws.shutdown();

    gpu.destroyBuffer(cubeCB);
    gpu.destroyPipeline(cubePipeline);
    gpu.destroyDescriptorSetLayout(cubeDSL);

    imgui->shutdown();

    gpuProfiler.shutdown();
    resourceManager.shutdown();

    //This is here to solve a bug that happens when allocating image from a .glb file. 
    for (uint32_t i = 0; i < images2.size; ++i) 
    {
        renderer.destroyTexture(&images2[i]);
    }

    renderer.shutdown();

    vertices.shutdown();
    meshIndices.shutdown();
    samplers2.shutdown();
    images2.shutdown();

    resourceNameBuffer.shutdown();

    inputHandler.shutdown();
    Window::instance()->shutdown();

    MemoryService::instance()->shutdown();

    return 0;
}