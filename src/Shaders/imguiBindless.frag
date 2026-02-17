#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColour;
layout(location = 2) flat in uint textureID;

layout(location = 0) out vec4 outColour;

layout(set = 1, binding = 0) uniform sampler2D globalTextures[];
//Alias textures to use the same binding point, as bindless texture is shared
//between all kind of textures: 1d, 2d, 3d.
layout(set = 1, binding = 0) uniform sampler3D globalTextures3D[];

//Write only image do not need formatting in layout.
layout(set = 1, binding = 1) writeonly uniform image2D globalImages2D[];

void main()
{
   outColour = fragColour * texture(globalTextures[nonuniformEXT(textureID)], fragUV.st);
}
