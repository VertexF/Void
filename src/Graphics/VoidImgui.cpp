#include "VoidImgui.hpp"

#include "Foundation/HashMap.hpp"
#include "Foundation/Memory.hpp"
#include "Foundation/File.hpp"

#include "GPUDevice.hpp"
#include "CommandBuffer.hpp"

#include <vender/imgui/imgui.h>
#include <vender/imgui/backends/imgui_impl_sdl3.h>

#include <stdio.h>

namespace
{
    TextureHandle FONT_TEXTURE;
    PipelineHandle IMGUI_PIPELINE;
    BufferHandle VB;
    BufferHandle IB;
    BufferHandle UI_CB;
    DescriptorSetLayoutHandle sDescriptorSetLayout;
    DescriptorSetHandle UI_DESCRIPTOR_SET;

    uint32_t VB_SIZE = 665536;
    uint32_t IB_SIZE = 665536;

    FlatHashMap<uint32_t, uint32_t> TEXTURE_TO_DESCRIPTOR_SET;

    void setStyleDarkGold()
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        ImVec4* colours = style->Colors;

        colours[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        colours[ImGuiCol_TextDisabled] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
        colours[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colours[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colours[ImGuiCol_Border] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colours[ImGuiCol_FrameBgHovered] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colours[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colours[ImGuiCol_TitleBg] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colours[ImGuiCol_TitleBgActive] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colours[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colours[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.53f);
        colours[ImGuiCol_ScrollbarGrab] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.81f, 0.83f, 0.81f, 1.00f);
        colours[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colours[ImGuiCol_SliderGrab] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_SliderGrabActive] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_Button] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colours[ImGuiCol_ButtonHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colours[ImGuiCol_Header] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colours[ImGuiCol_HeaderHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_HeaderActive] = ImVec4(0.93f, 0.65f, 0.14f, 1.00f);
        colours[ImGuiCol_Separator] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        colours[ImGuiCol_SeparatorHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_SeparatorActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colours[ImGuiCol_ResizeGrip] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        colours[ImGuiCol_ResizeGripHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_ResizeGripActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colours[ImGuiCol_Tab] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
        colours[ImGuiCol_TabHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
        colours[ImGuiCol_TabActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
        colours[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colours[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
        colours[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colours[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colours[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colours[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colours[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colours[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colours[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colours[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colours[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colours[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        style->FramePadding = ImVec2(4, 2);
        style->ItemSpacing = ImVec2(10, 2);
        style->IndentSpacing = 12;
        style->ScrollbarSize = 10;

        style->WindowRounding = 4;
        style->FrameRounding = 4;
        style->PopupRounding = 4;
        style->ScrollbarRounding = 6;
        style->GrabRounding = 4;
        style->TabRounding = 4;

        style->WindowTitleAlign = ImVec2(1.f, 0.5f);
        style->WindowMenuButtonPosition = ImGuiDir_Right;

        style->DisplaySafeAreaPadding = ImVec2(4, 4);
    }

    void setStyleGreenBlue()
    {
        ImVec4* colours = ImGui::GetStyle().Colors;

        colours[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colours[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colours[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colours[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colours[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.44f, 0.44f, 0.60f);
        colours[ImGuiCol_FrameBgHovered] = ImVec4(0.57f, 0.57f, 0.57f, 0.70f);
        colours[ImGuiCol_FrameBgActive] = ImVec4(0.76f, 0.76f, 0.76f, 0.80f);
        colours[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colours[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colours[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
        colours[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colours[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colours[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colours[ImGuiCol_CheckMark] = ImVec4(0.13f, 0.75f, 0.55f, 0.80f);
        colours[ImGuiCol_SliderGrab] = ImVec4(0.13f, 0.75f, 0.75f, 0.80f);
        colours[ImGuiCol_SliderGrabActive] = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colours[ImGuiCol_Button] = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colours[ImGuiCol_ButtonHovered] = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colours[ImGuiCol_ButtonActive] = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colours[ImGuiCol_Header] = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colours[ImGuiCol_HeaderHovered] = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colours[ImGuiCol_HeaderActive] = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colours[ImGuiCol_Separator] = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colours[ImGuiCol_SeparatorHovered] = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colours[ImGuiCol_SeparatorActive] = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colours[ImGuiCol_ResizeGrip] = ImVec4(0.13f, 0.75f, 0.55f, 0.40f);
        colours[ImGuiCol_ResizeGripHovered] = ImVec4(0.13f, 0.75f, 0.75f, 0.60f);
        colours[ImGuiCol_ResizeGripActive] = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colours[ImGuiCol_Tab] = ImVec4(0.13f, 0.75f, 0.55f, 0.80f);
        colours[ImGuiCol_TabHovered] = ImVec4(0.13f, 0.75f, 0.75f, 0.80f);
        colours[ImGuiCol_TabActive] = ImVec4(0.13f, 0.75f, 1.00f, 0.80f);
        colours[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colours[ImGuiCol_TabUnfocusedActive] = ImVec4(0.36f, 0.36f, 0.36f, 0.54f);
#if defined(IMGUI_HAS_DOCK)
        colours[ImGuiCol_DockingPreview] = ImVec4(0.13f, 0.75f, 0.55f, 0.80f);
        colours[ImGuiCol_DockingEmptyBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.80f);
#endif // IMGUI_HAS_DOCK
        colours[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colours[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colours[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colours[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
#if defined (IMGUI_HAS_TABLE)
        colours[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colours[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colours[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colours[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
#endif // IMGUI_HAS_TABLE
        colours[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colours[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colours[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colours[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colours[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colours[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }

    void setStyleDarkRed()
    {
        ImVec4* colours = ImGui::GetStyle().Colors;
        colours[ImGuiCol_Text] = ImVec4(0.75f, 0.75f, 0.75f, 1.f);
        colours[ImGuiCol_TextDisabled] = ImVec4(0.35f, 0.35f, 0.35f, 1.f);
        colours[ImGuiCol_WindowBg] = ImVec4(0.f, 0.f, 0.f, 0.94f);
        colours[ImGuiCol_ChildBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        colours[ImGuiCol_PopupBg] = ImVec4(0.f, 0.f, 0.f, 0.94f);
        colours[ImGuiCol_Border] = ImVec4(0.f, 0.f, 0.f, 0.5f);
        colours[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);
        colours[ImGuiCol_FrameBg] = ImVec4(0.f, 0.f, 0.f, 0.54f);
        colours[ImGuiCol_FrameBgHovered] = ImVec4(0.37f, 0.14f, 0.14f, 0.67f);
        colours[ImGuiCol_FrameBgActive] = ImVec4(0.39f, 0.2f, 0.2f, 0.67f);

        colours[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colours[ImGuiCol_TitleBgActive] = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
        colours[ImGuiCol_TitleBgCollapsed] = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
        colours[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colours[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colours[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colours[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colours[ImGuiCol_CheckMark] = ImVec4(0.56f, 0.10f, 0.10f, 1.00f);
        colours[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.19f, 0.19f, 0.40f);

        colours[ImGuiCol_SliderGrabActive] = ImVec4(0.89f, 0.00f, 0.19f, 1.00f);
        colours[ImGuiCol_Button] = ImVec4(1.00f, 0.19f, 0.19f, 0.40f);
        colours[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.17f, 0.00f, 1.00f);
        colours[ImGuiCol_ButtonActive] = ImVec4(0.89f, 0.00f, 0.19f, 1.00f);
        colours[ImGuiCol_Header] = ImVec4(0.33f, 0.35f, 0.36f, 0.53f);
        colours[ImGuiCol_HeaderHovered] = ImVec4(0.76f, 0.28f, 0.44f, 0.67f);
        colours[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.47f, 0.47f, 0.67f);
        colours[ImGuiCol_Separator] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colours[ImGuiCol_SeparatorHovered] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colours[ImGuiCol_SeparatorActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colours[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);

        colours[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
        colours[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
        colours[ImGuiCol_Tab] = ImVec4(0.07f, 0.07f, 0.07f, 0.51f);
        colours[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.23f, 0.43f, 0.67f);
        colours[ImGuiCol_TabActive] = ImVec4(0.19f, 0.19f, 0.19f, 0.57f);
        colours[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.05f, 0.05f, 0.90f);
        colours[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13f, 0.13f, 0.13f, 0.74f);
#if defined(IMGUI_HAS_DOCK)
        colours[ImGuiCol_DockingPreview] = ImVec4(0.47f, 0.47f, 0.47f, 0.47f);
        colours[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
#endif //IMGUI_HAS_DOCK
        colours[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);

        colours[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colours[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colours[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
#if defined(IMGUI_HAS_TABLE)
        colours[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colours[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colours[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colours[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colours[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
#endif //IMGUI_HAS_TABLE
        colours[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colours[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);

        colours[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colours[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colours[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colours[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }
}//Anon

//Graphics Data
ImguiService* ImguiService::instance()
{
    static ImguiService imguiService;
    return &imguiService;
}

void ImguiService::init(void* configuration)
{
    ImguiServiceConfiguration* imguiConfig = reinterpret_cast<ImguiServiceConfiguration*>(configuration);
    gpu = imguiConfig->gpu;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    //Setup Platform/Renderer bindings.
    ImGui_ImplSDL3_InitForVulkan(reinterpret_cast<SDL_Window*>(imguiConfig->windowHandle));
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Air_ImGUI";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    //Load font texture atlas
    unsigned char* pixels;
    int width;
    int height;

    //Load as RGBA 32-bits (75% of memory is wasted, but default font is small) because it is more 
    //likely to be compatible with user's existing shaders. If your ImTextureID represent a higher-level
    //concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    TextureCreation textureCreation;
    textureCreation.setFormatType(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D)
        .setData(pixels)
        .setSize(uint16_t(width), uint16_t(height), 1)
        .setFlags(1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .setName("Imgui_Font");
    FONT_TEXTURE = gpu->createTexture(textureCreation);

    //Store our identifies
    io.Fonts->TexID = reinterpret_cast<ImTextureID>(&FONT_TEXTURE);

    //Manual code. Used to remove dependency from that.
    ShaderStateCreation shaderCreation{};

    if (gpu->bindlessSupported)
    {
        FileReadResult vertexShaderCode = fileReadBinary("Assets/Shaders/imguiBindless.vert.spv", &MemoryService::instance()->scratchAllocator);
        FileReadResult fragShaderCode = fileReadBinary("Assets/Shaders/imguiBindless.frag.spv", &MemoryService::instance()->scratchAllocator);

        shaderCreation.setName("Imgui")
            .addStage(vertexShaderCode.data, vertexShaderCode.size, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragShaderCode.data, fragShaderCode.size, VK_SHADER_STAGE_FRAGMENT_BIT)
            .setSPVInput(true);
    }
    else
    {
        FileReadResult vertexShaderCode = fileReadBinary("Assets/Shaders/imgui.vert.spv", &MemoryService::instance()->scratchAllocator);
        FileReadResult fragShaderCode = fileReadBinary("Assets/Shaders/imgui.frag.spv", &MemoryService::instance()->scratchAllocator);

        shaderCreation.setName("Imgui")
            .addStage(vertexShaderCode.data, vertexShaderCode.size, VK_SHADER_STAGE_VERTEX_BIT)
            .addStage(fragShaderCode.data, fragShaderCode.size, VK_SHADER_STAGE_FRAGMENT_BIT)
            .setSPVInput(true);
    }

    PipelineCreation pipelineCreation{};
    pipelineCreation.name = "Pipeline_Imgui";
    pipelineCreation.shaders = shaderCreation;

    pipelineCreation.blendState.addBlendState().setColour(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD);

    pipelineCreation.vertexInput.addVertexAttribute({ 0, 0, 0, VK_FORMAT_R32G32_SFLOAT })
        .addVertexAttribute({ 1, 0, 8, VK_FORMAT_R32G32_SFLOAT })
        .addVertexAttribute({ 2, 0, 16, VK_FORMAT_R8G8B8A8_UINT });

    pipelineCreation.vertexInput.addVertexStream({ 0, 20, VK_VERTEX_INPUT_RATE_VERTEX });
    pipelineCreation.renderPass = gpu->getSwapchainOutput();

    DescriptorSetLayoutCreation descriptorSetLayoutCreation{};
    if (gpu->bindlessSupported)
    {
        descriptorSetLayoutCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, VK_SHADER_STAGE_VERTEX_BIT, "LocalConstants" })
            .addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "Texture" })
            .setName("RLL_Imgui");
    }
    else
    {
        descriptorSetLayoutCreation.addBinding({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, VK_SHADER_STAGE_VERTEX_BIT, "LocalConstants" })
            .setName("RLL_Imgui");
        descriptorSetLayoutCreation.addBinding({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, VK_SHADER_STAGE_FRAGMENT_BIT, "Texture" })
            .setName("RLL_Imgui");
    }

    sDescriptorSetLayout = gpu->createDescriptorSetLayout(descriptorSetLayoutCreation);
    pipelineCreation.addDescriptorSetLayout(sDescriptorSetLayout);
    IMGUI_PIPELINE = gpu->createPipeline(pipelineCreation);

    //Create constant buffer.
    BufferCreation cbCreation;
    cbCreation.set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64)
        .setName("CB_Imgui");
    UI_CB = gpu->createBuffer(cbCreation);

    //Create descriptor set
    DescriptorSetCreation dsCreation{};
    if (gpu->bindlessSupported == false)
    {
        dsCreation.setLayout(pipelineCreation.descriptorSetLayout[0])
            .buffer(UI_CB, 0)
            .texture(FONT_TEXTURE, 1)
            .setName("RL_Imgui");
    }
    else
    {
        dsCreation.setLayout(pipelineCreation.descriptorSetLayout[0])
            .buffer(UI_CB, 0)
            .setName("RL_Imgui");
    }
    UI_DESCRIPTOR_SET = gpu->createDescriptorSet(dsCreation);

    //Add descriptor set to the map
    TEXTURE_TO_DESCRIPTOR_SET.init(&MemoryService::instance()->systemAllocator, 4);
    TEXTURE_TO_DESCRIPTOR_SET.insert(FONT_TEXTURE.index, UI_DESCRIPTOR_SET.index);

    //Create vertex and index buffer
    BufferCreation vbCreation;
    vbCreation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VB_SIZE)
        .setName("VB_Imgui");
    VB = gpu->createBuffer(vbCreation);

    BufferCreation ibCreation;
    ibCreation.set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, IB_SIZE)
        .setName("IB_SIZE");
    IB = gpu->createBuffer(ibCreation);
}

void ImguiService::shutdown()
{
    FlatHashMapIterator it = TEXTURE_TO_DESCRIPTOR_SET.iteratorBegin();
    while (it.isValid())
    {
        DescriptorSetHandle resourceHandle;
        resourceHandle.index = TEXTURE_TO_DESCRIPTOR_SET.get(it);
        gpu->destroyDescriptorSet(resourceHandle);
        TEXTURE_TO_DESCRIPTOR_SET.iteratorAdvance(it);
    }
    
    TEXTURE_TO_DESCRIPTOR_SET.shutdown();
    
    gpu->destroyBuffer(VB);
    gpu->destroyBuffer(IB);
    gpu->destroyBuffer(UI_CB);
    gpu->destroyDescriptorSetLayout(sDescriptorSetLayout);

    gpu->destroyPipeline(IMGUI_PIPELINE);
    gpu->destroyTexture(FONT_TEXTURE);

    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImguiService::newFrame()
{
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}


void ImguiService::render(CommandBuffer& commands)
{
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();

    //Avoid rendering when minimised, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0)
    {
        return;
    }

    //Vulkan backend has a different origin than OpenGL
    bool clipOriginLowerLeft = false;

    size_t vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    size_t indexSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    if (vertexSize >= VB_SIZE || indexSize >= IB_SIZE)
    {
        vprint("ImGui Backend Error: vertex/index overflow.\n");
        return;
    }

    if (vertexSize == 0 && indexSize == 0)
    {
        return;
    }

    ImDrawVert* vertexDst = nullptr;
    ImDrawIdx* indexDst = nullptr;

    MapBufferParameters mapParametersVB = { VB, 0, static_cast<uint32_t>(vertexSize) };
    vertexDst = reinterpret_cast<ImDrawVert*>(gpu->mapBuffer(mapParametersVB));

    if (vertexDst)
    {
        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            memcpy(vertexDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            vertexDst += cmdList->VtxBuffer.Size;
        }

        gpu->unmapBuffer(mapParametersVB);
    }

    MapBufferParameters mapParametersIB = { IB, 0, static_cast<uint32_t>(indexSize) };
    indexDst = reinterpret_cast<ImDrawIdx*>(gpu->mapBuffer(mapParametersIB));

    if (indexDst)
    {
        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            memcpy(indexDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            indexDst += cmdList->IdxBuffer.Size;
        }

        gpu->unmapBuffer(mapParametersIB);
    }


    //TODO: Look into trying to sorting here or if you need it at all.
    commands.pushMarker("ImGUI");

    commands.bindPass(gpu->getSwapchainPass());
    commands.bindPipeline(IMGUI_PIPELINE);
    commands.bindVertexBuffer(VB, 0, 0);
    commands.bindIndexBuffer(IB, 0, VK_INDEX_TYPE_UINT16);

    const Viewport viewport = { 0, 0, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight), 0.f, 1.f };
    commands.setViewport(&viewport);

    //Setup viewport, orthographic projection matrix.
    //Our visible imgui space lies from the drawData->DisplayPos (the top left) to drawData->DisplayPos + drawData->DisplaySize (bottom right). 
    //DisplayMin is typically (0, 0) for single viewport apps.
    float left = drawData->DisplayPos.x;
    float right = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float top = drawData->DisplayPos.y;
    float bottom = drawData->DisplayPos.y + drawData->DisplaySize.y;
    const float orthoProjection[4][4] =
    {
        { 2.f / (right - left),            0.f,                              0.f, 0.f },
        { 0.f,                             2.f / (top - bottom),             0.f, 0.f },
        { 0.f,                             0.f,                             -1.f, 0.f },
        { (right + left) / (left - right), (top + bottom) / (bottom - top), 0.f, 1.f }
    };

    MapBufferParameters cbMap = { UI_CB, 0, 0 };
    float* cbData = static_cast<float*>(gpu->mapBuffer(cbMap));
    if (cbData)
    {
        memcpy(cbData, &orthoProjection[0][0], 64);
        gpu->unmapBuffer(cbMap);
    }

    //Will project scissor/clipping rectangles into framebuffer space
    //(0, 0) unless using multi-viewport
    ImVec2 clipOff = drawData->DisplayPos;
    //(1, 1) unless using retina display which are often (2, 2)
    ImVec2 clipScale = drawData->FramebufferScale;

    //Render command lists.
    int counts = drawData->CmdListsCount;
    TextureHandle lastTexture = FONT_TEXTURE;

    //TODO: set up the mapping with this descriptor set like how you do it with the textures.
    DescriptorSetHandle lastDescriptorSet = { TEXTURE_TO_DESCRIPTOR_SET.get(lastTexture.index) };

    commands.bindDescriptorSet(&lastDescriptorSet, 1, nullptr, 0);

    uint32_t vertexBufferOffset = 0;
    uint32_t indexBufferOffset = 0;
    for (int i = 0; i < counts; ++i)
    {
        const ImDrawList* cmdList = drawData->CmdLists[i];

        for (int cmdIter = 0; cmdIter < cmdList->CmdBuffer.Size; ++cmdIter)
        {
            const ImDrawCmd* drawCmd = &cmdList->CmdBuffer[cmdIter];
            if (drawCmd->UserCallback)
            {
                //User callback (registered via ImDrawList::AddCallback)
                drawCmd->UserCallback(cmdList, drawCmd);
            }
            else
            {
                //Project scrissor/clipping rectangles into framebuffer space.
                ImVec4 clipRect;
                clipRect.x = (drawCmd->ClipRect.x - clipOff.x) * clipScale.x;
                clipRect.y = (drawCmd->ClipRect.y - clipOff.y) * clipScale.y;
                clipRect.z = (drawCmd->ClipRect.z - clipOff.x) * clipScale.x;
                clipRect.w = (drawCmd->ClipRect.w - clipOff.y) * clipScale.y;

                if (clipRect.x < fbWidth && clipRect.y < fbHeight && clipRect.z >= 0.f && clipRect.w >= 0.f)
                {
                    //Apply scissor/clipping rectangle
                    if (clipOriginLowerLeft)
                    {
                        Rect2DInt scissorRect =
                        {
                            .x = static_cast<int16_t>(clipRect.x),
                            .y = static_cast<int16_t>(fbHeight - clipRect.w),
                            .width = static_cast<uint16_t>(clipRect.z - clipRect.x),
                            .height = static_cast<uint16_t>(clipRect.w - clipRect.y),
                        };
                        commands.setScissor(&scissorRect);
                    }
                    else
                    {
                        Rect2DInt scissorRect =
                        {
                            .x = static_cast<int16_t>(clipRect.x),
                            .y = static_cast<int16_t>(clipRect.y),
                            .width = static_cast<uint16_t>(clipRect.z - clipRect.x),
                            .height = static_cast<uint16_t>(clipRect.w - clipRect.y),
                        };
                        commands.setScissor(&scissorRect);
                    }

                    //Retrieve
                    TextureHandle* newTexture = reinterpret_cast<TextureHandle*>(drawCmd->TexRef._TexID);
                    if (gpu->bindlessSupported == false)
                    {
                        if (newTexture->index != lastTexture.index && newTexture->index != INVALID_TEXTURE.index)
                        {
                            lastTexture = *newTexture;
                            FlatHashMapIterator it = TEXTURE_TO_DESCRIPTOR_SET.find(lastTexture.index);

                            //TODO: When you have an invalidate handle and when do you update descriptor set? 
                            //He found this problem when reusing the handle from a previous if not present.
                            if (it.isInvalid())
                            {
                                //Create new descriptor set
                                DescriptorSetCreation dsCreation{};

                                dsCreation.setLayout(sDescriptorSetLayout)
                                    .buffer(UI_CB, 0)
                                    .texture(lastTexture, 1)
                                    .setName("RL_Dynamic_Imgui");
                                lastDescriptorSet = gpu->createDescriptorSet(dsCreation);

                                TEXTURE_TO_DESCRIPTOR_SET.insert(newTexture->index, lastDescriptorSet.index);
                            }
                            else
                            {
                                lastDescriptorSet.index = TEXTURE_TO_DESCRIPTOR_SET.get(it);
                            }
                            commands.bindDescriptorSet(&lastDescriptorSet, 1, nullptr, 0);
                        }
                    }

                    commands.drawIndexed(drawCmd->ElemCount, 1, indexBufferOffset + drawCmd->IdxOffset,
                        vertexBufferOffset + drawCmd->VtxOffset, newTexture->index);
                }
            }
        }

        indexBufferOffset += cmdList->IdxBuffer.Size;
        vertexBufferOffset += cmdList->VtxBuffer.Size;
    }

    commands.popMarker();
}

//Removes the Texture from the cache and destroy the associated descriptor set.
void ImguiService::removeCachedTexture(TextureHandle& texture)
{
    FlatHashMapIterator it = TEXTURE_TO_DESCRIPTOR_SET.find(texture.index);
    if (it.isValid())
    {
        //Destroy descriptor set
        DescriptorSetHandle descriptorSet = { TEXTURE_TO_DESCRIPTOR_SET.get(it) };
        gpu->destroyDescriptorSet(descriptorSet);

        //Remove from cache
        TEXTURE_TO_DESCRIPTOR_SET.remove(texture.index);
    }
}

void ImguiService::setStyle(ImguiStyles style)
{
    switch (style)
    {
    case GREEN_BLUE:
        setStyleGreenBlue();
        break;
    case DARK_RED:
        setStyleDarkRed();
        break;
    case DARK_GOLD:
        setStyleDarkGold();
        break;
    default:
        ImGui::StyleColorsDark();
        break;
    }
}

//This is an example dialogue but we are going to use it as a logger.
struct AppLogger
{
    ImGuiTextBuffer buffer;
    ImGuiTextFilter filter;
    //Index to lines offset. We main this with the addLog() calls, allowing us to have random access on lines.
    ImVector<int>   lineOffsets;
    //Keep scrolling if already at the bottom.
    bool autoScroll;

    AppLogger()
    {
        autoScroll = true;
        clear();
    }

    void clear()
    {
        buffer.clear();
        lineOffsets.clear();
        lineOffsets.push_back(0);
    }

    void addLog(const char* format, ...) IM_FMTARGS(2)
    {
        int oldSize = buffer.size();
        va_list args;
        va_start(args, format);
        buffer.appendfv(format, args);
        va_end(args);

        for (int newSize = buffer.size(); oldSize < newSize; ++oldSize)
        {
            if (buffer[oldSize] == '\n')
            {
                lineOffsets.push_back(oldSize + 1);
            }
        }
    }

    void draw(const char* title, bool* pOpen = nullptr)
    {
        if (ImGui::Begin(title, pOpen) == false)
        {
            ImGui::End();
            return;
        }

        //Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &autoScroll);
            ImGui::EndPopup();
        }

        //Main window
        if (ImGui::Button("Options"))
        {
            ImGui::OpenPopup("Options");
        }
        ImGui::SameLine();
        bool clearFlag = ImGui::Button("Clear");
        ImGui::SameLine();
        bool copyFlag = ImGui::Button("Copy");
        ImGui::SameLine();
        filter.Draw("Filter", -100.f);

        ImGui::Separator();
        ImGui::BeginChild("Scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (clearFlag)
        {
            clear();
        }

        if (copyFlag)
        {
            ImGui::LogToClipboard();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buff = buffer.begin();
        const char* buffEnd = buffer.end();
        if (filter.IsActive())
        {
            //We don't use the clipper when the filter is enabled.
            //This is because we don't have a random access on the result on our filter.
            //A real application preccessing logs with ten of thousands of entries may want to store the result of search/filter.
            //especially if the filtering function is not trivial e.g reg-exp.
            for (int lineNum = 0; lineNum < lineOffsets.Size; ++lineNum)
            {
                const char* lineStart = buff + lineOffsets[lineNum];
                const char* lineEnd = (lineNum + 1 < lineOffsets.Size) ? (buff + lineOffsets[lineNum + 1] - 1) : buffEnd;
                if (filter.PassFilter(lineStart, lineEnd))
                {
                    ImGui::TextUnformatted(lineStart, lineEnd);
                }
            }
        }
        else
        {
            //The simplest and easy to play the entire buffer: ImGui::TextUnformatted(bufferBegin, bufferEnd) and it'll just work.
            //TextUnformatted() has specialisation for large blob of text and will fast-forward to skip non-visible lines.
            //Here we instead demonstrate using the clipper to only process lines that are within the visible area.
            //If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them on your side is recommended.
            //Using ImGuiListClipper requires A) random access into your data, and B) items all being the same height, both of which we can handle since
            //we an array pointing to the beginning of each line of text. When using the filter (in the block of code above) we don't have random 
            //access into data to display anymore, which is why we don't use the clipper. Storing or skimming through the search result would make it
            //possible (and would be recommended if you want to earch through tens of thousands of entries)
            ImGuiListClipper clipper;
            clipper.Begin(lineOffsets.Size);
            while (clipper.Step())
            {
                for (int lineNum = clipper.DisplayStart; lineNum < clipper.DisplayEnd; ++lineNum)
                {
                    const char* lineStart = buff + lineOffsets[lineNum];
                    const char* lineEnd = (lineNum + 1 < lineOffsets.Size) ? (buff + lineOffsets[lineNum + 1] - 1) : buffEnd;
                    ImGui::TextUnformatted(lineStart, lineEnd);
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.f);
        }

        ImGui::EndChild();
        ImGui::End();
    }
};

static AppLogger IMGUI_LOG;
static bool IMGUI_LOG_OPEN = true;

void imguiPrint(const char* text)
{
    IMGUI_LOG.addLog("%s", text);
}

void imguiLogInit()
{
    LogService::instance()->setCallback(&imguiPrint);
}

void imguiLogShutdown()
{
    LogService::instance()->setCallback(nullptr);
}

void imguiLogDraw()
{
    IMGUI_LOG.draw("Log", &IMGUI_LOG_OPEN);
}

//Plot with a ring buffer
//https://github.com/leiradel/ImGuiAl
template<typename T, size_t L>
class Sparkline
{
public:
    Sparkline()
    {
        setLimits(0, 1);
        clear();
    }

    void setLimits(T const min, T const max)
    {
        _min = static_cast<float>(min);
        _max = static_cast<float>(max);
    }

    void add(T const value)
    {
        _offset = (_offset + 1) % L;
        _values[_offset] = value;
    }

    void clear()
    {
        memset(_values, 0, L * sizeof(T));
        _offset = L - 1;
    }

    void draw(char const* const label = "", ImVec2 const size = ImVec2()) const
    {
        char overlay[32];
        print(overlay, sizeof(overlay), _values[_offset]);

        ImGui::PlotLines(label, getValue, const_cast<Sparkline*>(this), L, 0, overlay, _min, _max, size);
    }

private:
    float _min;
    float _max;
    T _values[L];
    size_t _offset;

    static float getValue(void* const data, int const idx)
    {
        Sparkline const* const self = static_cast<Sparkline*>(data);
        size_t const index = (idx + self->_offset + 1) % L;

        return static_cast<float>(self->_values[index]);
    }

    static void print(char* const buffer, size_t const bufferLen, int const value)
    {
        snprintf(buffer, bufferLen, "%d", value);
    }

    static void print(char* const buffer, size_t const bufferLen, double const value)
    {
        snprintf(buffer, bufferLen, "%f", value);
    }
};

static Sparkline<float, 100> FPS_LINE;

//FPS graph
void imguiFPSInit()
{
    FPS_LINE.clear();
    FPS_LINE.setLimits(0.f, 33.f);
}

void imguiFPSShutdown()
{
}

void imguiFPSAdd(float deltaTime)
{
    FPS_LINE.add(deltaTime);
}

void imguiFPSDraw()
{
    ImVec2 size(0, 100);
    FPS_LINE.draw("FPS", size);
}