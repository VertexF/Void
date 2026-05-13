#include <Foundation/Memory/Debug/MemoryProfiler.hpp>

#include <gtest/gtest.h>

namespace Engine::Memory {

TEST(MemoryDifferentialAnalysis, DiffsLiveBytesAndCountsBetweenSnapshots)
{
    int a = 0;
    int b = 0;
    MemoryProfiler profiler;

    profiler.TrackAlloc(&a, 32, MemoryTag::Core);
    profiler.TakeSnapshot("before");

    profiler.TrackAlloc(&b, 96, MemoryTag::Render);
    profiler.TakeSnapshot("after");

    const MemoryProfiler::MemoryDiff diff = profiler.DiffSnapshots("before", "after");
    EXPECT_EQ(diff.nameA, "before");
    EXPECT_EQ(diff.nameB, "after");
    EXPECT_EQ(diff.totalByteDelta, 96);
    EXPECT_EQ(diff.totalCountDelta, 1);

    profiler.DeleteSnapshot("before");
    EXPECT_EQ(profiler.GetSnapshot("before"), nullptr);
    ASSERT_NE(profiler.GetSnapshot("after"), nullptr);
}

} // namespace Engine::Memory
