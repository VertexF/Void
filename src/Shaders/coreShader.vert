#version 450

#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_scalar_block_layout : require

uint MaterialFeatures_TangentVertexAttribute  = 1 << 5; 

struct Vertices
{
    float tx, ty, tz, tw;
    float px, py, pz;
    float nx, ny, nz;
    float tu, tv;
};

struct ModelPosition
{
   mat4 pos;  
};

layout(scalar, set = 0, scalar, binding = 0) uniform LocalConstants
{
    mat4 globalModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(scalar, set = 0, scalar, binding = 1) uniform MaterialConstant
{
    mat4 model;
    mat4 modelInv;
    
    uvec4 textures;
    vec4 baseColourFactor;
    vec4 metallicRoughnessOcclusionFactor;
    float alphaCutoff;
    float pad[3];
    
    vec3 emissiveFactor;
    uint emissiveTextureIndex;
    uint flags;
};

layout(scalar, set = 0, binding = 2) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(scalar, set = 2, binding = 0) readonly buffer ModelPositionData
{
    ModelPosition modelPositions[];
};

layout(push_constant, std140) uniform entityIndex
{
    uint index;
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

    gl_Position = viewPerspective * globalModel * model * modelPositions[index].pos * vec4(position, 1.0);
    vPosition  =  globalModel * model * modelPositions[index].pos * vec4(position, 1.0);

    vTexcoord0 = texcoord;
    vNormal = mat3(modelInv) * normal;

    vTangent = tagent;
}