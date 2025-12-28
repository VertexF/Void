#ifndef GPU_PROFILER_HDR
#define GPU_PROFILER_HDR

#include "Foundation/Platform.hpp"
#include "Foundation/Memory.hpp"
#include "GPUDevice.hpp"

struct GPUProfiler 
{
    void init(Allocator* newAllocator, uint32_t newMaxFrames);
    void shutdown();

    void update(GPUDevice& gpu);

    void imguiDraw();

    Allocator* allocator;
    GPUTimestamp* timestamps;
    uint16_t* perFrameActive;

    uint32_t maxFrames;
    uint32_t currentFrame;

    float maxTime;
    float minTime;
    float averageTime;

    float maxDuration;
    bool paused;
};


#endif // !GPU_PROFILER_HDR
