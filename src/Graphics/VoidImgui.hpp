#ifndef AIR_IMGUI_HDR
#define AIR_IMGUI_HDR

#include "Foundation/Platform.hpp"

struct ImFont;

struct GPUDevice;
struct CommandBuffer;
struct TextureHandle;

enum ImguiStyles
{
    DEFAULT,
    GREEN_BLUE,
    DARK_RED,
    DARK_GOLD
};//ImguiStyles

struct ImguiServiceConfiguration
{
    GPUDevice* gpu;
    void* windowHandle;
};//ImguiServiceConfiguration

struct ImguiService
{
    static ImguiService* instance();

    void init(void* configuration);
    void shutdown();

    void newFrame();
    void render(CommandBuffer& commands);

    void setStyle(ImguiStyles style);

    GPUDevice* gpu;
    ImFont* font;
};

#endif // !AIR_IMGUI_HDR
