#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColour;
layout(location = 2) flat in uint textureID;

layout(location = 0) out vec4 outColour;

layout(set = 1, binding = 10) uniform sampler2D textures[];

void main()
{
   outColour = fragColour * texture(textures[nonuniformEXT(textureID)], fragUV.st);
}
