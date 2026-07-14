//Note this is the highest prime number closet 20000.
const uint MAX_INSTANCE_COUNT = 19997;

struct ParticleData
{
    vec3 position;
    uint particleSet;
};

struct ParticleSet
{
    mat4 transform;
    vec4 colour;
    vec2 texCoords[4];
    uint textureID;
    float pad[3];
};
