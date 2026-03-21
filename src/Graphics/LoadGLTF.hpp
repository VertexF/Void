#ifndef LOAD_GLTF_HDR
#define LOAD_GLTF_HDR

#include "Foundation/Array.hpp"

#include "GPUDevice.hpp"
#include "Renderer.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/affine.h"

struct Vertices
{
    float position[3];
    uint8_t tangent[4];
    uint8_t normals[4];
    uint16_t texCoord0[2];
};

struct MeshDraw
{
    mat4s model;

    vec4s baseColourFactor;
    vec4s metallicRoughnessOcclusionFactor;
    vec3s scale;
    vec3s emissiveFactor;

    float alphaCutoff;

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

    vec3s emissiveFactor;
    uint32_t emissiveTextureIndex;
    uint32_t flags;
};

struct Model
{
    Array<MeshDraw> meshDraws;

    void loadModel(const char* modelPath, GPUDevice& gpu, Renderer& renderer, BufferHandle sceneBuffer, DescriptorSetLayoutHandle descriptorSetLayout);
    void shutdownModel(GPUDevice& gpu, Renderer& renderer);
};
#endif // !LOAD_GLTF_HDR
