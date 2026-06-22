#version 460

#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_ARB_gpu_shader_int64 : enable

struct Vertices
{
    float px, py, pz;
    uint8_t tx, ty, tz, tw;
    uint8_t nx, ny, nz, nw;
    float16_t tu, tv;
};

//Here we are going to attempt full bindless for the debug renderer to make this as painless as possible in the future.
struct ModelPosition
{
    mat4 pos; 
    mat4 debugModel;
    vec4 colour;
    float padd[4];
};

struct SceneData
{
    mat4 view;
    mat4 project;
    mat4 globalModel;
    vec4 eye;
    vec4 light;
};

layout(scalar, set = 0, binding = 0) uniform MaterialConstant
{
    mat4 model;
    mat4 modelInv;
    
    uvec4 textures;
    vec4 baseColourFactor;
    vec4 metallicRoughnessOcclusionFactor;
    float alphaCutoff;
    float iorFactor;
    
    vec3 emissiveFactor;
    uint emissiveTextureIndex;
    vec3 specularValue;
    uint flags;
};

layout(buffer_reference, buffer_reference_align = 8, scalar) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(buffer_reference, buffer_reference_align = 8, scalar) readonly buffer ModelPositionData
{
    ModelPosition modelPositions[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer SceneBufferData
{
    SceneData sceneData;
};

layout(scalar, push_constant) uniform entityIndex
{
    VertexData vertexDataReference;
    ModelPositionData modelPositionsReference;
    SceneBufferData sceneBufferReference;
};

layout(location = 0) out vec2 vTexcoord0;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vTangent;
layout(location = 3) out vec4 vPosition;
layout(location = 4) out vec4 vColour;

mat3 adjugate(in mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz), 
                cross(m[2].xyz, m[0].xyz), 
                cross(m[0].xyz, m[1].xyz));
}

void main()
{
    vec3 position = vec3(vertexDataReference.vertexData[gl_VertexIndex].px, 
                         vertexDataReference.vertexData[gl_VertexIndex].py, 
                         vertexDataReference.vertexData[gl_VertexIndex].pz);

    vec4 tangent = vec4(int(vertexDataReference.vertexData[gl_VertexIndex].tx), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].ty), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].tz), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].tw)) / 127.f - 1.0;

    vec3 normal = vec3(int(vertexDataReference.vertexData[gl_VertexIndex].nx), 
                       int(vertexDataReference.vertexData[gl_VertexIndex].ny),
                       int(vertexDataReference.vertexData[gl_VertexIndex].nz)) / 127.f - 1.0;

    vec2 texcoord = vec2(vertexDataReference.vertexData[gl_VertexIndex].tu, vertexDataReference.vertexData[gl_VertexIndex].tv);

    mat4 modelPostion = modelPositionsReference.modelPositions[gl_InstanceIndex].pos * model;

    gl_Position = sceneBufferReference.sceneData.project * sceneBufferReference.sceneData.view * sceneBufferReference.sceneData.globalModel * modelPostion * vec4(position, 1.0);
    vPosition  =  sceneBufferReference.sceneData.globalModel * modelPostion * vec4(position, 1.0);

    vTexcoord0 = texcoord;
    vNormal = mat3(adjugate(modelPostion)) * normal;

    vTangent = tangent;
    vColour = vec4(1.f, 1.f, 1.f, 1.f);//modelPositionsReference.modelPositions[2].colour;
}