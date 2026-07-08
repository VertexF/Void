#include "Foundation/Memory.hpp"
#include "Foundation/Time.hpp"

#include "Application/Input.hpp"
#include "Application/Audio.hpp"

#include "Graphics/GPUDevice.hpp"
#include "Graphics/GPUProfiler.hpp"
#include "Graphics/2DRenderer.hpp"

#include "vender/imgui/imgui.h"

#include "Game/Game.hpp"
#include "Game/MainMenu.hpp"

int main(int argc, char** argv)
{
    //Init services
    MemoryService::instance()->init(/*heapSize=*/ void_giga(1ull), /*stackSize=*/ void_mega(8), /*physicsStackSiz=*/ void_mega(40));
    timeServiceInit();

    HeapAllocator* allocator = &MemoryService::instance()->systemAllocator;
    StackAllocator scratchAllocator = MemoryService::instance()->scratchAllocator;

    Window::instance()->init(1280, 800, "Void Engine");

    InputHandler inputHandler;
    inputHandler.init(allocator);

    DeviceCreation deviceCreation;
    deviceCreation.setWindow(Window::instance()->width, Window::instance()->height, Window::instance()->platformHandle)
        .setAllocator(allocator)
        .setLinearAllocator(&scratchAllocator);

    GPUDevice gpu;
    gpu.init(deviceCreation);

    GPUProfiler gpuProfiler;
    gpuProfiler.init(allocator, 100);

    ImguiService* imgui;
    imgui = ImguiService::instance();
    ImguiServiceConfiguration imguiConfig = { &gpu, Window::instance()->platformHandle };
    imgui->init(&imguiConfig);

    AudioSystem audioSystem;
    audioSystem.init();
    audioSystem.loadAudio();

    //Window::instance()->setFullscreen(true);

    //MainMenu mainMenu;
    //mainMenu.init(gpu, audioSystem, *imgui);
    //mainMenu.loop(inputHandler, gpuProfiler);
    //mainMenu.shutdown();

    //if (Window::instance()->exitRequested == false)
    //{
        Game game;
        game.init(gpu, audioSystem, *imgui);
        game.loop(inputHandler, gpuProfiler);
        game.shutdown();
    //}

    audioSystem.shutdown();

    imgui->shutdown();

    gpuProfiler.shutdown();

    gpu.shutdown();

    inputHandler.shutdown();
    Window::instance()->shutdown();

    MemoryService::instance()->shutdown();

    return 0;
}