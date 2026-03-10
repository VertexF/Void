#version 450

#extension GL_EXT_scalar_block_layout : require
// Bindless support
// Enable non uniform qualifier extension
#extension GL_EXT_nonuniform_qualifier : require

layout(scalar, set = 0, binding = 0) uniform LocalConstants
{
    mat4 globalModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(scalar, set = 0, binding = 1) uniform SkyboxData
{
    vec3 testColour;
    uint skyboxTextureIndex;
};

layout(set = 1, binding = 0) uniform sampler2D globalTextures[];
//Alias textures to use the same binding point, as bindless texture is shared
//between all kind of textures: 1d, 2d, 3d.
layout(set = 1, binding = 0) uniform sampler3D globalTextures3D[];

layout(set = 1, binding = 0) uniform samplerCube globalTexturesCube[];

//Write only image do not need formatting in layout.
layout(set = 1, binding = 1) writeonly uniform image2D globalImages2D[];


layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 fragColour;

void main()
{
	fragColour = texture(globalTexturesCube[nonuniformEXT(skyboxTextureIndex)], dir);
	//fragColour = vec4(testColour, 1.0);
}
