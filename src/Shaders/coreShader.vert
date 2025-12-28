#version 450

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