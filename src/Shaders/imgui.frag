#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColour;

layout(location = 0) out vec4 outColour;

layout(binding = 1) uniform sampler2D Texture;

void main()
{
   outColour = fragColour * texture(Texture, fragUV.st);
}