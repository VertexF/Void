#include "Window.hpp"

#include "Foundation/Log.hpp"
#include "Foundation/Numerics.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdlib>

static SDL_Window* sdlWindow = nullptr;

static float SDLGetMonitorRefresh() 
{
    const SDL_DisplayMode* current = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    return 1.f / current->refresh_rate;
}

Window* Window::instance()
{
    static Window window;
    return &window;
}

void Window::init(uint32_t inWidth, uint32_t inHeight, const char* title)
{
    if (SDL_Init(SDL_INIT_VIDEO) == false)
    {
        VOID_ASSERTM(false, "SDL Init error : %s\n", SDL_GetError());
        return;
    }

    sdlWindow = SDL_CreateWindow(title, inWidth, inHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!sdlWindow)
    {
        VOID_ASSERTM(false, "SDL failed to create window : %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    int windowWidth, windowHeigth;
    SDL_GetWindowSizeInPixels(sdlWindow, &windowWidth, &windowHeigth);

    width = static_cast<uint16_t>(windowWidth);
    height = static_cast<uint16_t>(windowHeigth);

    //Assing this os it can be accessed from outside.
    platformHandle = sdlWindow;
    SDL_ShowWindow(sdlWindow);

    displayRefresh = SDLGetMonitorRefresh();
}

void Window::shutdown() 
{
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
}

void Window::setFullscreen(bool fullscreen)
{
    SDL_SetWindowFullscreen(sdlWindow, fullscreen);
}

void Window::centerMouse(bool dragging) const
{
    if (dragging) 
    {
        SDL_WarpMouseInWindow(sdlWindow, (float)roundU32(width / 2.f), (float)roundU32(height / 2.f));
        SDL_SetWindowRelativeMouseMode(sdlWindow, true);
        #if defined(__WIN32)
        SDL_HideCursor();
        #endif
    }
    else 
    {
        SDL_SetWindowRelativeMouseMode(sdlWindow, false);
        SDL_ShowCursor();
    }
}

void Window::setRefreshRate()
{
    displayRefresh = SDLGetMonitorRefresh();
}
