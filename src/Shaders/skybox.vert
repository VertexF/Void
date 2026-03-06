#version 450

#extension GL_EXT_scalar_block_layout : require

const vec3 pos[8] = vec3[8]
(
	vec3(-1.0,-1.0, 1.0),
	vec3( 1.0,-1.0, 1.0),
	vec3( 1.0, 1.0, 1.0),
	vec3(-1.0, 1.0, 1.0),

	vec3(-1.0,-1.0,-1.0),
	vec3( 1.0,-1.0,-1.0),
	vec3( 1.0, 1.0,-1.0),
	vec3(-1.0, 1.0,-1.0)
);

const int indices[36] = int[36]
(
	0, 1, 2, 2, 3, 0,	// front
	1, 5, 6, 6, 2, 1,	// right 
	7, 6, 5, 5, 4, 7,	// back
	4, 0, 3, 3, 7, 4,	// left
	4, 5, 1, 1, 0, 4,	// bottom
	3, 2, 6, 6, 7, 3	// top
);

layout(scalar, set = 0, binding = 0) uniform LocalConstants
{
    mat4 globalModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(scalar, set = 0, binding = 1) uniform MaterialConstant
{
    mat4 model;
    mat4 modelInv;
    
    uvec4 textures;
    vec4 baseColourFactor;
    vec4 metallicRoughnessOcclusionFactor;
    float alphaCutoff;
    
    vec3 emissiveFactor;
    uint emissiveTextureIndex;
    uint skyboxIndex;
    uint flags;
};

layout(set = 1, binding = 0) uniform sampler2D globalTextures[];
//Alias textures to use the same binding point, as bindless texture is shared
//between all kind of textures: 1d, 2d, 3d.
layout(set = 1, binding = 0) uniform sampler3D globalTextures3D[];

//Write only image do not need formatting in layout.
layout(set = 1, binding = 1) writeonly uniform image2D globalImages2D[];


layout (location = 0) out vec3 dir;

void main()
{
	int idx = indices[gl_VertexIndex];
	gl_Position = viewPerspective * vec4(pos[idx], 1.0);
	dir = pos[idx].xyz;

//	int idx = indices[gl_VertexIndex];
//    vec4 pos = viewPerspective * vec4(pos[idx], 1.0);
//	dir = pos.xyz;
//    gl_Position = pos.xyww;
}
