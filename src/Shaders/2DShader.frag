#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 vTexcoord;
layout(location = 1) in vec4 vColour;
layout(location = 2) flat in uint textureID;

layout(location = 0) out vec4 outColour;

layout(set = 0, binding = 0) uniform sampler2D globalTextures[];
//Alias textures to use the same binding point, as bindless texture is shared
//between all kind of textures: 1d, 2d, 3d.
layout(set = 0, binding = 0) uniform sampler3D globalTextures3D[];
layout(set = 0, binding = 0) uniform samplerCube globalTexturesCube[];

//Write only image do not need formatting in layout.
layout(set = 0, binding = 1) writeonly uniform image2D globalImages2D[];

#define INVALID_TEXTURE_INDEX 65535

void main()
{
	vec4 textureColour = vColour;
	if(textureID != INVALID_TEXTURE_INDEX)
	{
		textureColour = textureColour * texture(globalTextures[nonuniformEXT(textureID)], vTexcoord.st);
		if(textureColour.a == 0.f)
		{
			discard;
		}
	}

    outColour = vec4(1.f, 0.f, 0.f, 1.f);
}