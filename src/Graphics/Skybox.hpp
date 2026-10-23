#ifndef SKYBOX_HDR
#define SKYBOX_HDR

#include "GPUDevice.hpp"
#include "CommandBuffer.hpp"

#include "ShaderData.hpp"

void initSkybox(GPUDevice& gpu);
void drawSkybox(GPUDevice& gpu, CommandBuffer& gpuCommands, const SkyboxPushConstants& pushConstants);
void shutdownSkybox(GPUDevice& gpu);

#endif // !SKYBOX_HDR
