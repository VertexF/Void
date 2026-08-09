#include "Textures.hpp"

#include "GPUDevice.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <vender/stb_image.h>
#include <ktx.h>
#include <ktxvulkan.h>

TextureHandle loadTexture(GPUDevice& gpu, const char* filePath, uint32_t* outWidth, uint32_t* outHeight)
{
    TextureHandle textureResource;

    int comp;
    int width;
    int height;
    uint8_t mipLevels = 1;

    uint8_t* imageData = stbi_load(filePath, &width, &height, &comp, 4);
    if (imageData == nullptr)
    {
        textureResource = INVALID_TEXTURE;
        VOID_ERROR("Error loading texture %s", filePath);
    }

    // TODO: Add mipmap support later.
    uint32_t w = width;
    uint32_t h = height;

    while (w > 1 && h > 1)
    {
        w /= 2;
        h /= 2;

        ++mipLevels;
    }

    uint32_t size = static_cast<uint16_t>(width) * static_cast<uint16_t>(height) * 4;

    TextureCreation creation;
    creation.setData(imageData)
        .setFormatType(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1, size)
        .setName(filePath);

    TextureHandle newTexture = gpu.createTexture(creation);

    VOID_ASSERT(newTexture.index != INVALID_TEXTURE.index);

    stbi_image_free(imageData);

    return newTexture;
}

TextureHandle loadTexture(GPUDevice& gpu, uint8_t* bufferData, size_t size, uint32_t* outWidth, uint32_t* outHeight)
{
    int comp = 0;
    int width = 0;
    int height = 0;
    uint8_t mipLevels = 1;
    stbi_info_from_memory(bufferData, (int)size, &width, &height, &comp);

    int x;
    int y;
    uint8_t* textureData = stbi_load_from_memory(bufferData, (int)size, &x, &y, &comp, 4);

    uint32_t imageSize = static_cast<uint16_t>(width) * static_cast<uint16_t>(height) * 4;

    if (outWidth)
        *outWidth = width;
    if (outHeight)
        *outHeight = height;

    TextureCreation textureCreation{};
    textureCreation.setFormatType(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1, imageSize)
        .setData(textureData)
        .setFlags(mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setName(nullptr);

    TextureHandle newTexture = gpu.createTexture(textureCreation);
    VOID_ASSERT(newTexture.index != INVALID_TEXTURE.index);

    stbi_image_free(textureData);

    return newTexture;
}

TextureHandle loadTexture(GPUDevice& gpu, const char* filepath, ktx_transcode_fmt_e formatSelected, uint32_t* outWidth, uint32_t* outHeight)
{
    TextureHandle textureResource;

    ktxTexture2* kTexture = nullptr;
    uint8_t mipLevels = 1;
    KTX_error_code code = ktxTexture2_CreateFromNamedFile(filepath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
    if (code != KTX_SUCCESS)
    {
        textureResource = INVALID_TEXTURE;
        VOID_ERROR("Error loading texture %s", filepath);
    }

    if(outWidth)
        *outWidth = kTexture->baseWidth;
    if(outHeight)
        *outHeight = kTexture->baseHeight;

    if (ktxTexture2_NeedsTranscoding(kTexture))
    {
        code = ktxTexture2_TranscodeBasis(kTexture, formatSelected, 0);
        if (code != KTX_SUCCESS)
        {
            VOID_ERROR("Could not transcode the input texture to the selected target format.");
        }
    }

    TextureCreation creation;
    creation.setData(kTexture->pData)
        .setFormatType(ktxTexture2_GetVkFormat(kTexture), VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(kTexture->baseWidth), static_cast<uint16_t>(kTexture->baseHeight), 1, (uint32_t)kTexture->dataSize)
        .setName(filepath);

    TextureHandle newTexture = gpu.createTexture(creation);

    VOID_ASSERT(newTexture.index != INVALID_TEXTURE.index);

    ktxTexture2_Destroy(kTexture);

    return newTexture;
}

TextureHandle loadTexture(GPUDevice& gpu, uint8_t* bufferData, size_t size, ktx_transcode_fmt_e selectedFormat, uint32_t* outWidth, uint32_t* outHeight)
{
    TextureHandle textureResource;

    ktxTexture2* kTexture = nullptr;

    KTX_error_code code = ktxTexture2_CreateFromMemory(bufferData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
    if (code != KTX_SUCCESS)
    {
        textureResource = INVALID_TEXTURE;
        VOID_ERROR("Error loading ktx memory based textures.");
    }

    uint32_t width = kTexture->baseWidth;
    uint32_t height = kTexture->baseHeight;

    //If transcoding is too slow with the ETC1S compression type use compression type KTX_TTF_ETC2_RGBA if supported.
    //OR look into making faster a different way.
    if (ktxTexture2_NeedsTranscoding(kTexture))
    {
        code = ktxTexture2_TranscodeBasis(kTexture, selectedFormat, 0);
        if (code != KTX_SUCCESS)
        {
            VOID_ERROR("Could not transcode the input texture to the selected target format.");
        }
    }

    TextureCreation creation;
    creation.setData(kTexture->pData)
        .setFormatType(ktxTexture2_GetVkFormat(kTexture), VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(kTexture->numLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1, (uint32_t)kTexture->dataSize)
        .setName(nullptr);

    TextureHandle newTexture = gpu.createTexture(creation, kTexture);

    VOID_ASSERT(newTexture.index != INVALID_TEXTURE.index);

    ktxTexture2_Destroy(kTexture);

    return newTexture;
}

TextureHandle createACubemap(GPUDevice& gpu, const Array<const char*>& images, const char* name)
{
    Array<uint8_t*> skyboxImageArray;
    skyboxImageArray.init(&MemoryService::instance()->scratchAllocator, 6);

    int comp;
    int width;
    int height;

    for (uint32_t i = 0; i < images.size; ++i)
    {
        if (images[i])
        {
            //stbi_set_flip_vertically_on_load(1);
            //Load 6 images.
            uint8_t* imageData = stbi_load(images[i], &width, &height, &comp, 4);
            if (imageData == nullptr)
            {
                VOID_ERROR("Error loading texture %s", images[i]);
                return INVALID_TEXTURE;
            }

            skyboxImageArray.push(imageData);
        }
    }

    uint32_t size = static_cast<uint16_t>(width) * static_cast<uint16_t>(height) * 4 * images.size;

    //Create the single texture.
    TextureCreation creation{};
    creation.setFormatType(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE)
        .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(width), 1, size)
        .setImages(skyboxImageArray, skyboxImageArray.size)
        .setName(name);

    TextureHandle newTexture = gpu.createTexture(creation);

    for (uint32_t i = 0; i < skyboxImageArray.size; ++i)
    {
        stbi_image_free(skyboxImageArray[i]);
    }

    return newTexture;
}

TextureHandle createACubemap(GPUDevice& gpu, const char* images)
{
    TextureHandle textureResource;

    ktxTexture2* kTexture = nullptr;

    KTX_error_code code = ktxTexture2_CreateFromNamedFile(images, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
    if (code != KTX_SUCCESS)
    {
        textureResource = INVALID_TEXTURE;
        VOID_ERROR("Error loading texture %s", images);
    }

    //If transcoding is too slow with the ETC1S compression type use compression type KTX_TTF_ETC2_RGBA if supported.
    //OR look into making faster a different way.
    ktx_transcode_fmt_e formatSelected = KTX_TTF_BC7_RGBA;

    if (ktxTexture2_NeedsTranscoding(kTexture))
    {
        code = ktxTexture2_TranscodeBasis(kTexture, formatSelected, 0);
        if (code != KTX_SUCCESS)
        {
            VOID_ERROR("Could not transcode the input texture to the selected target format.");
        }
    }

    TextureCreation creation;
    creation.setData(kTexture->pData)
        .setFormatType(ktxTexture2_GetVkFormat(kTexture), VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE)
        .setFlags((uint8_t)kTexture->numLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(kTexture->baseWidth), static_cast<uint16_t>(kTexture->baseHeight), 1, (uint32_t)kTexture->dataSize)
        .setName(images);
    creation.layerCount = 6;

    TextureHandle newTexture = gpu.createTexture(creation, kTexture);

    VOID_ASSERT(newTexture.index != INVALID_TEXTURE.index);

    ktxTexture2_Destroy(kTexture);

    return newTexture;
}