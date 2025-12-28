#ifndef VOID_IMGUI_HDR
#define VOID_IMGUI_HDR

#include "Foundation/Platform.hpp"

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
    virtual ~ImguiService() = default;

    static ImguiService* instance();

    void init(void* configuration);
    void shutdown();

    void newFrame();
    void render(CommandBuffer& commands);

    //Removes the Texture from the cache and destroy the associated descriptor set.
    void removeCachedTexture(TextureHandle& texture);

    void setStyle(ImguiStyles style);

    GPUDevice* gpu;

    static constexpr const char* NAME = "Air_Imgui_Service";
};

//Application log
static void imguiPrint(const char* text);
static void imguiLogInit();
static void imguiLogShutdown();

static void imguiLogDraw();

//FPS graph
static void imguiFPSInit();
static void imguiFPSShutdown();
static void imguiFPSAdd(float deltaTime);
static void imguiFPSDraw();

#endif // !VOID_IMGUI_HDR
