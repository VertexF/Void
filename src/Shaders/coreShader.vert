#version 450

#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

uint MaterialFeatures_TangentVertexAttribute  = 1 << 5; 

struct Vertices
{
    float px, py, pz;
    uint8_t tx, ty, tz, tw;
    uint8_t nx, ny, nz, nw;
    float16_t tu, tv;
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

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer ModelPositionData
{
    ModelPosition modelPositions[];
};

layout(scalar, push_constant) uniform entityIndex
{
    ModelPositionData modelPositionsReference;
    VertexData vertexDataReference;
    uint index;
};

layout(location = 0) out vec2 vTexcoord0;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vTangent;
layout(location = 3) out vec4 vPosition;

void main()
{
    vec3 position = vec3(vertexDataReference.vertexData[gl_VertexIndex].px, 
                         vertexDataReference.vertexData[gl_VertexIndex].py, 
                         vertexDataReference.vertexData[gl_VertexIndex].pz);

    vec4 tagent = vec4(int(vertexDataReference.vertexData[gl_VertexIndex].tx), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].ty), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].tz), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].tw)) / 127.f - 1.0;

    vec3 normal = vec3(int(vertexDataReference.vertexData[gl_VertexIndex].nx), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].ny),
                       int(vertexDataReference.vertexData[gl_VertexIndex].nz)) / 127.f - 1.0;

    vec2 texcoord = vec2(vertexDataReference.vertexData[gl_VertexIndex].tu, vertexDataReference.vertexData[gl_VertexIndex].tv);

    gl_Position = viewPerspective * globalModel * model * modelPositionsReference.modelPositions[index].pos * vec4(position, 1.0);
    vPosition  =  globalModel * model * modelPositionsReference.modelPositions[index].pos * vec4(position, 1.0);

    vTexcoord0 = texcoord;
    vNormal = mat3(modelInv) * normal;

    vTangent = tagent;
}