#ifndef WINDOW_HDR
#define WINDOW_HDR

#include "Foundation/Array.hpp"

struct SDL_Window;

struct Window
{
    static Window* instance();

    void init(uint32_t inWidth, uint32_t inHeight, const char* title);
    void shutdown();

    void setFullscreen(bool fullscreen);
    void centerMouse(bool dragging) const;

    void setRefreshRate();

    SDL_Window* platformHandle = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    float displayRefresh = 1.f / 60.f;
    bool exitRequested = false;
    bool resizeRequested = false;
    bool minimised = false;

private:
    Window() = default;
    Window(Window const& rhs) = delete;
    Window(Window&& rhs) = delete;
    void operator=(Window const& rhs) = delete;
    Window operator=(Window&& rhs) = delete;
};

#endif // !WINDOW_HDR
