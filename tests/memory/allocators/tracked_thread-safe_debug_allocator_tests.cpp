#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TrackedAllocator.hpp>
#include <Foundation/Memory/Allocators/AlignedAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Debug/DebugAllocator.hpp>
#include <Foundation/Memory/Debug/LeakDetector.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <thread>
#include <vector>

namespace Engine::Memory {
namespace {

[[nodiscard]] bool IsAligned(void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

} // namespace

TEST(TrackedAllocator, TracksLivePeakAndReallocation)
{
    MallocAllocator backing;
    TrackedAllocator allocator(&backing);

    void* ptr = allocator.Allocate(64, 64);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsAligned(ptr, 64));
    EXPECT_TRUE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 64u);
    EXPECT_EQ(allocator.GetAllocationCount(), 1u);
    EXPECT_EQ(allocator.GetPeakAllocatedSize(), 64u);

    void* grown = allocator.Reallocate(ptr, 128, 64);
    ASSERT_NE(grown, nullptr);
    EXPECT_TRUE(IsAligned(grown, 64));
    EXPECT_TRUE(allocator.Owns(grown));
    EXPECT_EQ(allocator.AllocatedSize(), 128u);
    EXPECT_EQ(allocator.GetAllocationCount(), 1u);
    EXPECT_GE(allocator.GetPeakAllocatedSize(), 128u);

    allocator.Free(grown);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(allocator.GetAllocationCount(), 0u);
}

TEST(TrackedAllocator, RejectsInvalidAndDoubleFreeWithoutTouchingCounters)
{
    MallocAllocator backing;
    TrackedAllocator allocator(&backing);
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(allocator.GetAllocationCount(), 0u);

    void* ptr = allocator.Allocate(32, 16);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(allocator.Owns(ptr));
    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    allocator.Free(ptr);

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(allocator.GetAllocationCount(), 0u);
    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

TEST(ThreadSafeAllocator, ReallocateForwardsToBackingAllocator)
{
    MallocAllocator backing;
    ThreadSafeAllocator allocator(&backing);

    auto* ptr = static_cast<uint32*>(allocator.Allocate(sizeof(uint32) * 4, alignof(uint32)));
    ASSERT_NE(ptr, nullptr);
    ptr[0] = 7;
    ptr[1] = 11;

    auto* grown = static_cast<uint32*>(allocator.Reallocate(ptr, sizeof(uint32) * 8, alignof(uint32)));
    ASSERT_NE(grown, nullptr);
    EXPECT_EQ(grown[0], 7u);
    EXPECT_EQ(grown[1], 11u);

    allocator.Free(grown);
    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

TEST(ThreadSafeAllocator, HandlesConcurrentAllocFree)
{
    MallocAllocator backing;
    ThreadSafeAllocator allocator(&backing);

    constexpr int kThreadCount = 4;
    constexpr int kIterations = 512;
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([&allocator] {
            for (int i = 0; i < kIterations; ++i) {
                void* ptr = allocator.Allocate(96, 32);
                ASSERT_NE(ptr, nullptr);
                EXPECT_TRUE(IsAligned(ptr, 32));
                allocator.Free(ptr);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

TEST(AlignedAllocator, TracksUserBytesAndFreesBackingAllocation)
{
    MallocAllocator backing;
    AlignedAllocator allocator(64, &backing);
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);

    void* ptr = allocator.Allocate(80, 24);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsAligned(ptr, 64));
    EXPECT_TRUE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 80u);
    EXPECT_GT(backing.AllocatedSize(), 80u);

    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    allocator.Free(ptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

TEST(DebugAllocator, TracksLeakDetectorProfilerAndSupportsReallocate)
{
    MallocAllocator backing;
    LeakDetector leakDetector;
    MemoryProfiler profiler;
    DebugAllocator allocator(&backing, &leakDetector, &profiler);

    void* ptr = nullptr;
    {
        MemoryTagScope scope(MemoryTag::Debug);
        ptr = allocator.Allocate(32, 32);
    }

    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsAligned(ptr, 32));
    EXPECT_TRUE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 32u);
    EXPECT_EQ(leakDetector.GetLiveAllocationCount(), 1u);
    EXPECT_EQ(leakDetector.GetLiveBytes(), 32u);
    EXPECT_EQ(profiler.GetLiveBytes(), 32u);
    EXPECT_EQ(profiler.GetLiveBytesByTag(MemoryTag::Debug), 32u);

    auto* bytes = static_cast<uint8*>(ptr);
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(bytes[i], 0xCDu);
        bytes[i] = static_cast<uint8>(i);
    }

    void* grown = allocator.Reallocate(ptr, 64, 32);
    ASSERT_NE(grown, nullptr);
    EXPECT_TRUE(IsAligned(grown, 32));
    auto* grownBytes = static_cast<uint8*>(grown);
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(grownBytes[i], static_cast<uint8>(i));
    }
    EXPECT_EQ(leakDetector.GetLiveAllocationCount(), 1u);
    EXPECT_EQ(leakDetector.GetLiveBytes(), 64u);
    EXPECT_EQ(profiler.GetLiveBytes(), 64u);

    allocator.Free(grown);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(leakDetector.GetLiveAllocationCount(), 0u);
    EXPECT_EQ(leakDetector.GetLiveBytes(), 0u);
    EXPECT_EQ(profiler.GetLiveBytes(), 0u);
    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

TEST(DebugAllocator, RejectsInvalidAndDoubleFreeThroughLiveRegistry)
{
    MallocAllocator backing;
    LeakDetector leakDetector;
    MemoryProfiler profiler;
    DebugAllocator allocator(&backing, &leakDetector, &profiler);
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(leakDetector.GetLiveAllocationCount(), 0u);
    EXPECT_EQ(profiler.GetLiveBytes(), 0u);

    void* ptr = allocator.Allocate(48, 16);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(allocator.Owns(ptr));
    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    allocator.Free(ptr);

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(leakDetector.GetLiveAllocationCount(), 0u);
    EXPECT_EQ(profiler.GetLiveBytes(), 0u);
    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

TEST(DebugAllocator, ReportsGuardOverwritesWithoutFreeingAllocation)
{
    MallocAllocator backing;
    DebugAllocator allocator(&backing);

    void* ptr = allocator.Allocate(24, 16);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(allocator.ValidateAllocation(ptr), DebugAllocator::AllocationValidation::Valid);

    auto* bytes = static_cast<uint8*>(ptr);
    bytes[24] = 0x41u;
    EXPECT_EQ(allocator.ValidateAllocation(ptr), DebugAllocator::AllocationValidation::PostGuardCorrupt);
    const size_t failuresBeforePostGuardFree = allocator.GetStats().failedAllocationCount;
    allocator.Free(ptr);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresBeforePostGuardFree);
    }
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(backing.AllocatedSize(), 0u);

    ptr = allocator.Allocate(24, 16);
    ASSERT_NE(ptr, nullptr);
    bytes = static_cast<uint8*>(ptr);
    bytes[24] = 0xBBu;
    EXPECT_EQ(allocator.ValidateAllocation(ptr), DebugAllocator::AllocationValidation::Valid);

    bytes[-1] = 0x42u;
    EXPECT_EQ(allocator.ValidateAllocation(ptr), DebugAllocator::AllocationValidation::PreGuardCorrupt);
    bytes[-1] = 0xAAu;
    EXPECT_EQ(allocator.ValidateAllocation(ptr), DebugAllocator::AllocationValidation::Valid);

    allocator.Free(ptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(backing.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
