#version 450
uint MaterialFeatures_ColourTexture           = 1 << 0;
uint MaterialFeatures_NormalTexutre           = 1 << 1;
uint MaterialFeatures_RoughnessTexture        = 1 << 2;
uint MaterialFeatures_OcclusionTexture        = 1 << 3;
uint MaterialFeatures_EmissiveTexture         = 1 << 4;
uint MaterialFeatures_TangentVertexAttribute  = 1 << 5;
uint MaterialFeatures_TexcoordVertexAttribute = 1 << 6; 

struct Vertices
{
    float tx, ty, tz, tw;
    float px, py, pz;
    float nx, ny, nz;
    float tu, tv;
};

layout(std140, binding = 0) uniform LocalConstants
{
    mat4 globalModel;
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

layout(binding = 7) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(location = 0) out vec2 vTexcoord0;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vTangent;
layout(location = 3) out vec4 vPosition;

void main()
{
    vec3 position = vec3(vertexData[gl_VertexIndex].px, vertexData[gl_VertexIndex].py, vertexData[gl_VertexIndex].pz);
    vec4 tagent = vec4(vertexData[gl_VertexIndex].tx, vertexData[gl_VertexIndex].ty, vertexData[gl_VertexIndex].tz, vertexData[gl_VertexIndex].tw);
    vec3 normal = vec3(vertexData[gl_VertexIndex].nx, vertexData[gl_VertexIndex].ny, vertexData[gl_VertexIndex].nz);
    vec2 texcoord = vec2(vertexData[gl_VertexIndex].tu, vertexData[gl_VertexIndex].tv);

    gl_Position = viewPerspective * globalModel * vec4(position, 1.0);
    vPosition =  globalModel * vec4(position, 1.0);

    if ((flags & MaterialFeatures_TexcoordVertexAttribute) != 0)
    {
        vTexcoord0 = texcoord;
    }
    vNormal = mat3(modelInv) * normal;

    if ((flags & MaterialFeatures_TangentVertexAttribute) != 0) 
    {
        vTangent = tagent;
    }
}