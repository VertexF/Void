#include "MainMenu.hpp"

#include "Foundation/Time.hpp"

void MainMenu::init(GPUDevice& inGPU, AudioSystem& inAudioSystem, ImguiService& inImgui)
{
    gpu = &inGPU;
    audioSystem = &inAudioSystem;
    imgui = &inImgui;
    beginFrameTick = timeNow();

    renderer2D.init(*gpu);
    renderer2D.loadTexture("Assets/Textures/mainMenuUI.png");
    userInterface.init(renderer2D);
    userInterface.buildMainMenu();
    renderer2D.loadBuffer(UI_FLAG_2D);
}

void MainMenu::loop(InputHandler& inputHandler, [[maybe_unused]] GPUProfiler& gpuProfiler)
{
    while (Window::instance()->mainMenuRequested == false)
    {
        //ZoneScoped;
        inputHandler.onEvent(gpu, &userInterface);
        if (inputHandler.isKeyDown(Keys::KEY_ESCAPE))
        {
            Window::instance()->mainMenuRequested = true;
            Window::instance()->exitRequested = true;
        }
        else if (inputHandler.isKeyJustReleased(Keys::KEY_F))
        {
            Window::instance()->setFullscreen();
        }

        //New Frame
        if (Window::instance()->minimised == false)
        {
            //This is only false when we can't recreate the swapchain because of 0 height due to VK_ERROR_OUT_OF_DATE_KHR constantly being hit.
            //We still need to acquire an image to re-check if can now correctly fetch a swapchain image. 
            if (gpu->newFrame() == false)
            {
                continue;
            }

            if (Window::instance()->resizeRequested)
            {
                Window::instance()->resizeRequested = false;

                gpu->resize(Window::instance()->width, Window::instance()->height);
            }

            if (isUIPressed(EXIT_BUTTON, inputHandler))
            {
                Window::instance()->mainMenuRequested = true;
                Window::instance()->exitRequested = true;
            }

            if (isUIPressed(START_BUTTON, inputHandler))
            {
                Window::instance()->mainMenuRequested = true;
            }

            ////NOTE: This must be after the OS messages.
            //imgui->newFrame();

            //if (ImGui::Begin("Void ImGui"))
            //{
            //    ImGui::InputFloat("Model Scale", &modelScale, 0.001f);
            //}
            //ImGui::End();

            //if (ImGui::Begin("GPU"))
            //{
            //    gpuProfiler.imguiDraw();
            //}
            //ImGui::End();

            //Moves key pressed events stores then in a key-pressed array. This allows us to know if a key is being held down, rather than just pressed. 
            inputHandler.newFrame();
            //Saves the mouse position in screen coordinates and handles events that are for re-mapped key bindings 
            inputHandler.update();

            //Current use of delta time for physics isn't used in the menu yet,
            //const int64_t currentTick = timeNow();
            //float deltaTime = static_cast<float>(timeDeltaSeconds(beginFrameTick, currentTick));
            //beginFrameTick = currentTick;

            CommandBuffer* gpuCommands = gpu->getCommandBuffer(VK_QUEUE_GRAPHICS_BIT, true);
            gpuCommands->pushMarker("Frame");

            gpu->beginRenderingTransition(gpuCommands);
            gpuCommands->beginRendering();

            gpuCommands->setScissor(nullptr);
            gpuCommands->setViewport(nullptr);

            renderer2D.drawQuad(*gpuCommands);

            //imgui->render(*gpuCommands);

            gpuCommands->popMarker();

            //gpuProfiler.update(gpu);

            gpu->queueCommandBuffer(gpuCommands);
            gpu->present();
        }
        else
        {
            //ImGui::Render();
        }

        //FrameMark;
    }
}

void MainMenu::shutdown()
{
    //vkDeviceWaitIdle(gpu->vulkanDevice);

    renderer2D.shutdown();
}
