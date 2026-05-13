#include <Foundation/Memory/Debug/LeakDetector.hpp>
#include <Foundation/Memory/Debug/GlobalAllocHooks.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>

#include <gtest/gtest.h>

#include <new>
#include <thread>
#include <vector>

namespace Engine::Memory {
namespace {

struct alignas(128) OverAlignedGlobalObject {
    uint32 value = 0xABCD1234u;
};

[[nodiscard]] bool IsAligned(void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

} // namespace

TEST(LeakDetector, TracksLiveAllocationsByPointer)
{
    int a = 0;
    int b = 0;
    LeakDetector detector;

    detector.TrackAlloc(&a, 32, MemoryTag::Core);
    detector.TrackAlloc(&b, 64, MemoryTag::Render);

    EXPECT_EQ(detector.GetLiveAllocationCount(), 2u);
    EXPECT_EQ(detector.GetLiveBytes(), 96u);

    Vector<LeakRecord> records = detector.GetLiveAllocations();
    EXPECT_EQ(records.size(), 2u);

    detector.TrackFree(&a);
    EXPECT_EQ(detector.GetLiveAllocationCount(), 1u);
    EXPECT_EQ(detector.GetLiveBytes(), 64u);

    detector.Reset();
    EXPECT_EQ(detector.GetLiveAllocationCount(), 0u);
    EXPECT_EQ(detector.GetLiveBytes(), 0u);
}

TEST(LeakDetector, ReplacingPointerUpdatesLiveBytes)
{
    int value = 0;
    LeakDetector detector;

    detector.TrackAlloc(&value, 64, MemoryTag::Core);
    detector.TrackAlloc(&value, 16, MemoryTag::Render);

    EXPECT_EQ(detector.GetLiveAllocationCount(), 1u);
    EXPECT_EQ(detector.GetLiveBytes(), 16u);

    Vector<LeakRecord> records = detector.GetLiveAllocations();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].tag, MemoryTag::Render);

    detector.TrackFree(&value);
    EXPECT_EQ(detector.GetLiveBytes(), 0u);
}

TEST(LeakDetector, HandlesConcurrentTrackAllocFree)
{
    LeakDetector detector;
    constexpr int kThreadCount = 4;
    constexpr int kIterations = 512;
    alignas(64) uint8 storage[kThreadCount][kIterations]{};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([&detector, &storage, threadIndex] {
            for (int i = 0; i < kIterations; ++i) {
                detector.TrackAlloc(&storage[threadIndex][i], 1, MemoryTag::Debug);
                detector.TrackFree(&storage[threadIndex][i]);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(detector.GetLiveAllocationCount(), 0u);
    EXPECT_EQ(detector.GetLiveBytes(), 0u);
}

TEST(MemoryProfiler, TracksLivePeakTagAndBudgetStats)
{
    int core = 0;
    int render = 0;
    MemoryProfiler profiler;

    profiler.SetBudgetByTag(MemoryTag::Core, 64);
    profiler.TrackAlloc(&core, 48, MemoryTag::Core);
    profiler.TrackAlloc(&render, 96, MemoryTag::Render);

    EXPECT_EQ(profiler.GetTotalAllocations(), 2u);
    EXPECT_EQ(profiler.GetLiveBytes(), 144u);
    EXPECT_EQ(profiler.GetPeakBytes(), 144u);
    EXPECT_EQ(profiler.GetLiveBytesByTag(MemoryTag::Core), 48u);
    EXPECT_EQ(profiler.GetLiveBytesByTag(MemoryTag::Render), 96u);
    EXPECT_EQ(profiler.GetBudgetByTag(MemoryTag::Core), 64u);

    profiler.TrackFree(&core, 0, MemoryTag::Default);
    EXPECT_EQ(profiler.GetTotalFrees(), 1u);
    EXPECT_EQ(profiler.GetLiveBytes(), 96u);
    EXPECT_EQ(profiler.GetLiveBytesByTag(MemoryTag::Core), 0u);
    EXPECT_EQ(profiler.GetPeakBytesByTag(MemoryTag::Render), 96u);
}

TEST(MemoryProfiler, HandlesConcurrentAllocationTracking)
{
    MemoryProfiler profiler;
    constexpr int kThreadCount = 4;
    constexpr int kIterations = 512;
    alignas(64) uint8 storage[kThreadCount][kIterations]{};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([&profiler, &storage, threadIndex] {
            for (int i = 0; i < kIterations; ++i) {
                profiler.TrackAlloc(&storage[threadIndex][i], 1, MemoryTag::Core);
                profiler.TrackAccess(&storage[threadIndex][i]);
                profiler.TrackFree(&storage[threadIndex][i], 0, MemoryTag::Default);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(profiler.GetTotalAllocations(), static_cast<size_t>(kThreadCount * kIterations));
    EXPECT_EQ(profiler.GetTotalFrees(), static_cast<size_t>(kThreadCount * kIterations));
    EXPECT_EQ(profiler.GetLiveBytes(), 0u);
    EXPECT_EQ(profiler.GetLiveBytesByTag(MemoryTag::Core), 0u);
    EXPECT_EQ(profiler.GetMissedFrees(), 0u);
}

TEST(GlobalAllocHooks, RoutesOperatorNewDeleteThroughProfiler)
{
    MemoryProfiler profiler;

    SetGlobalAllocProfiler(&profiler);
    auto* value = new uint32(0xC0FFEEu);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 0xC0FFEEu);
    EXPECT_EQ(GetGlobalAllocProfiler(), &profiler);
    EXPECT_GE(profiler.GetLiveBytes(), sizeof(uint32));

    delete value;
    SetGlobalAllocProfiler(nullptr);

    EXPECT_EQ(GetGlobalAllocProfiler(), nullptr);
    EXPECT_EQ(profiler.GetLiveBytes(), 0u);
    EXPECT_EQ(profiler.GetMissedFrees(), 0u);
}

TEST(GlobalAllocHooks, RoutesOverAlignedOperatorNewDeleteThroughProfiler)
{
    MemoryProfiler profiler;

    SetGlobalAllocProfiler(&profiler);
    auto* object = new OverAlignedGlobalObject();
    ASSERT_NE(object, nullptr);
    EXPECT_TRUE(IsAligned(object, alignof(OverAlignedGlobalObject)));
    EXPECT_EQ(object->value, 0xABCD1234u);
    EXPECT_GE(profiler.GetLiveBytes(), sizeof(OverAlignedGlobalObject));

    delete object;
    EXPECT_EQ(profiler.GetLiveBytes(), 0u);

    auto* array = new OverAlignedGlobalObject[3];
    ASSERT_NE(array, nullptr);
    EXPECT_TRUE(IsAligned(array, alignof(OverAlignedGlobalObject)));
    EXPECT_GE(profiler.GetLiveBytes(), sizeof(OverAlignedGlobalObject) * 3);

    delete[] array;
    SetGlobalAllocProfiler(nullptr);

    EXPECT_EQ(profiler.GetLiveBytes(), 0u);
    EXPECT_EQ(profiler.GetMissedFrees(), 0u);
}

TEST(GlobalAllocHooks, RoutesNothrowAlignedOperatorNewDeleteThroughProfiler)
{
    MemoryProfiler profiler;

    SetGlobalAllocProfiler(&profiler);
    void* ptr = ::operator new(
        sizeof(OverAlignedGlobalObject),
        std::align_val_t{alignof(OverAlignedGlobalObject)},
        std::nothrow);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsAligned(ptr, alignof(OverAlignedGlobalObject)));
    EXPECT_GE(profiler.GetLiveBytes(), sizeof(OverAlignedGlobalObject));

    ::operator delete(ptr, std::align_val_t{alignof(OverAlignedGlobalObject)}, std::nothrow);
    SetGlobalAllocProfiler(nullptr);

    EXPECT_EQ(profiler.GetLiveBytes(), 0u);
    EXPECT_EQ(profiler.GetMissedFrees(), 0u);
}

} // namespace Engine::Memory
