#ifndef MAIN_MENU_HDR
#define MAIN_MENU_HDR

#include "Application/Audio.hpp"
#include "Application/UserInterface.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/VoidImgui.hpp"
#include "Graphics/2DRenderer.hpp"
#include "Graphics/GPUProfiler.hpp"

struct MainMenu 
{
    void init(GPUDevice& inGPU, AudioSystem& inAudioSystem, ImguiService& inImgui);
    void loop(InputHandler& inputHandler, [[maybe_unused]] GPUProfiler& gpuProfiler);
    void shutdown();

    GPUDevice* gpu;
    AudioSystem* audioSystem;
    ImguiService* imgui;

    Renderer2D renderer2D;
    GUI userInterface;

    int64_t beginFrameTick;
};

#endif // !MAIN_MENU_HDR
