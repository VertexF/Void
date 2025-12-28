#ifndef GLTF_HDR
#define GLTF_HDR

#include "Platform.hpp"
#include "Memory.hpp"
#include "String.hpp"
#include "File.hpp"

namespace 
{
    const char* DEFAULT_3D_MODEL = "Assets/Models/2.0/Sponza/glTF/Sponza.gltf";
}

//I might try to remove this later.
#define InjectDefault3DModel() \
if (fileExists(DEFAULT_3D_MODEL)) \
{\
    argc = 2;\
    argv[1] = const_cast<char*>(DEFAULT_3D_MODEL);\
}\
else \
{\
    vprint("Could not find file.");\
    exit(-1);\
}\


//This is mostly a plain old data place file to store all the GLTF
namespace glTF 
{
    static const int32_t INVALID_INT_VALUE = 2147483647;
    static_assert(INVALID_INT_VALUE == INT32_MAX, "Mismatch between invalise in and INT32_MAX");
    static const float INVALID_FLOAT_VALUE = 3.402823466e+38f;

    struct Asset 
    {
        StringBuffer copyright;
        StringBuffer generator;
        StringBuffer minVersion;
        StringBuffer version;
    };

    struct CamerOrthographic 
    {
        float xMag;
        float yMag;
        float zFar;
        float zNear;
    };

    struct AccessorSparse 
    {
        int32_t count;
        int32_t indices;
        int32_t value;
    };

    struct Camera 
    {
        int32_t orthographic;
        int32_t perspective;
        //Perspective
        //Orthographic
        StringBuffer type;
    };

    struct AnimationChannel 
    {
        enum TargetType 
        {
            Translation, Rotation, Scale, Weights, Count
        };

        int32_t sampler;
        int32_t targetNode;
        TargetType targetType;
    };

    struct AnimationSampler 
    {
        //The index of an accessor containing keyframe timestamps.
        int32_t inputKeyframeBufferIndex;
        //The index of an accessor containing keyframe output values.
        int32_t outputKeyframeBufferIndex;

        //Linear The animated values are linear interpolated between keyframe. When targeting a rotation, spherical linear interpolation slerp
        // SHOULD be used to interpolate quaternions. The float of output elements MUST equal the float of input elements.
        //Step the animated values remain constant to the output of the first keyframe, until the next keyframe. The float of output elements 
        // MUST equal the float of the input elements.
        //Cubicspline the animation's interpolation is computed using a cubic spline with specified tangents. The float of output 
        // MUST equal three times the float of input elements. For each input element, the output stores three elements, an in-tagent, 
        // a spline vertex, and an out-tagent. MUST be at least two keyframes when using this interpolate.
        enum Interpolation 
        {
            Linear, Step, CubicSpline, Count
        };
        Interpolation interpolation;
    };

    struct Skin 
    {
        int32_t inverseBindMatricesBufferIndex;
        int32_t skeletonRootNodeIndex;
        uint32_t jointsCount;
        int32_t* joints;
    };

    struct BufferView 
    {
        enum Target 
        {
            ARRAY_BUFFER = 34962 /*Vertex Data*/, ELEMENT_ARRAY_BUFFER = 34963 /* Index Data*/
        };

        int32_t buffer;
        int32_t byteLength;
        int32_t byteOffset;
        int32_t byteStride;
        int32_t target;
        StringBuffer name;
    };

    struct Image 
    {
        int32_t bufferView;
        //image/jpeg
        //image/png
        StringBuffer mineType;
        StringBuffer uri;
    };

    struct Node 
    {
        int32_t camera;
        uint32_t childrenCount;
        int32_t* children;
        uint32_t matrixCount;
        float* matrix;
        int32_t mesh;
        uint32_t rotationCount;
        float* rotation;
        uint32_t scaleCount;
        float* scale;
        int32_t skin;
        uint32_t translationCount;
        float* translation;
        uint32_t weightsCount;
        float* weights;
        StringBuffer name;
    };

    struct TextureInfo 
    {
        int32_t index;
        int32_t texCoord;
    };

    struct MaterialPBRMetallicRoughness 
    {
        uint32_t baseColourFactorCount;
        float* baseColourFactor;
        TextureInfo* baseColourTexture;
        float metallicFactor;
        TextureInfo* metallicRoughnessTexture;
        float roughnessFactor;
    };

    struct MeshPrimitive 
    {
        struct Attribute 
        {
            StringBuffer key;
            int32_t accessorIndex;
        };

        uint32_t attributeCount;
        Attribute* attributes;
        int32_t indices;
        int32_t material;
        // 0 Point
        // 1 Lines
        // 2 Line_Loops
        // 3 Line_Strip
        // 4 Triangles
        // 5 Triangle_Strip
        // 6 Triangle_fan
        int32_t mode;
        //TODO: deal with the this json object
        //object* targets;
    };

    struct AccessorSpartseIndices 
    {
        int32_t bufferView;
        int32_t byteOffset;
        // 5121 UNSIGNED_BYTE
        // 5123 UNSIGNED_SHORT
        // 5125 UNSIGNED_INT
        int32_t componentType;
    };

    struct Accessor 
    {
        enum ComponentType 
        {
            BYTE = 5120,
            UNSIGNED_BYTE = 5121,
            SHORT = 5122,
            UNSIGNED_SHORT = 5123,
            UNSIGNED_INT = 2125,
            FLOAT = 5126
        };

        enum Type 
        {
            Scalar,
            Vec2,
            Vec3,
            Vec4,
            Mat2,
            Mat3,
            Mat4
        };

        int32_t bufferView;
        int32_t byteOffset;

        int32_t componentType;
        int32_t count;
        uint32_t maxCount;
        float* max;
        uint32_t minCount;
        float* min;
        bool normalised;
        int32_t sparse;
        Type type;
    };

    struct Texture 
    {
        int32_t sampler;
        int32_t source;
        StringBuffer name;
    };

    struct MaterialNormalTextureInfo 
    {
        int32_t index;
        int32_t texCoord;
        float scale;
    };

    struct Mesh 
    {
        uint32_t primitiveCount;
        MeshPrimitive* primitives;
        uint32_t weightCount;
        float* weights;
        StringBuffer name;
    };

    struct MaterialOcclusionTextureInfo 
    {
        int32_t index;
        int32_t texCoord;
        float strength;
    };

    struct Material 
    {
        float alphaCutOff;
        //OPAQUE The alpha value is ignored and the rendered output is fully opaque.
        //MASK The rendered output is either fully opaque or fully transparent depending on the alpha value and the specified "alphaCutoff"
        // value. The exact apparence of the edges MAY be subject to the implementation-specific techniques such as "Alpha-to-Coverage."
        //BLEND The alpha value is used to composite the source and destination areas. The rendered output is combined with the background
        // using the normal painting operation (i.e the Porter and Duffer over operator);
        StringBuffer alphaMode;
        bool doubleSided;
        uint32_t emissiveFactorCount;
        float* emissiveFactor;
        TextureInfo* emissiveTexture;
        MaterialNormalTextureInfo* normalTexture;
        MaterialOcclusionTextureInfo* occlusionTexture;
        MaterialPBRMetallicRoughness* pbrMetallicRoughness;
        StringBuffer name;
    };

    struct Buffer 
    {
        int32_t byteLength;
        StringBuffer uri;
        StringBuffer name;
    };

    struct CameraPerspective 
    {
        float apsectRatio;
        float fov;
        float zfar;
        float zNear;
    };

    struct Animation 
    {
        uint32_t channelsCount;
        AnimationChannel* channels;
        uint32_t samplersCount;
        AnimationSampler* sampler;
    };

    struct AccessorSparseValues 
    {
        int32_t bufferView;
        int32_t byteOffset;
    };

    struct Scene 
    {
        uint32_t nodesCount;
        int32_t* nodes;
    };

    struct Sampler 
    {
        enum Filter 
        {
            NEAREST = 9728,
            LINEAR = 9729,
            NEAREST_MIPMAP_NEAREST = 9984,
            LINEAR_MIPMAP_NEAREST = 9985,
            NEAREST_MIPMAP_LINEAR = 9986,
            LINEAR_MIPMAP_LINEAR = 9987
        };

        enum Wrap 
        {
            CLAMP_TO_EDGE = 33071,
            MIRROR_REPEAT = 33648,
            REPEAT = 10497 
        };

        int32_t magFilter;
        int32_t minFilter;
        int32_t wrapS;
        int32_t wrapT;
    };

    struct glTF 
    {
        uint32_t accessorsCount;
        Accessor* accessors;

        uint32_t animationsCount;
        Animation* animation;
        Asset asset;

        uint32_t bufferViewCount;
        BufferView* bufferViews;
        uint32_t buffersCount;
        Buffer* buffers;

        uint32_t camerasCount;
        Camera* camera;

        uint32_t extensionRequiredCount;
        StringBuffer* extensionsRequired;
        uint32_t extensionsUsedCount;
        StringBuffer* extensionUsed;

        uint32_t imagesCount;
        Image* images;

        uint32_t materialsCounts;
        Material* materials;

        uint32_t meshCount;
        Mesh* meshes;

        uint32_t nodesCount;
        Node* nodes;
        uint32_t samplersCount;
        Sampler* samplers;

        int32_t scene;
        uint32_t scenesCount;
        Scene* scenes;

        uint32_t skinsCount;
        Skin* skins;

        uint32_t texturesCount;
        Texture* textures;

        StackAllocator allocator;
    };

    int32_t getDataOffset(int32_t accessorOffset, int32_t bufferViewOffset);
}//GLFT

glTF::glTF gltfLoadFile(const char* filePath);
void gltfFree(glTF::glTF& scene);
int32_t gltfGetAttributeAccessorIndex(glTF::MeshPrimitive::Attribute* attributes, uint32_t atributeCount, const char* attributeName);

#endif // !GLTF_HDR
