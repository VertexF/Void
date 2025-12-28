
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
        mat4s cameraModel;
        mat4s viewPerspective;
        vec4s eye;
        vec4s light;
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

    InputHandler inputHandler = {};
    inputHandler.init(allocator);

    DeviceCreation deviceCreation;
    deviceCreation.setWindow(Window::instance()->width, Window::instance()->height, Window::instance()->platformHandle)
        .setAllocator(allocator)
        .setLinearAllocator(&scratchAllocator);

    GPUDevice gpu;
    gpu.init(deviceCreation);

    ResourceManager resourceManager;
    resourceManager.init(allocator, nullptr);

    GPUProfiler gpuProfiler;
    gpuProfiler.init(allocator, 100);

    Renderer renderer;
    renderer.init({ &gpu, allocator });
    renderer.setLoaders(&resourceManager);

    ImguiService* imgui = ImguiService::instance();
    ImguiServiceConfiguration imguiConfig = { &gpu, Window::instance()->platformHandle };
    imgui->init(&imguiConfig);

    //Window::instance()->setFullscreen(true);

    Directory cwd = {};
    directoryCurrent(&cwd);

    char GLTFBasePath[512] = {};
    memcpy(GLTFBasePath, argv[1], strlen(argv[1]));
    fileDirectoryFromPath(GLTFBasePath);

    directoryChange(GLTFBasePath);

    char GLTFFile[512] = {};
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

    TextureCreation textureCreation = {};
    uint32_t zeroValue = 0;
    textureCreation.setName("dummyTexture")
        .setSize(1, 1, 1)
        .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, TextureType::TEXTURE_2D)
        .setFlags(1, 0)
        .setData(&zeroValue);
    TextureHandle dummyTexture = gpu.createTexture(textureCreation);

    SamplerCreation samplerCreation = {};
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

    Array<BufferHandle> customMeshBuffers = {};
    customMeshBuffers.init(allocator, 8);

    vec4s dummyData[3] = {};
    BufferCreation bufferCreation = {};
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
        pipelineCreation.vertexInput.addVertexAttribute({ 0, 0, 0, VertexFormat::VertexComponentFormatType::FLOAT3 });
        pipelineCreation.vertexInput.addVertexStream({ 0, 12, VertexInput::VertexInputRateType::PER_VERTEX });
        //Tangents
        pipelineCreation.vertexInput.addVertexAttribute({ 1, 1, 0, VertexFormat::VertexComponentFormatType::FLOAT4 });
        pipelineCreation.vertexInput.addVertexStream({ 1, 16, VertexInput::VertexInputRateType::PER_VERTEX });
        //Normal
        pipelineCreation.vertexInput.addVertexAttribute({ 2, 2, 0, VertexFormat::VertexComponentFormatType::FLOAT3 });
        pipelineCreation.vertexInput.addVertexStream({ 2, 12, VertexInput::VertexInputRateType::PER_VERTEX });
        //texCoord
        pipelineCreation.vertexInput.addVertexAttribute({ 3, 3, 0, VertexFormat::VertexComponentFormatType::FLOAT2 });
        pipelineCreation.vertexInput.addVertexStream({ 3, 8, VertexInput::VertexInputRateType::PER_VERTEX });

        //Render pass
        pipelineCreation.renderPass = gpu.getSwapchainOutput();
        //Depth
        pipelineCreation.depthStencil.setDepth(true, VK_COMPARE_OP_LESS_OR_EQUAL);

        //Shader state

        const char* vsCode =
            R"FOO(#version 450
uint MaterialFeatures_ColourTexture           = 1 << 0;
uint MaterialFeatures_NormalTexutre           = 1 << 1;
uint MaterialFeatures_RoughnessTexture        = 1 << 2;
uint MaterialFeatures_OcclusionTexture        = 1 << 3;
uint MaterialFeatures_EmissiveTexture         = 1 << 4;
uint MaterialFeatures_TangentVertexAttribute  = 1 << 5;
uint MaterialFeatures_TexcoordVertexAttribute = 1 << 6; 

layout(std140, binding = 0) uniform LocalConstants
{
    mat4 cameraModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(std140, binding = 1) uniform MaterialConstant
{
    vec4 baseColourFactor;
    mat4 model;
    mat4 modelInv;

    vec3 emissiveFactor;
    float matallicFactor;

    float roughnessFactor;
    float occlusionsFactor;
    uint flags;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 tangent;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord0;

layout(location = 0) out vec2 vTexcoord0;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vTangent;
layout(location = 3) out vec4 vPosition;

void main()
{
    gl_Position = viewPerspective * cameraModel * model * vec4(position, 1);
    vPosition = cameraModel * model * vec4(position, 1.0);

    if ((flags & MaterialFeatures_TexcoordVertexAttribute) != 0)
    {
        vTexcoord0 = texCoord0;
    }
    vNormal = mat3(modelInv) * normal;

    if ((flags & MaterialFeatures_TangentVertexAttribute) != 0) 
    {
        vTangent = tangent;
    }
}
)FOO";

        const char* fsCode =
            R"FOO(#version 450
uint MaterialFeatures_ColourTexture           = 1 << 0;
uint MaterialFeatures_NormalTexture           = 1 << 1;
uint MaterialFeatures_RoughnessTexture        = 1 << 2;
uint MaterialFeatures_OcclusionTexture        = 1 << 3;
uint MaterialFeatures_EmissiveTexture         = 1 << 4;
uint MaterialFeatures_TangentVertexAttribute  = 1 << 5;
uint MaterialFeatures_TexcoordVertexAttribute = 1 << 6; 

layout(std140, binding = 0) uniform LocalConstants
{
    mat4 cameraModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(std140, binding = 1) uniform MaterialConstant
{
    vec4 baseColourFactor;
    mat4 model;
    mat4 modelInv;

    vec3 emissiveFactor;
    float metallicFactor;

    float roughnessFactor;
    float occlusionFactor;
    uint flags;
};

layout(binding = 2) uniform sampler2D diffuseTexture;
layout(binding = 3) uniform sampler2D roughnessMetalnessTexture;
layout(binding = 4) uniform sampler2D occlusionTexture;
layout(binding = 5) uniform sampler2D emissiveTexture;
layout(binding = 6) uniform sampler2D normalTexture;

layout(location = 0) in vec2 vTexcoord0;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vTangent;
layout(location = 3) in vec4 vPosition;

layout(location = 0) out vec4 fragColour;

#define PI 3.1415926538

vec3 decodeSRGB(vec3 colour)
{
    vec3 result;
    if (colour.r <= 0.04045)
    {
        result.r = colour.r / 12.92;
    }
    else
    {
        result.r = pow((colour.r + 0.055) / 1.055, 2.4);
    }

    if (colour.g <= 0.04045)
    {
        result.g = colour.g / 12.92;
    }
    else
    {
        result.g = pow((colour.g + 0.055) / 1.055, 2.4);
    }

    if (colour.b <= 0.04045)
    {
        result.b = colour.b / 12.92;
    }
    else
    {
        result.b = pow((colour.b + 0.055) / 1.055, 2.4);
    }

    return clamp(result, 0.0, 1.0);
}

vec3 encodeSRGB(vec3 colour)
{
    vec3 result;
    if (colour.r <= 0.0031308)
    {
        result.r = colour.r * 12.92;
    }
    else
    {
        result.r = 1.055 * pow(colour.r, 1.0 / 2.4) - 0.055;
    }

    if (colour.g <= 0.0031308)
    {
        result.g = colour.g * 12.92;
    }
    else
    {
        result.g = 1.055 * pow(colour.g, 1.0 / 2.4) - 0.055;
    }

    if (colour.b <= 0.0031308)
    {
        result.b = colour.b * 12.92;
    }
    else
    {
        result.b = 1.055 * pow(colour.b, 1.0 / 2.4) - 0.055;
    }

    return clamp(result, 0.0, 1.0);
}

float heaviside(float value)
{
    if (value > 0.0) 
    {
        return 1.0;
    }
    return 0.0;
}

void main()
{
    mat3 TBN = mat3(1.0);

    if ((flags & MaterialFeatures_TangentVertexAttribute) != 0)
    {
        vec3 tangent = normalize(vTangent.xyz);
        vec3 bitangent = cross(normalize(vNormal), tangent) * vTangent.w;

        TBN = mat3(tangent, bitangent, normalize(vNormal));
    }
    else
    {
        //NOTE: Taken from https://community.khronos.org/t/computing-the-tangent-space-in-the-fragment-shader/52861
        vec3 Q1 = dFdx(vPosition.xyz);
        vec3 Q2 = dFdy(vPosition.xyz);
        vec2 st1 = dFdx(vTexcoord0);
        vec2 st2 = dFdy(vTexcoord0);

        vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
        vec3 B = normalize(-Q1 * st2.s + Q2 * st1.s);

        //The transpose of texture-to-eye space matrix.
        TBN = mat3(T, B, normalize(vNormal));
    }

    vec3 V = normalize(eye.xyz - vPosition.xyz);
    vec3 L = normalize(light.xyz - vPosition.xyz);
    //NOTE: Normal textures are encoded to [0, 1] but we need it to be maped to [-1, 1] value.
    vec3 N = normalize(vNormal);
    if ((flags & MaterialFeatures_NormalTexture) != 0) 
    {
        N = normalize(texture(normalTexture, vTexcoord0).rgb * 2.0 - 1.0);
        N = normalize(TBN * N);
    }
    vec3 H = normalize(L + V);

    float roughness = roughnessFactor;
    float metalness = metallicFactor;

    if ((flags & MaterialFeatures_RoughnessTexture) != 0) 
    {
        //Red channel for occlusion value.
        //Green channel contains roughness values.
        //Blue channel contains metalness.
        vec4 rm = texture(roughnessMetalnessTexture, vTexcoord0);

        roughness *= rm.g;
        metalness *= rm.b;
    } 

    float ao = 1.f;
    if ((flags & MaterialFeatures_OcclusionTexture) != 0) 
    {
        ao = texture(occlusionTexture, vTexcoord0).r;
    }

    float alpha = pow(roughness, 2.0);

    vec4 baseColour = baseColourFactor;

    if ((flags & MaterialFeatures_ColourTexture) != 0) 
    {
        vec4 albedo = texture(diffuseTexture, vTexcoord0);
        baseColour.rgb *= decodeSRGB(albedo.rgb);
        baseColour.a *= albedo.a;
    }

    vec3 emissive = vec3(0);
    if ((flags & MaterialFeatures_EmissiveTexture) != 0) 
    {
        vec4 e = texture(emissiveTexture, vTexcoord0);

        emissive += decodeSRGB(e.rgb) * emissiveFactor;
    }

    //NOTE: taken from https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specular-brdf
    float NdotH = dot(N, H);
    float alphaSquared = alpha * alpha;
    float dDenom = (NdotH * NdotH) * (alphaSquared - 1.0) + 1.0;
    float distribution = (alphaSquared * heaviside(NdotH)) / (PI * dDenom * dDenom);

    float NdotL = clamp(dot(N, L), 0, 1);

    if (NdotL > 1e-5)
    {
        float NdotV = dot(N, V);
        float HdotL = dot(H, L);
        float HdotV = dot(H, V);

        float visibility = (heaviside(HdotL) / (abs(NdotL) + sqrt(alphaSquared + (1.0 - alphaSquared) * (NdotL * NdotL)))) * 
                           (heaviside(HdotV) / (abs(NdotV) + sqrt(alphaSquared + (1.0 - alphaSquared) * (NdotV * NdotV))));

        float specularBrdf = visibility * distribution;

        vec3 diffuseBrdf = (1 / PI) * baseColour.rgb;

        //NOTE: f0 in the formula notation refers to the base colour here.
        vec3 conductorFresnel = specularBrdf * (baseColour.rgb + (1.0 - baseColour.rgb) * pow(1.0 - abs(HdotV), 5));

        //NOTE: f0 in the formula notation refers to the value derived from IOR = 1.5.
        float f0 = 0.04;
        float fr = f0 + (1 - f0) * pow(1 - abs(HdotV), 5);
        vec3 fresnelMix = mix(diffuseBrdf, vec3(specularBrdf), fr);

        vec3 materialColour = mix(fresnelMix, conductorFresnel, metalness);

        materialColour = emissive + mix(materialColour, materialColour * ao, occlusionFactor);

        fragColour = vec4(encodeSRGB(materialColour), baseColour.a);
    }
    else
    {
        fragColour = vec4(baseColour.rgb, baseColour.a);
    }
}
)FOO";

        pipelineCreation.shaders.setName("Cube")
            .addStage(vsCode, static_cast<uint32_t>(strlen(vsCode)), VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fsCode, static_cast<uint32_t>(strlen(fsCode)), VK_SHADER_STAGE_FRAGMENT_BIT);

        //Descriptor set layout.
        DescriptorSetLayoutCreation cubeRLLCreation = {};
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
        BufferCreation bufferCreation;
        bufferCreation.reset()
            .set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceType::Type::DYNAMIC, sizeof(UniformData))
            .setName("cubeCB");
        cubeCB = gpu.createBuffer(bufferCreation);

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

            mat4s localMatrix = {};

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
                MeshDraw meshDraw = {};

                meshDraw.materialData.model = finalMatrix;

                glTF::MeshPrimitive& meshPrimitive = mesh.primitives[primitiveIndex];

                glTF::Accessor& indicesAccessor = scene.accessors[meshPrimitive.indices];
                VOID_ASSERT(indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ||
                    indicesAccessor.componentType == glTF::Accessor::UNSIGNED_SHORT);
                meshDraw.indexType = indicesAccessor.componentType == glTF::Accessor::UNSIGNED_INT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

                glTF::BufferView& indicesBufferView = scene.bufferViews[indicesAccessor.bufferView];
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
                    glTF::BufferView& positionBufferView = scene.bufferViews[positionAccessor.bufferView];
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
                    glTF::BufferView& normalBufferView = scene.bufferViews[normalAccessor.bufferView];
                    BufferResource& normalBufferGPU = buffers[normalAccessor.bufferView];

                    meshDraw.normalBuffer = normalBufferGPU.handle;
                    meshDraw.normalOffset = normalAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : normalAccessor.byteOffset;
                }
                else
                {
                    //NOTE: Should you try and compte this at run time?
                    Array<vec3s> normalsArray = {};
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

                    BufferCreation normalCreation = {};
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
                    glTF::BufferView& tangentBufferView = scene.bufferViews[tangentAccessor.bufferView];
                    BufferResource& tangentBufferGPU = buffers[tangentAccessor.bufferView];

                    meshDraw.tangentBuffer = tangentBufferGPU.handle;
                    meshDraw.tangentOffset = tangentAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : tangentAccessor.byteOffset;

                    meshDraw.materialData.flags |= MaterialFeatures_TangentVertexAttribute;
                }

                if (texcoordAccessorIndex != -1)
                {
                    glTF::Accessor& texCooordAccessor = scene.accessors[texcoordAccessorIndex];
                    glTF::BufferView& texCoordBufferView = scene.bufferViews[texCooordAccessor.bufferView];
                    BufferResource& texCoordBufferGPU = buffers[texCooordAccessor.bufferView];

                    meshDraw.texcoordBuffer = texCoordBufferGPU.handle;
                    meshDraw.texcoordOffset = texCooordAccessor.byteOffset == glTF::INVALID_INT_VALUE ? 0 : texCooordAccessor.byteOffset;

                    meshDraw.materialData.flags |= MaterialFeatures_TexcoordVertexAttribute;
                }

                VOID_ASSERTM(meshPrimitive.material != glTF::INVALID_INT_VALUE, "Mesh with no material is not supported.");
                glTF::Material& material = scene.materials[meshPrimitive.material];

                //Descriptor set
                DescriptorSetCreation dsCreation = {};
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

    float modelScale = 1.f;
    while (Window::instance()->exitRequested == false)
    {
        //ZoneScoped;

        //New Frame
        if (Window::instance()->minimised == false)
        {
            gpu.newFrame();
        }

        inputHandler.onEvent();

        if (Window::instance()->resizeRequested)
        {
            gpu.resize(Window::instance()->width, Window::instance()->height);
            Window::instance()->resizeRequested = false;
        }
        //NOTE: This mused be after the OS messages.
        imgui->newFrame();

        const int64_t currentTick = timeNow();
        float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
        beginFrameTick = currentTick;

        inputHandler.newFrame();
        inputHandler.update();

        if (ImGui::Begin("Air ImGui"))
        {
            ImGui::InputFloat("Model Scale", &modelScale, 0.001f);
        }
        ImGui::End();

        if (ImGui::Begin("GPU"))
        {
            gpuProfiler.imguiDraw();
        }
        ImGui::End();

        mat4s globalModel = {};
        {
            //Update rotating cube data.
            MapBufferParameters cbMap = { cubeCB, 0, 0 };
            float* cbData = static_cast<float*>(gpu.mapBuffer(cbMap));
            if (cbData)
            {
                if (inputHandler.isMouseDown(MouseButtons::MOUSE_BUTTON_LEFT) && ImGui::GetIO().WantCaptureMouse == false)
                {
                    pitch += (inputHandler.mousePosition.y - inputHandler.previousMousePosition.y) * 0.1f;
                    yaw += (inputHandler.mousePosition.x - inputHandler.previousMousePosition.x) * 0.3f;

                    pitch = clamp(pitch, -60.f, 60.f);

                    if (yaw > 360.f)
                    {
                        yaw -= 360.f;
                    }

                    mat3s rxm = glms_mat4_pick3(glms_rotate_make(glm_rad(-pitch), vec3s{ 1.f, 0.f, 0.f }));
                    mat3s rym = glms_mat4_pick3(glms_rotate_make(glm_rad(-yaw), vec3s{ 0.f, 1.f, 0.f }));

                    look = glms_mat3_mulv(rxm, vec3s{ 0.f, 0.f, -1.f });
                    look = glms_mat3_mulv(rym, look);

                    right = glms_cross(look, vec3s{ 0.f, 1.f, 0.f });
                }

                if (inputHandler.isKeyDown(KEY_W))
                {
                    eye = glms_vec3_add(eye, glms_vec3_scale(look, 5.f * deltaTime));
                }
                else if (inputHandler.isKeyDown(KEY_S))
                {
                    eye = glms_vec3_sub(eye, glms_vec3_scale(look, 5.f * deltaTime));
                }

                if (inputHandler.isKeyDown(KEY_D))
                {
                    eye = glms_vec3_add(eye, glms_vec3_scale(right, 5.f * deltaTime));
                }
                else if (inputHandler.isKeyDown(KEY_A))
                {
                    eye = glms_vec3_sub(eye, glms_vec3_scale(right, 5.f * deltaTime));
                }

                mat4s view = glms_lookat(eye, glms_vec3_add(eye, look), vec3s{ 0.f, 1.f, 0.f });
                mat4s projection = glms_perspective(glm_rad(60.f), gpu.swapchainWidth * 1.f / gpu.swapchainHeight, 0.01f, 1000.f);

                //Calculate view projection matrix
                mat4s viewPerspective = glms_mat4_mul(projection, view);

                //Rotate cube??
                rX += 1.f * deltaTime;
                rY += 2.f * deltaTime;

                mat4s rxm = glms_rotate_make(rX, vec3s{ 1.f, 0.f, 0.f });
                mat4s rym = glms_rotate_make(glm_rad(45.f), vec3s{ 0.f, 1.f, 0.f });

                mat4s sm = glms_scale_make(vec3s{ modelScale, modelScale, modelScale });
                globalModel = glms_mat4_mul(rym, sm);

                //TODO: Match these name with what's in the shader.
                UniformData uniformData = {};
                uniformData.viewPerspective = viewPerspective;
                uniformData.cameraModel = globalModel;
                uniformData.eye = vec4s{ eye.x, eye.y, eye.z, 1.f };
                uniformData.light = vec4s{ 2.f, 2.f, 0.f, 1.f };

                memcpy(cbData, &uniformData, sizeof(UniformData));

                gpu.unmapBuffer(cbMap);
            }
        }

        if (Window::instance()->minimised == false)
        {
            CommandBuffer* gpuCommands = gpu.getCommandBuffer(Queue::QueueType::GRAPHICS, true);
            gpuCommands->pushMarker("Frame");

            gpuCommands->clear(0.7f, 0.9f, 1.f, 1.f);
            gpuCommands->clearDepthStencil(1.f, 0);
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

                gpuCommands->drawIndexed(Topology::TopologyType::TRIANGLE, meshDraw.count, 1, 0, 0, 0);
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