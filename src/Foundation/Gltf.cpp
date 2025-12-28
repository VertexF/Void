#include "Gltf.hpp"

#include "vender/json.hpp"

#include "Assert.hpp"
#include "File.hpp"

using json = nlohmann::json;

static void* allocateAndZero(Allocator* allocator, size_t size)
{
    void* result = allocator->allocate(size, 64);
    memset(result, 0, size);

    return result;
}

static void tryLoadString(json& jsonData, const char* key, StringBuffer& stringBuffer, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        return;
    }

    std::string value = jsonData.value(key, "");

    stringBuffer.init(value.length() + 1, allocator);
    stringBuffer.append(value.c_str());
}

static void tryLoadInt(json& jsonData, const char* key, int32_t& value)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        value = glTF::INVALID_INT_VALUE;
        return;
    }

    value = jsonData.value(key, 0);
}

static void tryLoadFloat(json& jsonData, const char* key, float& value)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        value = glTF::INVALID_FLOAT_VALUE;
        return;
    }

    value = jsonData.value(key, 0.f);
}

static void tryLoadBool(json& jsonData, const char* key, bool& value)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        value = false;
        return;
    }

    value = jsonData.value(key, false);
}

static void tryLoadType(json& jsonData, const char* key, glTF::Accessor::Type& type)
{
    std::string value = jsonData.value(key, "");
    if (value == "SCALAR")
    {
        type = glTF::Accessor::Scalar;
    }
    else if (value == "VEC2")
    {
        type = glTF::Accessor::Vec2;
    }
    else if (value == "VEC3")
    {
        type = glTF::Accessor::Vec3;
    }
    else if (value == "VEC4")
    {
        type = glTF::Accessor::Vec4;
    }
    else if (value == "MAT2")
    {
        type = glTF::Accessor::Mat2;
    }
    else if (value == "MAT3")
    {
        type = glTF::Accessor::Mat3;
    }
    else if (value == "MAT4")
    {
        type = glTF::Accessor::Mat4;
    }
    else
    {
        VOID_ASSERTM(false, "Type doesn't exist.");
    }
}

static void tryLoadIntArray(json& jsonData, const char* key, uint32_t& count, int32_t** array, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        count = 0;
        *array = nullptr;
        return;
    }

    json jsonArray = jsonData.at(key);

    count = jsonArray.size();

    int32_t* values = reinterpret_cast<int32_t*>(allocateAndZero(allocator, sizeof(int32_t) * count));

    for (size_t i = 0; i < count; ++i)
    {
        values[i] = jsonArray.at(i);
    }

    *array = values;
}

static void tryLoadFloatArray(json& jsonData, const char* key, uint32_t& count, float** array, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        count = 0;
        *array = nullptr;
        return;
    }

    json jsonArray = jsonData.at(key);

    count = jsonArray.size();

    float* values = reinterpret_cast<float*>(allocateAndZero(allocator, sizeof(float) * count));

    for (size_t i = 0; i < count; ++i)
    {
        values[i] = jsonArray.at(i);
    }

    *array = values;
}

static void loadAsset(json& jsonData, glTF::Asset& asset, Allocator* allocator)
{
    json jsonAsset = jsonData["asset"];

    tryLoadString(jsonAsset, "copyright", asset.copyright, allocator);
    tryLoadString(jsonAsset, "generator", asset.generator, allocator);
    tryLoadString(jsonAsset, "minVersion", asset.minVersion, allocator);
    tryLoadString(jsonAsset, "version", asset.version, allocator);
}

static void loadScene(json& jsonData, glTF::Scene& scene, Allocator* allocator)
{
    tryLoadIntArray(jsonData, "nodes", scene.nodesCount, &scene.nodes, allocator);
}

static void loadScenes(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json scenes = jsonData["scenes"];

    size_t sceneCount = scenes.size();
    gltfData.scenes = reinterpret_cast<glTF::Scene*>(allocateAndZero(allocator, sizeof(glTF::Scene) * sceneCount));
    gltfData.scenesCount = sceneCount;

    for (size_t i = 0; i < sceneCount; ++i)
    {
        loadScene(scenes[i], gltfData.scenes[i], allocator);
    }
}

static void loadBuffer(json& jsonData, glTF::Buffer& buffer, Allocator* allocator)
{
    tryLoadString(jsonData, "uri", buffer.uri, allocator);
    tryLoadInt(jsonData, "byteLength", buffer.byteLength);
    tryLoadString(jsonData, "name", buffer.name, allocator);
}

static void loadBuffers(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json buffers = jsonData["buffers"];

    size_t bufferCount = buffers.size();
    gltfData.buffers = reinterpret_cast<glTF::Buffer*>(allocateAndZero(allocator, sizeof(glTF::Buffer) * bufferCount));
    gltfData.buffersCount = bufferCount;

    for (size_t i = 0; i < bufferCount; ++i)
    {
        loadBuffer(buffers[i], gltfData.buffers[i], allocator);
    }
}

static void loadBufferView(json& jsonData, glTF::BufferView& bufferView, Allocator* allocator)
{
    tryLoadInt(jsonData, "buffer", bufferView.buffer);
    tryLoadInt(jsonData, "byteLength", bufferView.byteLength);
    tryLoadInt(jsonData, "byteOffset", bufferView.byteOffset);
    tryLoadInt(jsonData, "byteStride", bufferView.byteStride);
    tryLoadInt(jsonData, "target", bufferView.target);
    tryLoadString(jsonData, "name", bufferView.name, allocator);
}

static void loadBufferViews(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json buffers = jsonData["bufferViews"];

    size_t bufferCount = buffers.size();
    gltfData.bufferViews = reinterpret_cast<glTF::BufferView*>(allocateAndZero(allocator, sizeof(glTF::BufferView) * bufferCount));
    gltfData.bufferViewCount = bufferCount;

    for (size_t i = 0; i < bufferCount; ++i)
    {
        loadBufferView(buffers[i], gltfData.bufferViews[i], allocator);
    }
}

static void loadNode(json& jsonData, glTF::Node& node, Allocator* allocator)
{
    tryLoadInt(jsonData, "camera", node.camera);
    tryLoadInt(jsonData, "mesh", node.mesh);
    tryLoadInt(jsonData, "skin", node.skin);
    tryLoadIntArray(jsonData, "children", node.childrenCount, &node.children, allocator);
    tryLoadFloatArray(jsonData, "matrix", node.matrixCount, &node.matrix, allocator);
    tryLoadFloatArray(jsonData, "rotation", node.rotationCount, &node.rotation, allocator);
    tryLoadFloatArray(jsonData, "scale", node.scaleCount, &node.scale, allocator);
    tryLoadFloatArray(jsonData, "translation", node.translationCount, &node.translation, allocator);
    tryLoadFloatArray(jsonData, "weights", node.weightsCount, &node.weights, allocator);
    tryLoadString(jsonData, "name", node.name, allocator);
}

static void loadNodes(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json array = jsonData["nodes"];

    size_t arrayCount = array.size();
    gltfData.nodes = reinterpret_cast<glTF::Node*>(allocateAndZero(allocator, sizeof(glTF::Node) * arrayCount));
    gltfData.nodesCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadNode(array[i], gltfData.nodes[i], allocator);
    }
}

static void loadMeshPrimitive(json& jsonData, glTF::MeshPrimitive& meshPrimitive, Allocator* allocator)
{
    tryLoadInt(jsonData, "indices", meshPrimitive.indices);
    tryLoadInt(jsonData, "material", meshPrimitive.material);
    tryLoadInt(jsonData, "mode", meshPrimitive.mode);

    json attribute = jsonData["attributes"];

    meshPrimitive.attributes = reinterpret_cast<glTF::MeshPrimitive::Attribute*>(allocateAndZero(allocator, sizeof(glTF::MeshPrimitive::Attribute) * attribute.size()));
    meshPrimitive.attributeCount = attribute.size();

    uint32_t index = 0;
    for (auto jsonAttribute : attribute.items())
    {
        std::string key = jsonAttribute.key();
        glTF::MeshPrimitive::Attribute& attribute = meshPrimitive.attributes[index];

        attribute.key.init(key.size() + 1, allocator);
        attribute.key.append(key.c_str());

        attribute.accessorIndex = jsonAttribute.value();

        ++index;
    }
}

static void loadMeshPrimitives(json& jsonData, glTF::Mesh& mesh, Allocator* allocator)
{
    json array = jsonData["primitives"];

    size_t arrayCount = array.size();
    mesh.primitives = reinterpret_cast<glTF::MeshPrimitive*>(allocateAndZero(allocator, sizeof(glTF::MeshPrimitive) * arrayCount));
    mesh.primitiveCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadMeshPrimitive(array[i], mesh.primitives[i], allocator);
    }
}

static void loadMesh(json& jsonData, glTF::Mesh& mesh, Allocator* allocator)
{
    loadMeshPrimitives(jsonData, mesh, allocator);
    tryLoadFloatArray(jsonData, "weights", mesh.weightCount, &mesh.weights, allocator);
    tryLoadString(jsonData, "name", mesh.name, allocator);
}

static void loadMeshes(json& jsonData, glTF::glTF& glTFData, Allocator* allocator)
{
    json array = jsonData["meshes"];

    size_t arrayCount = array.size();
    glTFData.meshes = reinterpret_cast<glTF::Mesh*>(allocateAndZero(allocator, sizeof(glTF::Mesh) * arrayCount));
    glTFData.meshCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadMesh(array[i], glTFData.meshes[i], allocator);
    }
}

static void loadAccessor(json& jsonData, glTF::Accessor& accessor, Allocator* allocator)
{
    tryLoadInt(jsonData, "bufferView", accessor.bufferView);
    tryLoadInt(jsonData, "byteOffset", accessor.byteOffset);
    tryLoadInt(jsonData, "componentType", accessor.componentType);
    tryLoadInt(jsonData, "count", accessor.count);
    tryLoadInt(jsonData, "sparse", accessor.sparse);
    tryLoadFloatArray(jsonData, "max", accessor.maxCount, &accessor.max, allocator);
    tryLoadFloatArray(jsonData, "min", accessor.minCount, &accessor.min, allocator);
    tryLoadBool(jsonData, "normalized", accessor.normalised);
    tryLoadType(jsonData, "type", accessor.type);
}

static void loadAccessors(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json array = jsonData["accessors"];

    size_t arrayCount = array.size();
    gltfData.accessors = reinterpret_cast<glTF::Accessor*>(allocateAndZero(allocator, sizeof(glTF::Accessor) * arrayCount));
    gltfData.accessorsCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadAccessor(array[i], gltfData.accessors[i], allocator);
    }
}

static void tryLoadTextureInfo(json& jsonData, const char* key, glTF::TextureInfo** textureInfo, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        *textureInfo = nullptr;
        return;
    }

    glTF::TextureInfo* tInfo = reinterpret_cast<glTF::TextureInfo*>(allocator->allocate(sizeof(glTF::TextureInfo), 64));

    tryLoadInt(*it, "index", tInfo->index);
    tryLoadInt(*it, "texCoord", tInfo->texCoord);

    *textureInfo = tInfo;
}

static void tryLoadMaterialNormalTextureInfo(json& jsonData, const char* key,
    glTF::MaterialNormalTextureInfo** textureInfo, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        *textureInfo = nullptr;
        return;
    }

    glTF::MaterialNormalTextureInfo* tInfo = reinterpret_cast<glTF::MaterialNormalTextureInfo*>
        (allocator->allocate(sizeof(glTF::MaterialNormalTextureInfo), 64));

    tryLoadInt(*it, "index", tInfo->index);
    tryLoadInt(*it, "texCoord", tInfo->texCoord);
    tryLoadFloat(*it, "scale", tInfo->scale);

    *textureInfo = tInfo;
}

static void tryLoadMaterialOcclusionTextureInfo(json& jsonData, const char* key,
    glTF::MaterialOcclusionTextureInfo** textureInfo, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        *textureInfo = nullptr;
        return;
    }

    glTF::MaterialOcclusionTextureInfo* tInfo = reinterpret_cast<glTF::MaterialOcclusionTextureInfo*>
        (allocator->allocate(sizeof(glTF::MaterialOcclusionTextureInfo), 64));

    tryLoadInt(*it, "index", tInfo->index);
    tryLoadInt(*it, "texCoord", tInfo->texCoord);
    tryLoadFloat(*it, "strength", tInfo->strength);

    *textureInfo = tInfo;
}

static void tryLoadMaterialPBRMetallicRoughnessTextureInfo(json& jsonData, const char* key,
    glTF::MaterialPBRMetallicRoughness** textureInfo, Allocator* allocator)
{
    auto it = jsonData.find(key);
    if (it == jsonData.end())
    {
        *textureInfo = nullptr;
        return;
    }

    glTF::MaterialPBRMetallicRoughness* tInfo = reinterpret_cast<glTF::MaterialPBRMetallicRoughness*>
        (allocator->allocate(sizeof(glTF::MaterialPBRMetallicRoughness), 64));

    tryLoadFloatArray(*it, "baseColorFactor", tInfo->baseColourFactorCount, &tInfo->baseColourFactor, allocator);
    tryLoadTextureInfo(*it, "baseColorTexture", &tInfo->baseColourTexture, allocator);
    tryLoadFloat(*it, "metallicFactor", tInfo->metallicFactor);
    tryLoadTextureInfo(*it, "metallicRoughnessTexture", &tInfo->metallicRoughnessTexture, allocator);
    tryLoadFloat(*it, "roughnessFactor", tInfo->roughnessFactor);

    *textureInfo = tInfo;
}

static void loadMaterial(json& jsonData, glTF::Material& material, Allocator* allocator)
{
    tryLoadFloatArray(jsonData, "emissiveFactor", material.emissiveFactorCount, &material.emissiveFactor, allocator);
    tryLoadFloat(jsonData, "alphaCutoff", material.alphaCutOff);
    tryLoadString(jsonData, "alphaMode", material.alphaMode, allocator);
    tryLoadBool(jsonData, "doubleSided", material.doubleSided);

    tryLoadTextureInfo(jsonData, "emissiveTexture", &material.emissiveTexture, allocator);
    tryLoadMaterialNormalTextureInfo(jsonData, "normalTexture", &material.normalTexture, allocator);
    tryLoadMaterialOcclusionTextureInfo(jsonData, "occlusionTexture", &material.occlusionTexture, allocator);
    tryLoadMaterialPBRMetallicRoughnessTextureInfo(jsonData, "pbrMetallicRoughness", &material.pbrMetallicRoughness, allocator);

    tryLoadString(jsonData, "name", material.name, allocator);
}

static void loadMaterials(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json array = jsonData["materials"];

    size_t arrayCount = array.size();
    gltfData.materials = reinterpret_cast<glTF::Material*>(allocateAndZero(allocator, sizeof(glTF::Material) * arrayCount));
    gltfData.materialsCounts = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadMaterial(array[i], gltfData.materials[i], allocator);
    }
}

static void loadTexture(json& jsonData, glTF::Texture& texture, Allocator* allocator)
{
    tryLoadInt(jsonData, "sampler", texture.sampler);
    tryLoadInt(jsonData, "source", texture.source);
    tryLoadString(jsonData, "name", texture.name, allocator);
}

static void loadTextures(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json array = jsonData["textures"];

    size_t arrayCount = array.size();
    gltfData.textures = reinterpret_cast<glTF::Texture*>(allocateAndZero(allocator, sizeof(glTF::Texture) * arrayCount));
    gltfData.texturesCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadTexture(array[i], gltfData.textures[i], allocator);
    }
}

static void loadImage(json& jsonData, glTF::Image& image, Allocator* allocator)
{
    tryLoadInt(jsonData, "bufferView", image.bufferView);
    tryLoadString(jsonData, "mimeType", image.mineType, allocator);
    tryLoadString(jsonData, "uri", image.uri, allocator);
}

static void loadImages(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json array = jsonData["images"];

    size_t arrayCount = array.size();
    gltfData.images = reinterpret_cast<glTF::Image*>(allocateAndZero(allocator, sizeof(glTF::Image) * arrayCount));
    gltfData.imagesCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadImage(array[i], gltfData.images[i], allocator);
    }
}

static void loadSampler(json& jsonData, glTF::Sampler& sampler, Allocator* allocator)
{
    tryLoadInt(jsonData, "magFilter", sampler.magFilter);
    tryLoadInt(jsonData, "minFilter", sampler.minFilter);
    tryLoadInt(jsonData, "wrapS", sampler.wrapS);
    tryLoadInt(jsonData, "wrapT", sampler.wrapT);
}

static void loadSamplers(json& jsonData, glTF::glTF& gltfData, Allocator* allocator)
{
    json array = jsonData["samplers"];

    size_t arrayCount = array.size();
    gltfData.samplers = reinterpret_cast<glTF::Sampler*>(allocateAndZero(allocator, sizeof(glTF::Sampler) * arrayCount));
    gltfData.samplersCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i)
    {
        loadSampler(array[i], gltfData.samplers[i], allocator);
    }
}

static void loadSkin(json& jsonData, glTF::Skin& skin, Allocator* allocator)
{
    tryLoadInt(jsonData, "skeleton", skin.skeletonRootNodeIndex);
    tryLoadInt(jsonData, "inverseBindMatrices", skin.inverseBindMatricesBufferIndex);
    tryLoadIntArray(jsonData, "joints", skin.jointsCount, &skin.joints, allocator);
}

static void loadSkins(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) 
{
    json array = jsonData["skins"];

    size_t arrayCount = array.size();
    gltfData.skins = reinterpret_cast<glTF::Skin*>(allocateAndZero(allocator, sizeof(glTF::Skin) * arrayCount));
    gltfData.skinsCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i) 
    {
        loadSkin(array[i], gltfData.skins[i], allocator);
    }
}

static void loadAnimation(json& jsonData, glTF::Animation& animation, Allocator* allocator)
{
    json jsonArray = jsonData["samplers"];
    if (jsonArray.is_array())
    {
        size_t count = jsonArray.size();

        glTF::AnimationSampler* values = reinterpret_cast<glTF::AnimationSampler*>
            (allocateAndZero(allocator, sizeof(glTF::AnimationSampler) * count));

        for (size_t i = 0; i < count; ++i)
        {
            json element = jsonArray[i];
            glTF::AnimationSampler& sampler = values[i];

            tryLoadInt(element, "input", sampler.inputKeyframeBufferIndex);
            tryLoadInt(element, "output", sampler.outputKeyframeBufferIndex);

            std::string value = element.value("interpolation", "");
            if (value == "LINEAR")
            {
                sampler.interpolation = glTF::AnimationSampler::Linear;
            }
            else if (value == "STEP")
            {
                sampler.interpolation = glTF::AnimationSampler::Step;
            }
            else if (value == "CUBICSPLINE")
            {
                sampler.interpolation = glTF::AnimationSampler::CubicSpline;
            }
            else
            {
                sampler.interpolation = glTF::AnimationSampler::Linear;
            }
        }

        animation.sampler = values;
        animation.samplersCount = count;
    }

    jsonArray = jsonData["channels"];
    if (jsonArray.is_array())
    {
        size_t count = jsonArray.size();
        glTF::AnimationChannel* values = reinterpret_cast<glTF::AnimationChannel*>
            (allocateAndZero(allocator, sizeof(glTF::AnimationChannel) * count));

        for (size_t i = 0; i < count; ++i)
        {
            json element = jsonArray[i];
            glTF::AnimationChannel& channel = values[i];

            tryLoadInt(element, "sampler", channel.sampler);
            json target = element["target"];
            tryLoadInt(element, "node", channel.targetNode);

            std::string targetPath = target.value("path", "");
            if (targetPath == "scale")
            {
                channel.targetType = glTF::AnimationChannel::Scale;
            }
            else if (targetPath == "rotation")
            {
                channel.targetType = glTF::AnimationChannel::Rotation;
            }
            else if (targetPath == "translation")
            {
                channel.targetType = glTF::AnimationChannel::Translation;
            }
            else if (targetPath == "weights")
            {
                channel.targetType = glTF::AnimationChannel::Weights;
            }
            else
            {
                VOID_ASSERTM(false, "Could not pars target path %s\n", targetPath.c_str());
                channel.targetType = glTF::AnimationChannel::Count;
            }
        }

        animation.channels = values;
        animation.channelsCount = count;
    }
}

static void loadAnimations(json& jsonData, glTF::glTF& gltfData, Allocator* allocator) 
{
    json array = jsonData["animations"];

    size_t arrayCount = array.size();
    gltfData.animation = reinterpret_cast<glTF::Animation*>(allocateAndZero(allocator, sizeof(glTF::Animation) * arrayCount));
    gltfData.animationsCount = arrayCount;

    for (size_t i = 0; i < arrayCount; ++i) 
    {
        loadAnimation(array[i], gltfData.animation[i], allocator);
    }
}

int32_t glTF::getDataOffset(int32_t accessorOffset, int32_t bufferViewOffset)
{
    int32_t byteOffset = bufferViewOffset == INVALID_INT_VALUE ? 0 : bufferViewOffset;
    byteOffset += accessorOffset == INVALID_INT_VALUE ? 0 : accessorOffset;
    return byteOffset;
}

glTF::glTF gltfLoadFile(const char* filePath) 
{
    glTF::glTF result{};

    if (fileExists(filePath) == false) 
    {
        //TODO: Might not be an assert situation
        VOID_ASSERTM(false, "Could not find file %s", filePath);
        return result;
    }

    HeapAllocator* heapAllocator = &MemoryService::instance()->systemAllocator;

    FileReadResult readResult = fileReadText(filePath, heapAllocator);

    json gltfData = json::parse(readResult.data);
    result.allocator.init(void_mega(2));
    Allocator* allocator = &result.allocator;

    for (auto properties : gltfData.items()) 
    {
        if (properties.key() == "asset")
        {
            loadAsset(gltfData, result.asset, allocator);
        }
        else if (properties.key() == "scene")
        {
            tryLoadInt(gltfData, "scene", result.scene);
        }
        else if (properties.key() == "scenes")
        {
            loadScenes(gltfData, result, allocator);
        }
        else if (properties.key() == "buffers")
        {
            loadBuffers(gltfData, result, allocator);
        }
        else if (properties.key() == "bufferViews")
        {
            loadBufferViews(gltfData, result, allocator);
        }
        else if (properties.key() == "nodes")
        {
            loadNodes(gltfData, result, allocator);
        }
        else if (properties.key() == "meshes")
        {
            loadMeshes(gltfData, result, allocator);
        }
        else if (properties.key() == "accessors")
        {
            loadAccessors(gltfData, result, allocator);
        }
        else if (properties.key() == "materials")
        {
            loadMaterials(gltfData, result, allocator);
        }
        else if (properties.key() == "textures")
        {
            loadTextures(gltfData, result, allocator);
        }
        else if (properties.key() == "images")
        {
            loadImages(gltfData, result, allocator);
        }
        else if (properties.key() == "samplers")
        {
            loadSamplers(gltfData, result, allocator);
        }
        else if (properties.key() == "skins")
        {
            loadSkins(gltfData, result, allocator);
        }
        else if (properties.key() == "animations")
        {
            loadAnimations(gltfData, result, allocator);
        }
    }

    heapAllocator->deallocate(readResult.data);

    return result;
}

void gltfFree(glTF::glTF& scene) 
{
    scene.allocator.shutdown();
}

int32_t gltfGetAttributeAccessorIndex(glTF::MeshPrimitive::Attribute* attributes, uint32_t atributeCount, const char* attributeName) 
{
    for (uint32_t index = 0; index < atributeCount; ++index) 
    {
        glTF::MeshPrimitive::Attribute& attribute = attributes[index];
        if (strcmp(attribute.key.data, attributeName) == 0)
        {
            return attribute.accessorIndex;
        }
    }

    return -1;
}