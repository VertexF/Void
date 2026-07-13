#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_ARB_gpu_shader_int64 : enable

const vec3 pos[4] = vec3[4]
(
	vec3(0.0, 0.0, 0.0),
	vec3(1.0, 0.0, 0.0),
	vec3(1.0, 1.0, 0.0),
	vec3(0.0, 1.0, 0.0)
);

const int indices[6] = int[6]
(
	0, 1, 2, 2, 3, 0
);

struct QuadPosition
{
    mat4 transform;
    vec4 colour;
    vec2 texCoords[4];
    uint textureID;
    float pad[3];
};

struct ParticleData
{
    vec3 position;
    uint particleSet;
};

struct SceneData2D
{
    mat4 view;
    mat4 project;
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer QuadPositionData
{
    QuadPosition quadPositions[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer SceneBuffer2DData
{
    SceneData2D sceneData2D;
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer Particles
{
    ParticleData particleData[];
};

layout(scalar, push_constant) uniform entityIndex
{
    QuadPositionData quadPositionsReference;
    SceneBuffer2DData scene2D;
    Particles particleDataPtr;
};

layout(location = 0) out vec2 vTexcoord;
layout(location = 1) out vec4 vColour;
layout(location = 2) flat out uint textureID;

void main()
{
    int idx = indices[gl_VertexIndex];
    vec3 position = pos[idx];

    vec2 texcoord = vec2(quadPositionsReference.quadPositions[gl_InstanceIndex].texCoords[idx].x, quadPositionsReference.quadPositions[gl_InstanceIndex].texCoords[idx].y);

    //project is an othorgraphical projection not a perspective.
    gl_Position = scene2D.sceneData2D.project * quadPositionsReference.quadPositions[gl_InstanceIndex].transform * vec4(position, 1.0);

    textureID = quadPositionsReference.quadPositions[gl_InstanceIndex].textureID;

    vTexcoord = texcoord;

    vColour = quadPositionsReference.quadPositions[gl_InstanceIndex].colour;
}