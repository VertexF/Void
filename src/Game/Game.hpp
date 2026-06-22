#ifndef GAME_HDR
#define GAME_HDR

#include "Application/Input.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/GPUProfiler.hpp"
#include "Graphics/VoidImgui.hpp"

#include "Game/Player.hpp"
#include "Game/Scene.hpp"

struct Game
{
    void init();
    void loop();
    void shutdown();

    void deleteEntity();

    GPUDevice gpu{};
    InputHandler inputHandler{};
    GameCamera gameCamera;
    Scene scene;
    GPUProfiler gpuProfiler;
    AudioSystem audioSystem;
    vec3s eye = vec3s{ 0.f, 1.f, 0.f };
    vec3s playerPosition{};

    ImguiService* imgui;

    int64_t beginFrameTick;

    float modelScale = 1.0f;

    uint32_t element = 0;

    bool fullscreen = false;
    bool recreatePositionBuffer = false;
    bool debugRenderer = true;
};

#endif // !GAME_HDR
