#include <Foundation/Memory/Debug/MemoryProfiler.hpp>

#include <gtest/gtest.h>

namespace Engine::Memory {

TEST(ZombieDetection, ReportsAllocationsNotAccessedForThresholdFrames)
{
    int stale = 0;
    int active = 0;
    MemoryProfiler profiler;

    profiler.UpdateFrameCount(10);
    profiler.TrackAlloc(&stale, 32, MemoryTag::Core);
    profiler.TrackAlloc(&active, 64, MemoryTag::Render);

    profiler.UpdateFrameCount(14);
    profiler.TrackAccess(&active);

    profiler.UpdateFrameCount(20);
    Vector<MemoryProfiler::ZombieAllocation> zombies = profiler.DetectZombies(8);

    ASSERT_EQ(zombies.size(), 1u);
    EXPECT_EQ(zombies[0].address, &stale);
    EXPECT_EQ(zombies[0].size, 32u);
    EXPECT_EQ(zombies[0].lastAccessFrame, 10u);
    EXPECT_EQ(zombies[0].ageFrames, 10u);
}

} // namespace Engine::Memory
