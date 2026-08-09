#ifndef TEXTURES_HDR
#define TEXTURES_HDR

#include "GPUResources.hpp"

struct GPUDevice;
enum ktx_transcode_fmt_e;

TextureHandle loadTexture(GPUDevice& gpu, const char* filePath, uint32_t* width = nullptr, uint32_t* height = nullptr);
TextureHandle loadTexture(GPUDevice& gpu, uint8_t* bufferData, size_t size, uint32_t* width = nullptr, uint32_t* height = nullptr);

TextureHandle loadTexture(GPUDevice& gpu, const char* filepath, ktx_transcode_fmt_e formatSelected, uint32_t* width = nullptr, uint32_t* height = nullptr);
TextureHandle loadTexture(GPUDevice& gpu, uint8_t* bufferData, size_t size, ktx_transcode_fmt_e selectedFormat, uint32_t* width = nullptr, uint32_t* height = nullptr);

TextureHandle createACubemap(GPUDevice& gpu, const Array<const char*>& images, const char* name);
TextureHandle createACubemap(GPUDevice& gpu, const char* images);

#endif // !TEXTURES_HDR
