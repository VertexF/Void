#ifndef LOAD_GLTF_HDR
#define LOAD_GLTF_HDR

#include "Foundation/Array.hpp"

#include "GPUDevice.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

#include <cgltf.h>

struct Vertices
{
    float position[3];
    uint8_t tangent[4];
    uint8_t normals[4];
    uint16_t texCoord0[2];
};

struct ColliderVertices
{
    float position[3];
};

struct MeshDraw
{
    mat4s model;

    vec4s baseColourFactor;
    vec4s metallicRoughnessOcclusionFactor;
    vec3s scale;
    vec3s emissiveFactor;
    vec3s specularValue;

    float alphaCutoff;
    float iorFactor;

    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;
    BufferHandle materialBuffer;

    uint32_t indexOffset;

    uint32_t count;
    uint32_t flags;

    VkIndexType indexType;

    DescriptorSetHandle descriptorSet;

    VkIndexType componentType;

    //Indices used for bindless textures.
    uint16_t diffuseTextureIndex;
    uint16_t roughnessTextureIndex;
    uint16_t normalTextureIndex;
    uint16_t occlusionTextureIndex;
    uint16_t emisiveTextureIndex;
};

struct MaterialData
{
    mat4s model;
    mat4s modelInv;

    uint32_t textures[4];
    vec4s baseColourFactor;
    vec4s metallicRoughnessOcclusionFactor;
    float alphaCutoff;
    float iorFactor;

    vec3s emissiveFactor;
    uint32_t emissiveTextureIndex;

    vec3s specularValue;
    uint32_t flags;
};

struct cgltf_data;

struct Model
{
    void loadModel(const char* modelPath, GPUDevice& gpu, DescriptorSetLayoutHandle descriptorSetLayout);
    void shutdownModel(GPUDevice& gpu);

    Array<MeshDraw> meshDraws;

    mat4s finalMatrix;

    Array<SamplerHandle> samplers;
    Array<TextureHandle> images;
    Array<cgltf_node> nodeStack;

    StringBuffer resourceNameBuffer;

    HeapAllocator* allocator;
    StackAllocator* scratchAllocator;

    SamplerHandle dummySampler;
    //This value tracks how many of the same model we have. This is to support instance rendering. 
    uint32_t instanceCount;
};

struct DebugModel 
{
    void loadCollider(const char* modelPath, GPUDevice& gpu);
    void shutdownModel(GPUDevice& gpu);

    Array<MeshDraw> meshDraws;
    Array<cgltf_node> nodeStack;

    mat4s finalMatrix;

    HeapAllocator* allocator;
    StackAllocator* scratchAllocator;

    //This value tracks how many of the same model we have. This is to support instance rendering. 
    uint32_t instanceCount;
};

#endif // !LOAD_GLTF_HDR
