#version 450

layout (std140, binding = 0) uniform ModelData
{
	mat4 model;
	mat4 view;
	mat4 proj;
}modelData;

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec3 inColour; 

layout (location = 0) out vec3 fragColour;

void main()
{
	gl_Position = modelData.proj * modelData.view * modelData.model * vec4(inPosition, 0.0, 1.0);
	fragColour = inColour;
}