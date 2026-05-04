#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

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

struct SceneData
{
    mat4 viewPerspective;
    mat4 globalModel;
    vec4 eye;
    vec4 light;
};

struct Vertices
{
	float pad;
};

struct ModelPosition
{
	float pad;
};

layout(scalar, buffer_reference) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(scalar, buffer_reference) readonly buffer ModelPositionData
{
    ModelPosition modelPositions[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer SceneBufferData
{
    SceneData sceneData;
};

layout(scalar, set = 0, binding = 0) uniform SkyboxData
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

layout (location = 0) out vec3 dir;

layout(scalar, push_constant) uniform entityIndex
{
    VertexData vertexDataReference;
    ModelPositionData modelPositionsReference;
    SceneBufferData sceneBufferReference;
};

void main()
{
	mat4 viewProject = sceneBufferReference.sceneData.viewPerspective;
	viewProject[3][0] = 0;
	viewProject[3][1] = 0;
	viewProject[3][2] = 0;
	viewProject[3][3] = 1;

	//mat4 viewP = mat4(mat3(viewProject));

	int idx = indices[gl_VertexIndex];
    vec4 pos = viewProject * vec4(pos[idx], 1.0);
	dir = pos.xyz;
    //gl_Position = pos.xyww;
	gl_Position = vec4(pos.x, pos.y, 0.f, pos.w);
}
