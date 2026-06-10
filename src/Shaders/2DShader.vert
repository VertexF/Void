#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_ARB_gpu_shader_int64 : enable

struct Vertices
{
    float px, py, pz;
    float padd;
    float16_t tu, tv;
};

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

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer ModelPositionData
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

layout(location = 0) out vec2 vTexcoord;
layout(location = 1) out vec4 vColour;
layout(location = 2) flat out uint textureID;

void main()
{
    vec3 position = vec3(vertexDataReference.vertexData[gl_VertexIndex].px, 
                         vertexDataReference.vertexData[gl_VertexIndex].py, 
                         vertexDataReference.vertexData[gl_VertexIndex].pz);

    vec2 texcoord = vec2(vertexDataReference.vertexData[gl_VertexIndex].tu, vertexDataReference.vertexData[gl_VertexIndex].tv);

    gl_Position = sceneBufferReference.sceneData.project * sceneBufferReference.sceneData.view * modelPositionsReference.modelPositions[gl_InstanceIndex].pos * vec4(position, 1.0);

    textureID = gl_InstanceIndex;

    vTexcoord = texcoord;

    vColour = modelPositionsReference.modelPositions[gl_InstanceIndex].colour;
}