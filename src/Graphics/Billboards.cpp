#include "Billboards.hpp"

#include "vender/stb_image.h"

void Billboard::init(GPUDevice& inGPU) 
{

}

void Billboard::loadTexture(TextureAtlas atlas) 
{
    TextureHandle textureResource;

    int comp;
    uint8_t mipLevels = 1;

    stbi_set_flip_vertically_on_load(1);
    uint8_t* imageData = stbi_load(sAtlasPaths[atlas], &width, &height, &comp, 4);
    if (imageData == nullptr)
    {
        textureResource = INVALID_TEXTURE;
        VOID_ERROR("Error loading texture %s", sAtlasPaths[atlas]);
    }

    TextureCreation creation;
    creation.setData(imageData)
        .setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setFlags(mipLevels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setSize(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1)
        .setName(sAtlasPaths[atlas]);

    sTextureAlasHandles[atlas] = gpu->createTexture(creation).index;

    VOID_ASSERT(sTextureAlasHandles[atlas] != INVALID_TEXTURE.index);

    free(imageData);
}

void Billboard::loadBuffer() 
{
}

void Billboard::drawQuad(CommandBuffer& commandBuffer) 
{
}

void Billboard::shutdown()
{
}