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
    //We also need the global scale to be the same as the regular geometry other wise things will be too small.
    mat4 globalModel;
    //We need this because the final matrix that comes out the glb after multiplying 
    //all local nodes together needs to be the same as the collision geometry.
    //Meaning that model matrix we get out of the actual geometry needs to be given to the debug geometry if they tied together when creating the buffer.
    mat4 model;
    mat4 viewPerspective;
    //Colour will be used as a key for various different objects.
    vec4 colour;
    float pad[4];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer ModelPositionData
{
    ModelPosition modelPositions[];
};

layout(scalar, push_constant) uniform entityIndex
{
    VertexData vertexDataReference;
    ModelPositionData modelPositionsReference;
    uint index;
};

//Pipeline layout needs changing over the default one.
layout(location = 0) out vec4 vColour;

void main()
{
    vec3 position = vec3(vertexDataReference.vertexData[gl_VertexIndex].px, 
                         vertexDataReference.vertexData[gl_VertexIndex].py, 
                         vertexDataReference.vertexData[gl_VertexIndex].pz);

    gl_Position = modelPositionsReference.modelPositions[index].viewPerspective * modelPositionsReference.modelPositions[index].globalModel * modelPositionsReference.modelPositions[index].model * modelPositionsReference.modelPositions[index].pos * vec4(position, 1.0);
    vColour = modelPositionsReference.modelPositions[index].colour;
}