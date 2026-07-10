#ifndef GAME_HDR
#define GAME_HDR

#include "Application/Input.hpp"
#include "Application/UserInterface.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/GPUProfiler.hpp"
#include "Graphics/VoidImgui.hpp"
#include "Graphics/2DRenderer.hpp"

#include "Game/Player.hpp"
#include "Game/Scene.hpp"

struct Game
{
    void init(GPUDevice& inGPU, AudioSystem& inAudioSystem, ImguiService& inImgui);
    void loop(InputHandler& inputHandler, [[maybe_unused]] GPUProfiler& gpuProfiler);
    void shutdown();

    void setupDrawCalls();
    void runParticleCompute(CommandBuffer* commandBuffer);
    void deleteEntity();

    GPUDevice* gpu;
    AudioSystem* audioSystem;
    ImguiService* imgui;

    Renderer2D renderer2D;
    GameCamera gameCamera;
    Scene scene;
    GUI userInterface;

    vec3s eye = vec3s{ 0.f, 1.f, 0.f };
    vec3s playerPosition{};

    int64_t beginFrameTick;

    float modelScale = 1.0f;

    uint32_t element = 0;

    bool recreatePositionBuffer = false;
    bool debugRenderer = true;
};

#endif // !GAME_HDR
