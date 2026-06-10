#version 460

#extension GL_ARB_shader_draw_parameters: require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require

struct Vertices
{
    float px, py, pz;
};

//Here we are going to attempt full bindless for the debug renderer to make this as painless as possible in the future.
struct ModelPosition
{
    mat4 pos; 
    //all local nodes together needs to be the same as the collision geometry.
    //Meaning that model matrix we get out of the actual geometry needs to be given to the debug geometry if they tied together when creating the buffer.
    mat4 debugModel;
    //Colour will be used as a key for various different objects.
    vec4 colour;
    float padd[4];
};

struct SceneData
{
    mat4 view;
    mat4 project;
    mat4 globalModel;
    vec4 eye;
    vec4 light;
};

struct DebugModel
{
    mat4 model;
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

layout(scalar, push_constant) uniform entityIndex
{
    VertexData vertexDataReference;
    ModelPositionData modelPositionsReference;
    SceneBufferData sceneBufferReference;
};

//Pipeline layout needs changing over the default one.
layout(location = 0) out vec4 vColour;

void main()
{
    vec3 position = vec3(vertexDataReference.vertexData[gl_VertexIndex].px, 
                         vertexDataReference.vertexData[gl_VertexIndex].py, 
                         vertexDataReference.vertexData[gl_VertexIndex].pz);

    gl_Position = sceneBufferReference.sceneData.project * sceneBufferReference.sceneData.view * sceneBufferReference.sceneData.globalModel * modelPositionsReference.modelPositions[gl_InstanceIndex].pos * modelPositionsReference.modelPositions[gl_InstanceIndex].debugModel * vec4(position, 1.0);
    vColour = modelPositionsReference.modelPositions[gl_InstanceIndex].colour;
}