#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 UV;
layout(location = 2) in uvec4 colour;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColour;

layout(std140, binding = 0) uniform LocalConstants 
{ 
	mat4 projectionMatrix; 
};

void main()
{
   fragUV = UV;
   fragColour = colour / 255.0f;
   gl_Position = projectionMatrix * vec4(position.xy, 0, 1);
}