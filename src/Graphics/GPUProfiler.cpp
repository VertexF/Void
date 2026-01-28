#include "GPUProfiler.hpp"

#include "Foundation/HashMap.hpp"
#include "Foundation/Numerics.hpp"
#include "Foundation/Colour.hpp"

#include "Renderer.hpp"

#include "vender/imgui/imgui.h"

#include "Application/Window.hpp"

#include <cmath>
#include <stdio.h>

namespace 
{
    //GPU Task names to colours
    FlatHashMap<uint64_t, uint32_t> nameToColour;
    uint32_t initialFramesPaused = 3;
}

void GPUProfiler::init(Allocator* newAllocator, uint32_t newMaxFrames) 
{
    allocator = newAllocator;
    maxFrames = newMaxFrames;
    timestamps = reinterpret_cast<GPUTimestamp*>(void_alloca(sizeof(GPUTimestamp) * maxFrames * 32, allocator));
    perFrameActive = reinterpret_cast<uint16_t*>(void_alloca(sizeof(uint16_t) * maxFrames, allocator));

    maxDuration = 16.666f;
    currentFrame = 0;
    minTime = 0.f;
    maxTime = 0.f;
    averageTime = 0.f;
    paused = false;

    memset(perFrameActive, 0, 2 * maxFrames);

    nameToColour.init(allocator, 16);
    nameToColour.setDefaultValue(UINT32_MAX);
}

void GPUProfiler::shutdown() 
{
    nameToColour.shutdown();

    void_free(timestamps, allocator);
    void_free(perFrameActive, allocator);
}

void GPUProfiler::update(GPUDevice& gpu) 
{
    gpu.setGPUTimestampsEnable(paused == false);

    if (initialFramesPaused) 
    {
        --initialFramesPaused;
        return;
    }

    if (paused && Window::instance()->resizeRequested)
    {
        return;
    }

    uint32_t activeTimestamps = gpu.getGPUTimestamps(&timestamps[32 * currentFrame]);
    perFrameActive[currentFrame] = static_cast<uint16_t>(activeTimestamps);

    //Get the colours
    for (uint32_t i = 0; i < activeTimestamps; ++i) 
    {
        GPUTimestamp& timestamp = timestamps[32 * currentFrame + i];

        uint64_t hashedName = hashCalculate(timestamp.name);
        uint32_t colourIndex = nameToColour.get(hashedName);
        //No entry found add new colour.
        if (colourIndex == UINT32_MAX) 
        {
            colourIndex = static_cast<uint32_t>(nameToColour.size);
            nameToColour.insert(hashedName, colourIndex);
        }

        timestamp.colour = Colour::getDistinctColour(colourIndex);
    }

    currentFrame = (currentFrame + 1) % maxFrames;

    //Reset the min/max/average after few frames
    if (currentFrame == 0) 
    {
        maxTime = -FLT_MAX;
        minTime = FLT_MAX;
        averageTime = 0.f;
    }
}

void GPUProfiler::imguiDraw()
{
    if (initialFramesPaused)
    {
        return;
    }
    {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    float widgetHeight = canvasSize.y - 100;

    float legendWidth = 200;
    float graphWidth = fabsf(canvasSize.x - legendWidth);
    uint32_t rectWidth = ceilU32(graphWidth / maxFrames);
    int32_t rectX = ceilI32(graphWidth - rectWidth);

    double newAverage = 0.0;

    ImGuiIO& io = ImGui::GetIO();

    static char buff[128];

    const ImVec2 mousePos = io.MousePos;

    int32_t selectedFrame = -1;

    //Draw time reference lines
    sprintf(buff, "%3.4fms", maxDuration);
    drawList->AddText({ cursorPos.x, cursorPos.y }, 0xFF0000FF, buff);
    drawList->AddLine({ cursorPos.x + rectWidth, cursorPos.y }, { cursorPos.x + graphWidth, cursorPos.y }, 0xFF0000FF);

    sprintf(buff, "%3.4fms", maxDuration / 2.f);
    drawList->AddText({ cursorPos.x, cursorPos.y + widgetHeight / 2.f }, 0xFF00FFFF, buff);
    drawList->AddLine({ cursorPos.x + rectWidth, cursorPos.y + widgetHeight / 2.f }, { cursorPos.x + graphWidth, cursorPos.y + widgetHeight / 2.f }, 0xFF00FFFF);

    //Draw graph
    for (uint32_t i = 0; i < maxFrames; ++i)
    {
        uint32_t frameIndex = (currentFrame - 1 - i) % maxFrames;

        float frameX = cursorPos.x + rectX;
        GPUTimestamp* frameTimestamps = &timestamps[frameIndex * 32];
        float frameTime = static_cast<float>(frameTimestamps[0].elapsedMS);
        //Clamp values to note destroy the frame data.
        frameTime = clamp(frameTime, 0.00001f, 1000.f);
        //Update timings.
        newAverage += frameTime;
        minTime = min(minTime, frameTime);
        maxTime = max(maxTime, frameTime);

        float rectHeight = frameTime / maxDuration * widgetHeight;

        for (uint32_t j = 0; j < perFrameActive[frameIndex]; ++j)
        {
            const GPUTimestamp& timestamp = frameTimestamps[j];

            rectHeight = static_cast<float>(timestamp.elapsedMS) / maxDuration * widgetHeight;
            drawList->AddRectFilled({ frameX, cursorPos.y + widgetHeight - rectHeight },
                                    { frameX + rectWidth, cursorPos.y + widgetHeight },
                                    timestamp.colour);
        }

        if (mousePos.x >= frameX && mousePos.x < frameX + rectWidth &&
            mousePos.y >= cursorPos.y && mousePos.y < cursorPos.y + widgetHeight)
        {
            drawList->AddRectFilled({ frameX, cursorPos.y + widgetHeight }, { frameX + rectWidth, cursorPos.y }, 0xFFFFFFF);

            ImGui::SetTooltip("(%u): %f", frameIndex, frameTime);

            selectedFrame = frameIndex;
        }

        drawList->AddLine({ frameX, cursorPos.y + widgetHeight }, { frameX, cursorPos.y }, 0x0FFFFFFF);
        rectX -= rectWidth;
    }

    averageTime = static_cast<float>(newAverage) / maxFrames;

    //Draw key
    ImGui::SetCursorPosX(cursorPos.x + graphWidth);
    //Default to last frame if nothing is selected
    selectedFrame = selectedFrame == -1 ? (currentFrame - 1) % maxFrames : selectedFrame;
    if (selectedFrame >= 0)
    {
        GPUTimestamp* frameTimestamps = &timestamps[selectedFrame * 32];

        float x = cursorPos.x + graphWidth;
        float y = cursorPos.y;

        for (uint32_t j = 0; j < perFrameActive[selectedFrame]; ++j)
        {
            const GPUTimestamp& timestamp = frameTimestamps[j];

            drawList->AddRectFilled({ x, y }, { x + 8, y + 8 }, timestamp.colour);

            sprintf(buff, "(%d)-%s %2.4f", timestamp.depth, timestamp.name, timestamp.elapsedMS);
            drawList->AddText({ x + 12, y }, 0xFFFFFFFF, buff);

            y += 16;
        }
    }

    ImGui::Dummy({ canvasSize.x, widgetHeight });
}
    ImGui::SetNextItemWidth(100.f);
    ImGui::LabelText("", "Max %3.4fms", maxTime);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    ImGui::LabelText("", "Max %3.4fms", minTime);
    ImGui::SameLine();
    ImGui::LabelText("", "Ave %3.4fms", averageTime);

    ImGui::Separator();
    ImGui::Checkbox("Paused", &paused);

    static const char* items[] = {"200ms", "100ms" , "66ms", "33ms", "16ms", "8ms", "4ms" };
    static const float maxDurations[] = { 200.f, 100.f, 66.f, 33.f, 16.f, 8.f, 4.f };

    static int maxDurationIndex = 4;
    if (ImGui::Combo("Graph Max", &maxDurationIndex, items, IM_ARRAYSIZE(items)))
    {
        maxDuration = maxDurations[maxDurationIndex];
    }
}