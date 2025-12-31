
//###########-TO BE RESTORED LATER-###########################
//#include "Foundation/Memory.hpp"
//#include "Graphics/VulkanRenderer.hpp"
//
//int main() 
//{
//	MemoryService::instance()->init(void_giga(1ull), void_mega(8));
//
//	VulkanRenderer* renderer = (VulkanRenderer*)void_alloca(sizeof(VulkanRenderer), &MemoryService::instance()->systemAllocator);
//
//	runGame(renderer);
//
//	void_free(renderer, &MemoryService::instance()->systemAllocator);
//
//	MemoryService::instance()->shutdown();
//}

#include "Application/Window.hpp"
#include "Application/Input.hpp"
#include "Application/Keys.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/VoidImgui.hpp"
#include "Graphics/GPUProfiler.hpp"

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"
#include "cglm/struct/cam.h"

#include "vender/imgui/imgui.h"
//#include "vender/tracy/tracy/Tracy.hpp"

#include "Foundation/File.hpp"
#include "Foundation/Gltf.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/ResourceManager.hpp"
#include "Foundation/Time.hpp"

#include <stdlib.h>

namespace
{

    //TODO: Figure out if you need this stuff.
    BufferHandle cubeVB;
    BufferHandle cubeIB;
    PipelineHandle cubePipeline;
    BufferHandle cubeCB;
    DescriptorSetHandle cubeRL;
    DescriptorSetLayoutHandle cubeDSL;

    float rX;
    float rY;

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

    struct MeshDraw
    {
        BufferHandle indexBuffer;
        BufferHandle positionBuffer;
        BufferHandle tangentBuffer;
        BufferHandle normalBuffer;
        BufferHandle texcoordBuffer;

        BufferHandle materialBuffer;
        MaterialData materialData;

        uint32_t indexOffset;
        uint32_t positionOffset;
        uint32_t tangentOffset;
        uint32_t normalOffset;
        uint32_t texcoordOffset;

        uint32_t count;

        VkIndexType indexType;

        DescriptorSetHandle descriptorSet;
    };

    struct UniformData
    {
        alignas(16) mat4s globalModel;
        alignas(16) mat4s viewPerspective;
        alignas(16) vec4s eye;
        alignas(16) vec4s light;
    };

    struct Transform
    {
        vec3s scale;
        versors rotation;
        vec3s translation;

        void reset();
        mat4s calculateMatrix() const
        {
            const mat4s translationMatrix = glms_translate_make(translation);
            const mat4s scaleMatrix = glms_scale_make(scale);
            const mat4s localMatrix = glms_mat4_mul(glms_mat4_mul(translationMatrix, glms_quat_mat4(rotation)), scaleMatrix);

            return localMatrix;
        }
    };

    uint8_t* getBufferData(glTF::BufferView* bufferViews, uint32_t bufferIndex,
                           Array<void*>& buffersData, uint32_t* bufferSize = nullptr,
                           char** bufferName = nullptr)
    {
        glTF::BufferView& buffer = bufferViews[bufferIndex];

        int32_t offset = buffer.byteOffset;
        if (offset == glTF::INVALID_INT_VALUE)
        {
            offset = 0;
        }

        if (bufferName != nullptr)
        {
            *bufferName = buffer.name.data;
        }

        if (bufferSize != nullptr)
        {
            *bufferSize = buffer.byteLength;
        }

        uint8_t* data = reinterpret_cast<uint8_t*>(buffersData[buffer.buffer]) + offset;

        return data;
    }
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

    Allocator* allocator = &MemoryService::instance()->systemAllocator;
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

    glTF::glTF scene = gltfLoadFile(GLTFFile);

    Array<TextureResource> images;
    images.init(allocator, scene.imagesCount);

    for (uint32_t imageIndex = 0; imageIndex < scene.imagesCount; ++imageIndex)
    {
        glTF::Image& image = scene.images[imageIndex];
        TextureResource* textureResources = renderer.createTexture(image.uri.data, image.uri.data);

        VOID_ASSERT(textureResources != nullptr);

        images.push(*textureResources);
    }

    TextureCreation textureCreation{};
    uint32_t zeroValue = 0;
    textureCreation.setName("dummyTexture")
        .setSize(1, 1, 1)
        .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(1, 0)
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

    Array<SamplerResource> samplers;
    samplers.init(allocator, scene.samplersCount);

    for (uint32_t samplerIndex = 0; samplerIndex < scene.samplersCount; ++samplerIndex)
    {
        glTF::Sampler& sampler = scene.samplers[samplerIndex];

        char* samplerName = resourceNameBuffer.appendUseF("Sampler_%u", samplerIndex);

        SamplerCreation creation;
        creation.minFilter = sampler.minFilter == glTF::Sampler::Filter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        creation.magFilter = sampler.magFilter == glTF::Sampler::Filter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        creation.name = samplerName;

        SamplerResource* samplerResource = renderer.createSampler(creation);
        VOID_ASSERT(samplerResource != nullptr);

        samplers.push(*samplerResource);
    }

    Array<void*> buffersData;
    buffersData.init(allocator, scene.buffersCount);

    for (uint32_t bufferIndex = 0; bufferIndex < scene.buffersCount; ++bufferIndex)
    {
        glTF::Buffer& buffer = scene.buffers[bufferIndex];

        FileReadResult bufferData = fileReadBinary(buffer.uri.data, allocator);
        buffersData.push(bufferData.data);
    }

    Array<BufferResource> buffers;
    buffers.init(allocator, scene.bufferViewCount);

    for (uint32_t bufferIndex = 0; bufferIndex < scene.bufferViewCount; ++bufferIndex)
    {
        char* bufferName = nullptr;
        uint32_t bufferSize = 0;
        uint8_t* data = getBufferData(scene.bufferViews, bufferIndex, buffersData, &bufferSize, &bufferName);

        //NOTE: The target attribute of a BufferView is not mandatory, so we prepare for both uses.
        VkBufferUsageFlags flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        if (bufferName == nullptr)
        {
            bufferName = resourceNameBuffer.appendUseF("buffer_%u", bufferIndex);
        }
        else
        {
            //NOTE: Some buffers might have the same name, which causes issues in the renderer cache
            bufferName = resourceNameBuffer.appendUseF("%s_%u", bufferName, bufferIndex);
        }

        BufferResource* bufferResource = renderer.createBuffer(flags, ResourceType::Type::IMMUTABLE, bufferSize, data, bufferName);
        VOID_ASSERT(bufferResource != nullptr);

        buffers.push(*bufferResource);
    }

    //NOTE: resource working directory
    directoryChange(cwd.path);

    Array<MeshDraw> meshDraws;
    meshDraws.init(allocator, scene.meshCount);

    Array<BufferHandle> customMeshBuffers{};
    customMeshBuffers.init(allocator, 8);

    vec4s dummyData[3]{};
    BufferCreation bufferCreation{};
    bufferCreation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ResourceType::Type::IMMUTABLE, sizeof(vec4s) * 3)
        .setData(dummyData)
        .setName("Dummy_attribute_buffer");

    BufferHandle dummyAttributeBuffer = gpu.createBuffer(bufferCreation);

    {
        //Create pipeline state
        PipelineCreation pipelineCreation;

        //Vertex input
        //TODO: Component format should be based on buffer view type.
        //Position
        pipelineCreation.vertexInput.addVertexAttribute({ 0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT });
        pipelineCreation.vertexInput.addVertexStream({ 0, 12, VK_VERTEX_INPUT_RATE_VERTEX });
        //Tangents
        pipelineCreation.vertexInput.addVertexAttribute({ 1, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT });
        pipelineCreation.vertexInput.addVertexStream({ 1, 16, VK_VERTEX_INPUT_RATE_VERTEX });
        //Normal
        pipelineCreation.vertexInput.addVertexAttribute({ 2, 2, 0, VK_FORMAT_R32G32B32_SFLOAT });
        pipelineCreation.vertexInput.addVertexStream({ 2, 12, VK_VERTEX_INPUT_RATE_VERTEX });
        //texCoord
        pipelineCreation.vertexInput.addVertexAttribute({ 3, 3, 0, VK_FORMAT_R32G32_SFLOAT });
        pipelineCreation.vertexInput.addVertexStream({ 3, 8, VK_VERTEX_INPUT_RATE_VERTEX });

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
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 1, "MaterialConstant" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1, "diffuseTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1, "roughnessMetalnessTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, 1, "occlusionTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, 1, "emissiveTexture" });
        cubeRLLCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, 1, "normalTexture" });

        //Setting it into pipeline.
        cubeDSL = gpu.createDescriptorSetLayout(cubeRLLCreation);
        pipelineCreation.addDescriptorSetLayout(cubeDSL);

        //Constant buffer
        BufferCreation uniformBufferCreation;
        uniformBufferCreation.reset()
            .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceType::Type::DYNAMIC, sizeof(UniformData))
            .setName("cubeCB");
        cubeCB = gpu.createBuffer(uniformBufferCreation);

        cubePipeline = gpu.createPipeline(pipelineCreation);
        glTF::Scene& rootGLTFScene = scene.scenes[scene.scene];

        Array<int32_t> nodeParents;
        nodeParents.init(allocator, scene.nodesCount, scene.nodesCount);

        Array<uint32_t> nodeStack;
        nodeStack.init(allocator, 8);

        Array<mat4s> nodeMatrix;
        nodeMatrix.init(allocator, scene.nodesCount, scene.nodesCount);

        for (uint32_t nodeIndex = 0; nodeIndex < rootGLTFScene.nodesCount; ++nodeIndex)
        {
            uint32_t rootNode = rootGLTFScene.nodes[nodeIndex];
            nodeParents[rootNode] = -1;
            nodeStack.push(rootNode);
        }

        while (nodeStack.size)
        {
            uint32_t nodeIndex = nodeStack.back();
            nodeStack.pop();
            glTF::Node& node = scene.nodes[nodeIndex];

            mat4s localMatrix{};

            if (node.matrixCount)
            {
                //CGLM and glTF have the same matrix layout, just memcpy it.
                memcpy(&localMatrix, node.matrix, sizeof(mat4s));
            }
            else
            {
                vec3s nodeScale = { 1.f, 1.f, 1.f };
                if (node.scaleCount != 0)
                {
                    VOID_ASSERT(node.scaleCount == 3);
                    nodeScale = vec3s{ node.scale[0], node.scale[1], node.scale[2] };
                }

                vec3s nodeTranslation = { 0.f, 0.f, 0.f };
                if (node.translationCount)
                {
                    VOID_ASSERT(node.translationCount == 3);
                    nodeTranslation = vec3s{ node.translation[0], node.translation[1], node.translation[2] };
                }

                //Rotation is written as a plain quaterion.
                versors nodeRotation = glms_quat_identity();
                if (node.rotationCount)
                {
                    VOID_ASSERT(node.translationCount == 4);
                    nodeRotation = glms_quat_init(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
                }

                Transform transform;
                transform.translation = nodeTranslation;
                transform.scale = nodeScale;
                transform.rotation = nodeRotation;

                localMatrix = transform.calculateMatrix();
            }

            nodeMatrix[nodeIndex] = localMatrix;

            for (uint32_t childIndex = 0; childIndex < node.childrenCount; ++childIndex)
            {
                uint32_t childNodeIndex = node.children[childIndex];
                nodeParents[childNodeIndex] = nodeIndex;
                nodeStack.push(childNodeIndex);
            }

            if (node.mesh == glTF::INVALID_INT_VALUE)
            {
                continue;
            }

            glTF::Mesh& mesh = scene.meshes[node.mesh];

            mat4s finalMatrix = localMatrix;
            int32_t nodeParent = nodeParents[nodeIndex];
            while (nodeParent != -1)
            {
                finalMatrix = glms_mat4_mul(nodeMatrix[nodeParent], finalMatrix);
                nodeParent = nodeParents[nodeParent];
            }

            //Final SRT composition
            for (uint32_t primitiveIndex = 0; primitiveIndex < mesh.primitiveCount; ++primitiveIndex)
            {
                MeshDraw meshDraw{};

                meshDraw.materialData.model = finalMatrix;

                glTF::MeshPrimitive& meshPrimitive = mesh.primitives[primitiveIndex];

                glTF::Accessor& indicesAccessor = scene.accessors[meshPrimitive.indices];
                VOID_ASSERT(indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ||
                    indicesAccessor.componentType == glTF::Accessor::UNSIGNED_SHORT);
                meshDraw.indexType = indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

                BufferResource& indicesBufferGPU = buffers[indicesAccessor.bufferView];
                meshDraw.indexBuffer = indicesBufferGPU.handle;
                meshDraw.indexOffset = indicesAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : indicesAccessor.byteOffset;
                meshDraw.count = indicesAccessor.count;
                VOID_ASSERT((meshDraw.count % 3) == 0);

                int32_t positionAccessorIndex = gltfGetAttributeAccessorIndex(meshPrimitive.attributes, meshPrimitive.attributeCount, "POSITION");
                int32_t tangentAccessorIndex = gltfGetAttributeAccessorIndex(meshPrimitive.attributes, meshPrimitive.attributeCount, "TANGENT");
                int32_t normalAccessorIndex = gltfGetAttributeAccessorIndex(meshPrimitive.attributes, meshPrimitive.attributeCount, "NORMAL");
                int32_t texcoordAccessorIndex = gltfGetAttributeAccessorIndex(meshPrimitive.attributes, meshPrimitive.attributeCount, "TEXCOORD_0");

                vec3s* positionData = nullptr;
                uint32_t* indexData32 = reinterpret_cast<uint32_t*>(getBufferData(scene.bufferViews, indicesAccessor.bufferView, buffersData));
                uint16_t* indexData16 = reinterpret_cast<uint16_t*>(indexData32);
                uint32_t vertexCount = 0;

                if (positionAccessorIndex != -1)
                {
                    glTF::Accessor& positionAccessor = scene.accessors[positionAccessorIndex];
                    BufferResource& positionBufferGPU = buffers[positionAccessor.bufferView];

                    vertexCount = positionAccessor.count;

                    meshDraw.positionBuffer = positionBufferGPU.handle;
                    meshDraw.positionOffset = positionAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : positionAccessor.byteOffset;

                    positionData = reinterpret_cast<vec3s*>(getBufferData(scene.bufferViews, positionAccessor.bufferView, buffersData));
                }
                else
                {
                    VOID_ERROR("No position data found.");
                    continue;
                }

                if (normalAccessorIndex != -1)
                {
                    glTF::Accessor& normalAccessor = scene.accessors[normalAccessorIndex];
                    BufferResource& normalBufferGPU = buffers[normalAccessor.bufferView];

                    meshDraw.normalBuffer = normalBufferGPU.handle;
                    meshDraw.normalOffset = normalAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : normalAccessor.byteOffset;
                }
                else
                {
                    //NOTE: Should you try and compte this at run time?
                    Array<vec3s> normalsArray{};
                    normalsArray.init(allocator, vertexCount, vertexCount);
                    memset(normalsArray.data, 0, normalsArray.size * sizeof(vec3s));

                    uint32_t indexCount = meshDraw.count;
                    for (uint32_t index = 0; index < indexCount; index += 3)
                    {
                        uint32_t i0 = indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ? indexData32[index] : indexData16[index];
                        uint32_t i1 = indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ? indexData32[index + 1] : indexData16[index + 1];
                        uint32_t i2 = indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ? indexData32[index + 2] : indexData16[index + 2];

                        vec3s p0 = positionData[i0];
                        vec3s p1 = positionData[i1];
                        vec3s p2 = positionData[i2];

                        vec3s a = glms_vec3_sub(p1, p0);
                        vec3s b = glms_vec3_sub(p2, p0);

                        vec3s normal = glms_cross(a, b);

                        normalsArray[i0] = glms_vec3_add(normalsArray[i0], normal);
                        normalsArray[i1] = glms_vec3_add(normalsArray[i1], normal);
                        normalsArray[i2] = glms_vec3_add(normalsArray[i2], normal);
                    }

                    for (uint32_t vertex = 0; vertex < vertexCount; ++vertex)
                    {
                        normalsArray[vertex] = glms_normalize(normalsArray[vertex]);
                    }

                    BufferCreation normalCreation{};
                    normalCreation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ResourceType::Type::IMMUTABLE, normalsArray.size * sizeof(vec3s))
                        .setName("normals")
                        .setData(normalsArray.data);

                    meshDraw.normalBuffer = gpu.createBuffer(normalCreation);
                    meshDraw.normalOffset = 0;

                    customMeshBuffers.push(meshDraw.normalBuffer);

                    normalsArray.shutdown();
                }

                if (tangentAccessorIndex != -1)
                {
                    glTF::Accessor& tangentAccessor = scene.accessors[tangentAccessorIndex];
                    BufferResource& tangentBufferGPU = buffers[tangentAccessor.bufferView];

                    meshDraw.tangentBuffer = tangentBufferGPU.handle;
                    meshDraw.tangentOffset = tangentAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : tangentAccessor.byteOffset;

                    meshDraw.materialData.flags |= MaterialFeatures_TangentVertexAttribute;
                }

                if (texcoordAccessorIndex != -1)
                {
                    glTF::Accessor& texCooordAccessor = scene.accessors[texcoordAccessorIndex];
                    BufferResource& texCoordBufferGPU = buffers[texCooordAccessor.bufferView];

                    meshDraw.texcoordBuffer = texCoordBufferGPU.handle;
                    meshDraw.texcoordOffset = texCooordAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : texCooordAccessor.byteOffset;

                    meshDraw.materialData.flags |= MaterialFeatures_TexcoordVertexAttribute;
                }

                VOID_ASSERTM(meshPrimitive.material != glTF::INVALID_INT_VALUE, "Mesh with no material is not supported.");
                glTF::Material& material = scene.materials[meshPrimitive.material];

                //Descriptor set
                DescriptorSetCreation dsCreation{};
                dsCreation.setLayout(cubeDSL)
                    .buffer(cubeCB, 0);

                bufferCreation.reset()
                    .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceType::Type::DYNAMIC, sizeof(MaterialData))
                    .setName("material");
                meshDraw.materialBuffer = gpu.createBuffer(bufferCreation);
                dsCreation.buffer(meshDraw.materialBuffer, 1);
                if (material.pbrMetallicRoughness != nullptr)
                {
                    if (material.pbrMetallicRoughness->baseColourFactorCount != 0)
                    {
                        VOID_ASSERT(material.pbrMetallicRoughness->baseColourFactorCount == 4);

                        meshDraw.materialData.baseColourFactor =
                        {
                            material.pbrMetallicRoughness->baseColourFactor[0],
                            material.pbrMetallicRoughness->baseColourFactor[1],
                            material.pbrMetallicRoughness->baseColourFactor[2],
                            material.pbrMetallicRoughness->baseColourFactor[3]
                        };
                    }
                    else
                    {
                        meshDraw.materialData.baseColourFactor = { 1.f, 1.f, 1.f, 1.f };
                    }

                    if (material.pbrMetallicRoughness->baseColourTexture != nullptr)
                    {
                        glTF::Texture& diffuseTexture = scene.textures[material.pbrMetallicRoughness->baseColourTexture->index];
                        TextureResource& diffuseTextureGPU = images[diffuseTexture.source];

                        SamplerHandle samplerHandle = dummySampler;
                        if (diffuseTexture.sampler != glTF::INVALID_INT_VALUE)
                        {
                            samplerHandle = samplers[diffuseTexture.sampler].handle;
                        }

                        dsCreation.textureSampler(diffuseTextureGPU.handle, samplerHandle, 2);

                        meshDraw.materialData.flags |= MaterialFeatures_ColourTexture;
                    }
                    else
                    {
                        dsCreation.textureSampler(dummyTexture, dummySampler, 2);
                    }

                    if (material.pbrMetallicRoughness->metallicRoughnessTexture != nullptr)
                    {
                        glTF::Texture& roughnessTexture = scene.textures[material.pbrMetallicRoughness->metallicRoughnessTexture->index];
                        TextureResource& roughnessTextureGPU = images[roughnessTexture.source];

                        SamplerHandle samplerHandle = dummySampler;
                        if (roughnessTexture.sampler != glTF::INVALID_INT_VALUE)
                        {
                            samplerHandle = samplers[roughnessTexture.sampler].handle;
                        }

                        dsCreation.textureSampler(roughnessTextureGPU.handle, samplerHandle, 3);

                        meshDraw.materialData.flags |= MaterialFeatures_RoughnessTexture;
                    }
                    else
                    {
                        dsCreation.textureSampler(dummyTexture, dummySampler, 3);
                    }

                    if (material.pbrMetallicRoughness->metallicFactor != glTF::INVALID_FLOAT_VALUE)
                    {
                        meshDraw.materialData.metallicFactor = material.pbrMetallicRoughness->metallicFactor;
                    }
                    else
                    {
                        meshDraw.materialData.metallicFactor = 1.0f;
                    }

                    if (material.pbrMetallicRoughness->roughnessFactor != glTF::INVALID_FLOAT_VALUE)
                    {
                        meshDraw.materialData.roughnessFactor = material.pbrMetallicRoughness->roughnessFactor;
                    }
                    else
                    {
                        meshDraw.materialData.roughnessFactor = 1.0f;
                    }
                }

                if (material.occlusionTexture != nullptr)
                {
                    glTF::Texture& occlusionTexture = scene.textures[material.occlusionTexture->index];

                    //NOTE: This could be the same as the roughness texture but for now we treat it as a seperate texture.
                    TextureResource& occlusionTextureGPU = images[occlusionTexture.source];

                    SamplerHandle samplerHandle = dummySampler;
                    if (occlusionTexture.sampler != glTF::INVALID_INT_VALUE)
                    {
                        samplerHandle = samplers[occlusionTexture.sampler].handle;
                    }

                    dsCreation.textureSampler(occlusionTextureGPU.handle, samplerHandle, 4);

                    meshDraw.materialData.occlusionFactor = material.occlusionTexture->strength !=
                        glTF::INVALID_FLOAT_VALUE ?
                        material.occlusionTexture->strength :
                        1.f;
                    meshDraw.materialData.flags |= MaterialFeatures_OcclusionTexture;
                }
                else
                {
                    meshDraw.materialData.occlusionFactor = 1.f;
                    dsCreation.textureSampler(dummyTexture, dummySampler, 4);
                }

                if (material.emissiveFactorCount != 0)
                {
                    meshDraw.materialData.emissiveFactor = vec3s
                    {
                        material.emissiveFactor[0],
                        material.emissiveFactor[1],
                        material.emissiveFactor[2]
                    };
                }

                if (material.emissiveTexture != nullptr)
                {
                    glTF::Texture& emissiveTexture = scene.textures[material.emissiveTexture->index];
                    //NOTE: This could be the same as the roughness texture but for now we treat it as a seperate texture.
                    TextureResource& emissiveTextureGPU = images[emissiveTexture.source];

                    SamplerHandle samplerHandle = dummySampler;
                    if (emissiveTexture.sampler != glTF::INVALID_INT_VALUE)
                    {
                        samplerHandle = samplers[emissiveTexture.sampler].handle;
                    }

                    dsCreation.textureSampler(emissiveTextureGPU.handle, samplerHandle, 5);

                    meshDraw.materialData.flags |= MaterialFeatures_EmissiveTexture;
                }
                else
                {
                    dsCreation.textureSampler(dummyTexture, dummySampler, 5);
                }

                if (material.normalTexture != nullptr)
                {
                    glTF::Texture& normalTexture = scene.textures[material.normalTexture->index];
                    TextureResource& normalTextureGPU = images[normalTexture.source];

                    SamplerHandle samplerHandle = dummySampler;
                    if (normalTexture.sampler != glTF::INVALID_INT_VALUE)
                    {
                        samplerHandle = samplers[normalTexture.sampler].handle;
                    }

                    dsCreation.textureSampler(normalTextureGPU.handle, samplerHandle, 6);

                    meshDraw.materialData.flags |= MaterialFeatures_NormalTexture;
                }
                else
                {
                    dsCreation.textureSampler(dummyTexture, dummySampler, 6);
                }

                meshDraw.descriptorSet = gpu.createDescriptorSet(dsCreation);
                meshDraws.push(meshDraw);
            }
        }

        nodeParents.shutdown();
        nodeStack.shutdown();
        nodeMatrix.shutdown();

        rX = 0.f;
        rY = 0.f;
    }

    for (uint32_t bufferIndex = 0; bufferIndex < scene.buffersCount; ++bufferIndex)
    {
        void* buffer = buffersData[bufferIndex];
        allocator->deallocate(buffer);
    }
    buffersData.shutdown();

    int64_t beginFrameTick = timeNow();

    vec3s eye = vec3s{ 0.f, 2.5f, 2.f };
    vec3s look = vec3s{ 0.f, 0.f, -1.f };
    vec3s right = vec3s{ 1.f, 0.f, 0.f };

    float yaw = 0.f;
    float pitch = 0.f;

    GameCamera gameCamera;
    gameCamera.internal3DCamera.initPerspective(0.01f, 1000.f, 60.f, (float)Window::instance()->width / (float)Window::instance()->height);
    gameCamera.init(true, 10.f, 6.0f, 0.1f);

    float modelScale = 1.f;
    while (Window::instance()->exitRequested == false)
    {
        //ZoneScoped;

        inputHandler.onEvent();

        //New Frame
        if (Window::instance()->minimised == false)
        {
            gpu.newFrame();

            if (Window::instance()->resizeRequested)
            {
                gpu.resize(Window::instance()->width, Window::instance()->height);
                gameCamera.internal3DCamera.setAspectRatio(Window::instance()->width * 1.f / Window::instance()->height);
                Window::instance()->resizeRequested = false;
                continue;
            }
            //NOTE: This mused be after the OS messages.
            imgui->newFrame();

            const int64_t currentTick = timeNow();
            float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
            beginFrameTick = currentTick;

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

            //Update rotating cube data.
            MapBufferParameters cbMap = { cubeCB, 0, 0 };
            void* cbData = gpu.mapBuffer(cbMap);
            if (cbData)
            {
                //if (inputHandler.isMouseDown(MouseButtons::MOUSE_BUTTON_RIGHT) && ImGui::GetIO().WantCaptureMouse == false)
                //{
                //    pitch += (inputHandler.mousePosition.y - (Window::instance()->width / 2.f)) * deltaTime;
                //    yaw += (inputHandler.mousePosition.x - (Window::instance()->height / 2.f)) * deltaTime;

                //    pitch = clamp(pitch, -60.f, 60.f);

                //    if (yaw > 360.f)
                //    {
                //        yaw -= 360.f;
                //    }

                //    mat3s rxm = glms_mat4_pick3(glms_rotate_make(glm_rad(-pitch), vec3s{ 1.f, 0.f, 0.f }));
                //    mat3s rym = glms_mat4_pick3(glms_rotate_make(glm_rad(-yaw), vec3s{ 0.f, 1.f, 0.f }));

                //    look = glms_mat3_mulv(rxm, vec3s{ 0.f, 0.f, -1.f });
                //    look = glms_mat3_mulv(rym, look);

                //    right = glms_cross(look, vec3s{ 0.f, 1.f, 0.f });
                //}

                //if (inputHandler.isKeyDown(KEY_W))
                //{
                //    eye = glms_vec3_add(eye, glms_vec3_scale(look, 5.f * deltaTime));
                //}
                //else if (inputHandler.isKeyDown(KEY_S))
                //{
                //    eye = glms_vec3_sub(eye, glms_vec3_scale(look, 5.f * deltaTime));
                //}

                //if (inputHandler.isKeyDown(KEY_D))
                //{
                //    eye = glms_vec3_add(eye, glms_vec3_scale(right, 5.f * deltaTime));
                //}
                //else if (inputHandler.isKeyDown(KEY_A))
                //{
                //    eye = glms_vec3_sub(eye, glms_vec3_scale(right, 5.f * deltaTime));
                //}

                //mat4s view = glms_lookat(eye, glms_vec3_add(eye, look), vec3s{ 0.f, 1.f, 0.f });
                //mat4s projection = glms_perspective(glm_rad(60.f), gpu.swapchainWidth * 1.f / gpu.swapchainHeight, 1000.f, 0.01f);

                ////Calculate view projection matrix
                //mat4s viewPerspective = glms_mat4_mul(projection, view);

                //globalModel = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });

                ////TODO: Match these name with what's in the shader.
                //UniformData uniformData{};
                //uniformData.viewPerspective = viewPerspective;
                //uniformData.globalModel = globalModel;
                //uniformData.eye = vec4s{ eye.x, eye.y, eye.z, 1.f };
                //uniformData.light = vec4s{ 2.f, 2.f, 0.f, 1.f };

                //memcpy(cbData, &uniformData, sizeof(UniformData));

                //gpu.unmapBuffer(cbMap);
                
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

                //uniformData.viewPerspective.m11 *= -1;

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

                gpuCommands->bindVertexBuffer(meshDraw.positionBuffer, 0, meshDraw.positionOffset);
                gpuCommands->bindVertexBuffer(meshDraw.normalBuffer, 2, meshDraw.normalOffset);

                if (meshDraw.materialData.flags & MaterialFeatures_TangentVertexAttribute)
                {
                    gpuCommands->bindVertexBuffer(meshDraw.tangentBuffer, 1, meshDraw.tangentOffset);
                }
                else
                {
                    gpuCommands->bindVertexBuffer(dummyAttributeBuffer, 1, 0);
                }

                if (meshDraw.materialData.flags & MaterialFeatures_TexcoordVertexAttribute)
                {
                    gpuCommands->bindVertexBuffer(meshDraw.texcoordBuffer, 3, meshDraw.texcoordOffset);
                }
                else
                {
                    gpuCommands->bindVertexBuffer(dummyAttributeBuffer, 3, 0);
                }

                gpuCommands->bindIndexBuffer(meshDraw.indexBuffer, meshDraw.indexOffset, meshDraw.indexType);
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

    for (uint32_t meshIndex = 0; meshIndex < meshDraws.size; ++meshIndex)
    {
        MeshDraw& meshDraw = meshDraws[meshIndex];
        gpu.destroyDescriptorSet(meshDraw.descriptorSet);
        gpu.destroyBuffer(meshDraw.materialBuffer);
    }

    for (uint32_t meshIndex = 0; customMeshBuffers.size; ++meshIndex)
    {
        gpu.destroyBuffer(customMeshBuffers[meshIndex]);
    }
    customMeshBuffers.shutdown();

    gpu.destroyBuffer(dummyAttributeBuffer);

    gpu.destroyTexture(dummyTexture);
    gpu.destroySampler(dummySampler);

    meshDraws.shutdown();

    gpu.destroyBuffer(cubeCB);
    gpu.destroyPipeline(cubePipeline);
    gpu.destroyDescriptorSetLayout(cubeDSL);

    imgui->shutdown();

    gpuProfiler.shutdown();
    resourceManager.shutdown();

    renderer.shutdown();

    samplers.shutdown();
    images.shutdown();
    buffers.shutdown();

    resourceNameBuffer.shutdown();

    //NOTE: we can't destroy this sooner as textures and buffer hold a pointers to the names stored here.
    gltfFree(scene);

    inputHandler.shutdown();
    Window::instance()->shutdown();

    MemoryService::instance()->shutdown();

    return 0;
}