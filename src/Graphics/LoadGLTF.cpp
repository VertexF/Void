#include "LoadGLTF.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stb_image.h>
#include <tlsf.h>
#include <meshoptimizer.h>

#include "Foundation/Memory.hpp"
#include "Foundation/File.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/ResourceManager.hpp"

#include "GPUResources.hpp"
#include "Renderer.hpp"

Array<Vertices> vertices;
Array<void*> meshIndices;
Array<SamplerResource> samplers;
Array<TextureResource> images;
StringBuffer resourceNameBuffer;

SamplerHandle dummySampler;

struct Transform
{
    vec3s scale;
    vec3s translation;
    versors rotation;

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

void Model::loadModel(const char* modelPath, GPUDevice& gpu, Renderer& renderer, BufferHandle sceneBuffer, DescriptorSetLayoutHandle descriptorSetLayout)
{
    HeapAllocator* allocator = &MemoryService::instance()->systemAllocator;
    StackAllocator scratchAllocator = MemoryService::instance()->scratchAllocator;

    Directory cwd{};
    directoryCurrent(&cwd);

    char GLTFBasePath[512]{};
    memcpy(GLTFBasePath, modelPath, strlen(modelPath));
    fileDirectoryFromPath(GLTFBasePath);

    directoryChange(GLTFBasePath);

    char GLTFFile[512]{};
    memcpy(GLTFFile, modelPath, strlen(modelPath));
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

    images.init(allocator, uint32_t(cgltfData->images_count));

    //GLB version.
    for (uint32_t imageIndex = 0; imageIndex < cgltfData->images_count; ++imageIndex)
    {
        cgltf_image image = cgltfData->images[imageIndex];

        if (image.uri != nullptr)
        {
            TextureResource* textureResource = renderer.createTexture(image.uri, image.uri);

            VOID_ASSERT(textureResource != nullptr);

            images.push(*textureResource);
        }
        else
        {
            int comp = 0;
            int width = 0;
            int height = 0;
            uint8_t mipLevels = 1;

            uint8_t* rawBufferData = reinterpret_cast<uint8_t*>(image.buffer_view->buffer->data) + image.buffer_view->offset;
            stbi_info_from_memory(rawBufferData, int(image.buffer_view->size), &width, &height, &comp);

            //TODO: Add mipmap support later.
            uint32_t w = width;
            uint32_t h = height;

            while (w > 1 && h > 1)
            {
                w /= 2;
                h /= 2;

                ++mipLevels;
            }

            int x;
            int y;
            uint8_t* textureData = stbi_load_from_memory(rawBufferData, int(image.buffer_view->size), &x, &y, &comp, 4);

            TextureCreation textureCreation{};
            textureCreation.setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
                .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1)
                .setData(textureData)
                .setFlags(mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .setName(nullptr);

            TextureResource* textureResource = renderer.createTexture(textureCreation);
            VOID_ASSERT(textureResource != nullptr);

            images.push(*textureResource);

            stbi_image_free(textureData);
        }
    }
    //NOTE: resource working directory
    directoryChange(cwd.path);

    SamplerCreation samplerCreation{};
    samplerCreation.minFilter = VK_FILTER_LINEAR;
    samplerCreation.magFilter = VK_FILTER_LINEAR;
    samplerCreation.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreation.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    dummySampler = gpu.createSampler(samplerCreation);

    resourceNameBuffer.init(void_kilo(64), allocator);

    samplers.init(allocator, uint32_t(cgltfData->samplers_count));

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

        samplers.push(*samplerResource);
    }

    //NOTE: resource working directory
    directoryChange(cwd.path);

    BufferHandle currentIndexBuffer = INVALID_BUFFER;

    meshDraws.init(allocator, uint32_t(cgltfData->meshes_count));

    //We have no idea if it's 
    meshIndices.init(allocator, 256);

    vertices.init(allocator, 256);

    //These two are tightly coupled. nodeparent describes the relationship between the children and parents.
    Array<int32_t> nodeParents;
    nodeParents.init(allocator, cgltfData->nodes_count);
    Array<cgltf_node> nodeStack;
    nodeStack.init(allocator, cgltfData->nodes_count);

    Array<mat4s> nodeMatrix;
    nodeMatrix.init(allocator, cgltfData->nodes_count);

    //Adding all the root nodes to the array.
    for (uint32_t sceneIndex = 0; sceneIndex < (uint32_t)cgltfData->scenes_count; ++sceneIndex)
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
    for (uint32_t sceneIndex = 0; sceneIndex < (uint32_t)cgltfData->scenes_count; ++sceneIndex)
    {
        for (uint32_t nodeIndex = 0; nodeIndex < cgltfData->nodes_count; ++nodeIndex)
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

            nodeMatrix.push(localMatrix);

            if (currentNode.children != nullptr && currentNode.children[0] != nullptr)
            {
                for (uint32_t childIndex = 0; childIndex < currentNode.children_count; ++childIndex)
                {
                    if (currentNode.children[childIndex] != nullptr)
                    {
                        cgltf_node childNode = *currentNode.children[childIndex];
                        nodeStack.push(childNode);
                    }
                    nodeParents.push(nodeIndex);
                }
            }

            finalMatrix = localMatrix;
            int32_t parentNodeIndex = nodeParents[nodeIndex];
            while (parentNodeIndex != -1)
            {
                finalMatrix = glms_mat4_mul(nodeMatrix[parentNodeIndex], finalMatrix);
                parentNodeIndex = nodeParents[parentNodeIndex];
            }

            cgltf_mesh* mesh = nodeStack[nodeIndex].mesh;
            if (mesh == nullptr)
            {
                continue;
            }

            //Final SRT composition
            for (uint32_t primitiveIndex = 0; primitiveIndex < (uint32_t)mesh->primitives_count; ++primitiveIndex)
            {
                MeshDraw meshDraw{};

                meshDraw.model = finalMatrix;

                cgltf_primitive meshPrimitive = mesh->primitives[primitiveIndex];

                //We are now correctly parsing indices. We always expect with the cgltf_accessor_unpack_indices that the index offset to 0.
                meshDraw.indexOffset = 0;
                uint32_t indexCount = uint32_t(meshPrimitive.indices->count);
                meshDraw.count = indexCount;
                meshDraw.componentType = meshPrimitive.indices->component_type == cgltf_component_type_r_32u ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

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

                //Here we are adding the scene buffer (that effect every model) into the descriptor set layout.
                DescriptorSetCreation dsCreation{};
                dsCreation.buffer(sceneBuffer, 0);

                bufferCreation.reset()
                    .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(MaterialData))
                    .setName("material");
                meshDraw.materialBuffer = gpu.createBuffer(bufferCreation);
                dsCreation.buffer(meshDraw.materialBuffer, 1)
                    .setLayout(descriptorSetLayout);

                meshDraw.alphaCutoff = material->alpha_cutoff != FLT_MAX ? material->alpha_cutoff : 1.f;

                if (material->has_pbr_metallic_roughness)
                {
                    meshDraw.baseColourFactor.x = material->pbr_metallic_roughness.base_color_factor[0];
                    meshDraw.baseColourFactor.y = material->pbr_metallic_roughness.base_color_factor[1];
                    meshDraw.baseColourFactor.z = material->pbr_metallic_roughness.base_color_factor[2];
                    meshDraw.baseColourFactor.w = material->pbr_metallic_roughness.base_color_factor[3];

                    meshDraw.metallicRoughnessOcclusionFactor.x = material->pbr_metallic_roughness.metallic_factor != FLT_MAX ? material->pbr_metallic_roughness.metallic_factor : 1.f;
                    meshDraw.metallicRoughnessOcclusionFactor.y = material->pbr_metallic_roughness.roughness_factor != FLT_MAX ? material->pbr_metallic_roughness.roughness_factor : 1.f;

                    if (material->pbr_metallic_roughness.base_color_texture.texture != nullptr)
                    {
                        cgltf_texture* textureInfo = material->pbr_metallic_roughness.base_color_texture.texture;
                        SamplerHandle samplerHandle = dummySampler;

                        uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                        TextureResource& textureGPU = images[imageIndex];

                        if (textureInfo->sampler)
                        {
                            uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                            SamplerResource& samplerGPU = samplers[sampleIndex];
                            gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                            samplerHandle = samplerGPU.handle;
                        }

                        meshDraw.diffuseTextureIndex = (uint16_t)textureGPU.handle.index;
                    }
                    else
                    {
                        meshDraw.diffuseTextureIndex = UINT16_MAX;
                    }

                    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr)
                    {
                        cgltf_texture* textureInfo = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                        SamplerHandle samplerHandle = dummySampler;

                        uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                        TextureResource& textureGPU = images[imageIndex];

                        if (textureInfo->sampler)
                        {
                            uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                            SamplerResource& samplerGPU = samplers[sampleIndex];
                            gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                            samplerHandle = samplerGPU.handle;
                        }

                        meshDraw.roughnessTextureIndex = (uint16_t)textureGPU.handle.index;
                    }
                    else
                    {
                        meshDraw.roughnessTextureIndex = UINT16_MAX;
                    }
                }

                if (material->occlusion_texture.texture != nullptr)
                {
                    cgltf_texture* textureInfo = material->occlusion_texture.texture;
                    SamplerHandle samplerHandle = dummySampler;

                    uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                    TextureResource& textureGPU = images[imageIndex];

                    if (textureInfo->sampler)
                    {
                        uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                        SamplerResource& samplerGPU = samplers[sampleIndex];
                        gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                        samplerHandle = samplerGPU.handle;
                    }

                    meshDraw.metallicRoughnessOcclusionFactor.z = material->occlusion_texture.scale !=
                        FLT_MAX ?
                        material->occlusion_texture.scale :
                        1.f;

                    meshDraw.occlusionTextureIndex = (uint16_t)textureGPU.handle.index;
                }
                else
                {
                    meshDraw.metallicRoughnessOcclusionFactor.z = 1.f;
                    meshDraw.occlusionTextureIndex = UINT16_MAX;
                }

                if (material->emissive_texture.texture != nullptr)
                {
                    cgltf_texture* textureInfo = material->emissive_texture.texture;
                    SamplerHandle samplerHandle = dummySampler;

                    uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                    TextureResource& textureGPU = images[imageIndex];

                    if (textureInfo->sampler)
                    {
                        uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                        SamplerResource& samplerGPU = samplers[sampleIndex];
                        gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                        samplerHandle = samplerGPU.handle;
                    }

                    meshDraw.emisiveTextureIndex = (uint16_t)textureGPU.handle.index;

                    //TODO: Is this always tide to the emissive texture?
                    meshDraw.emissiveFactor = vec3s
                    {
                        material->emissive_factor[0],
                        material->emissive_factor[1],
                        material->emissive_factor[2]
                    };
                }
                else
                {
                    meshDraw.emisiveTextureIndex = UINT16_MAX;
                }

                if (material->normal_texture.texture != nullptr)
                {
                    cgltf_texture* textureInfo = material->normal_texture.texture;
                    SamplerHandle samplerHandle = dummySampler;

                    uint32_t imageIndex = uint32_t(cgltf_image_index(cgltfData, textureInfo->image));
                    TextureResource& textureGPU = images[imageIndex];

                    if (textureInfo->sampler)
                    {
                        uint32_t sampleIndex = uint32_t(cgltf_sampler_index(cgltfData, textureInfo->sampler));
                        SamplerResource& samplerGPU = samplers[sampleIndex];
                        gpu.linkTextureSampler(textureGPU.handle, samplerGPU.handle);
                        samplerHandle = samplerGPU.handle;
                    }

                    meshDraw.normalTextureIndex = (uint16_t)textureGPU.handle.index;
                }
                else
                {
                    meshDraw.normalTextureIndex = UINT16_MAX;
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
                    uint32_t accessFloatSize = (uint32_t)cgltf_num_components(positionAccessor->type);
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
                    scratch.init(&scratchAllocator, normalCount * accessFloatSize, normalCount * accessFloatSize);
                    VOID_ASSERT(cgltf_num_components(normalAccessor->type) == 3);
                    cgltf_accessor_unpack_floats(normalAccessor, scratch.data, normalAccessor->count * accessFloatSize);

                    for (uint32_t j = 0; j < vertexCount; ++j)
                    {
                        vertex[j].normals[0] = uint8_t(scratch[j * 3 + 0] * 127.f + 127.5f);
                        vertex[j].normals[1] = uint8_t(scratch[j * 3 + 1] * 127.f + 127.5f);
                        vertex[j].normals[2] = uint8_t(scratch[j * 3 + 2] * 127.f + 127.5f);
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
                    scratch.init(&scratchAllocator, tangentCount * accessFloatSize, tangentCount * accessFloatSize);
                    VOID_ASSERT(cgltf_num_components(tangentAccessor->type) == 4);
                    cgltf_accessor_unpack_floats(tangentAccessor, scratch.data, tangentAccessor->count * accessFloatSize);

                    for (uint32_t j = 0; j < vertexCount; ++j)
                    {
                        vertex[j].tangent[0] = uint8_t(scratch[j * 4 + 0] * 127.f + 127.5f);
                        vertex[j].tangent[1] = uint8_t(scratch[j * 4 + 1] * 127.f + 127.5f);
                        vertex[j].tangent[2] = uint8_t(scratch[j * 4 + 2] * 127.f + 127.5f);
                        vertex[j].tangent[3] = uint8_t(scratch[j * 4 + 3] * 127.f + 127.5f);
                    }
                }
                else
                {
                    VOID_ERROR("The model needs tangent.");
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
                        vertex[j].texCoord0[0] = meshopt_quantizeHalf(scratch[j * 2 + 0]);
                        vertex[j].texCoord0[1] = meshopt_quantizeHalf(scratch[j * 2 + 1]);
                    };
                }

                bufferCreation.reset()
                    .set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Vertices) * vertex.size)
                    .setName("Vertices")
                    .setData(vertex.data);
                meshDraw.vertexBuffer = gpu.createBindlessBuffer(bufferCreation);

                scratchAllocator.freeMarker(stackPrimitveMarker);

                meshDraw.descriptorSet = gpu.createDescriptorSet(dsCreation);
                meshDraws.push(meshDraw);
            }
        }
    }

    nodeParents.shutdown();
    nodeStack.shutdown();
    nodeMatrix.shutdown();


    cgltf_free(cgltfData);
}

void Model::shutdownModel(GPUDevice& gpu, Renderer& renderer)
{
    for (uint32_t meshIndex = 0; meshIndex < meshDraws.size; ++meshIndex)
    {
        MeshDraw& meshDraw = meshDraws[meshIndex];
        gpu.destroyDescriptorSet(meshDraw.descriptorSet);
        gpu.destroyBuffer(meshDraw.materialBuffer);
        gpu.destroyBuffer(meshDraw.vertexBuffer);
        gpu.destroyBuffer(meshDraw.indexBuffer);
    }

    gpu.destroySampler(dummySampler);
    meshDraws.shutdown();

    //This is here to solve a bug that happens when allocating image from a .glb file. 
    for (uint32_t i = 0; i < images.size; ++i)
    {
        renderer.destroyTexture(&images[i]);
    }

    vertices.shutdown();
    meshIndices.shutdown();
    samplers.shutdown();
    images.shutdown();

    resourceNameBuffer.shutdown();
}