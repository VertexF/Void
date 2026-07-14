#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_include_directive: require

#include "ParticleData.h"

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

struct SceneData2D
{
    mat4 view;
    mat4 project;
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer ParticleSetData
{
    ParticleSet particleSets[];
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
    ParticleSetData particleSetsPtr;
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

    uint instance = gl_InstanceIndex + (MAX_INSTANCE_COUNT * gl_DrawID);

    uint particleSet = particleDataPtr.particleData[instance].particleSet;

    vec2 texcoord = vec2(particleSetsPtr.particleSets[particleSet].texCoords[idx].x, 
                         particleSetsPtr.particleSets[particleSet].texCoords[idx].y);

    vec3 cameraRightWorld = { scene2D.sceneData2D.view[0][0], scene2D.sceneData2D.view[1][0], scene2D.sceneData2D.view[2][0] };
    vec3 cameraUpWorld = { scene2D.sceneData2D.view[0][1], scene2D.sceneData2D.view[1][1], scene2D.sceneData2D.view[2][1] };
    vec3 positionWorld = particleDataPtr.particleData[instance].position + position.x * cameraRightWorld + 
                                                                                   position.y * cameraUpWorld;

    gl_Position = scene2D.sceneData2D.project * scene2D.sceneData2D.view * vec4(positionWorld, 1.0);

    textureID = particleSetsPtr.particleSets[particleSet].textureID;

    vTexcoord = texcoord;

    vColour = particleSetsPtr.particleSets[particleSet].colour;
}