#include <Foundation/Memory/Allocators/Advanced/BuddyAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadLocalLinearAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeLinearAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TrackedAllocator.hpp>
#include <Foundation/Memory/Allocators/BinnedAllocator.hpp>
#include <Foundation/Memory/Allocators/FrameAllocator.hpp>
#include <Foundation/Memory/Allocators/LinearAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Allocators/PoolAllocator.hpp>
#include <Foundation/Memory/Allocators/StackAllocator.hpp>
#include <Foundation/Memory/Debug/DebugAllocator.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <thread>

namespace Engine::Memory {
namespace {

void ExpectGrowPreservesPayload(const char* name, IAllocator& allocator)
{
    SCOPED_TRACE(name);
    auto* values = static_cast<uint32*>(allocator.Allocate(sizeof(uint32) * 4, alignof(uint32)));
    ASSERT_NE(values, nullptr);
    values[0] = 2;
    values[1] = 3;
    values[2] = 5;
    values[3] = 8;

    auto* grown = static_cast<uint32*>(allocator.Reallocate(values, sizeof(uint32) * 16, alignof(uint32)));
    ASSERT_NE(grown, nullptr);
    EXPECT_EQ(grown[0], 2u);
    EXPECT_EQ(grown[1], 3u);
    EXPECT_EQ(grown[2], 5u);
    EXPECT_EQ(grown[3], 8u);

    allocator.Free(grown);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace

TEST(MemoryReallocation, GeneralPurposeAllocatorsPreservePayloadOnGrowth)
{
    MallocAllocator mallocAllocator;
    ExpectGrowPreservesPayload("MallocAllocator", mallocAllocator);

    BinnedAllocator binnedAllocator;
    ExpectGrowPreservesPayload("BinnedAllocator", binnedAllocator);

    TLSFAllocator tlsfAllocator(1ull * 1024ull * 1024ull);
    ExpectGrowPreservesPayload("TLSFAllocator", tlsfAllocator);

    BuddyAllocator buddyAllocator(1ull * 1024ull * 1024ull, 256);
    ExpectGrowPreservesPayload("BuddyAllocator", buddyAllocator);

    TrackedAllocator trackedAllocator;
    ExpectGrowPreservesPayload("TrackedAllocator", trackedAllocator);

    DebugAllocator debugAllocator;
    ExpectGrowPreservesPayload("DebugAllocator", debugAllocator);

    ThreadSafeAllocator threadSafeAllocator;
    ExpectGrowPreservesPayload("ThreadSafeAllocator", threadSafeAllocator);
}

TEST(ArenaAllocators, LinearScopeRewindsAndChildArenaUsesParentStorage)
{
    LinearAllocator allocator(4096);
    void* first = allocator.Allocate(128, 16);
    ASSERT_NE(first, nullptr);
    const size_t marker = allocator.GetMarker();
    void* scoped = allocator.Allocate(256, 16);
    ASSERT_NE(scoped, nullptr);
    EXPECT_GT(allocator.AllocatedSize(), marker);

    allocator.RewindToMarker(marker);
    EXPECT_EQ(allocator.AllocatedSize(), marker);
    EXPECT_FALSE(allocator.Owns(scoped));
    EXPECT_TRUE(allocator.Owns(first));

    LinearAllocator* child = allocator.CreateChildArena(512);
    ASSERT_NE(child, nullptr);
    void* childMemory = child->Allocate(128, 16);
    ASSERT_NE(childMemory, nullptr);
    EXPECT_TRUE(child->Owns(childMemory));
}

TEST(ArenaAllocators, FrameAllocatorBeginFrameResetsAllAllocations)
{
    FrameAllocator allocator(4096);
    void* a = allocator.Allocate(256, 16);
    void* b = allocator.Allocate(512, 16);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_GT(allocator.AllocatedSize(), 0u);

    allocator.BeginFrame();
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_FALSE(allocator.Owns(a));
    EXPECT_FALSE(allocator.Owns(b));
}

TEST(ArenaAllocators, StackAllocatorRequiresLifoFree)
{
    StackAllocator allocator(4096);
    int stackValue = 0;
    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);

    void* a = allocator.Allocate(128, 16);
    void* b = allocator.Allocate(128, 16);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const size_t fullSize = allocator.AllocatedSize();

    EXPECT_FALSE(allocator.Owns(static_cast<uint8*>(b) + 1));
    allocator.Free(static_cast<uint8*>(b) + 1);
    EXPECT_EQ(allocator.AllocatedSize(), fullSize);

    allocator.Free(a);
    EXPECT_EQ(allocator.AllocatedSize(), fullSize);

    allocator.Free(b);
    EXPECT_LT(allocator.AllocatedSize(), fullSize);
    EXPECT_FALSE(allocator.Owns(b));
    EXPECT_TRUE(allocator.Owns(a));
    allocator.Free(a);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_FALSE(allocator.Owns(a));
}

TEST(ArenaAllocators, StackAllocatorRejectsForwardMarkers)
{
    StackAllocator allocator(256);
    void* ptr = allocator.Allocate(32, 16);
    ASSERT_NE(ptr, nullptr);

    const size_t allocated = allocator.AllocatedSize();
    allocator.RewindToMarker(allocated + 64);
    EXPECT_EQ(allocator.AllocatedSize(), allocated);
    EXPECT_FALSE(allocator.Owns(static_cast<uint8*>(ptr) + 40));

    allocator.Reset();
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_FALSE(allocator.Owns(ptr));
}

TEST(ArenaAllocators, LinearAndFrameAllocatorsRejectForwardMarkers)
{
    LinearAllocator linear(256);
    void* linearPtr = linear.Allocate(32, 16);
    ASSERT_NE(linearPtr, nullptr);
    const size_t linearAllocated = linear.AllocatedSize();

    linear.RewindToMarker(linearAllocated + 64);
    EXPECT_EQ(linear.AllocatedSize(), linearAllocated);
    EXPECT_FALSE(linear.Owns(static_cast<uint8*>(linearPtr) + 40));

    FrameAllocator frame(256);
    void* framePtr = frame.Allocate(32, 16);
    ASSERT_NE(framePtr, nullptr);
    const size_t frameAllocated = frame.AllocatedSize();

    frame.RewindToMarker(frameAllocated + 64);
    EXPECT_EQ(frame.AllocatedSize(), frameAllocated);
    EXPECT_FALSE(frame.Owns(static_cast<uint8*>(framePtr) + 40));
}

TEST(ArenaAllocators, ArenaStatsExposeReservedFreeAndFailures)
{
    LinearAllocator linear(256);
    void* linearPtr = linear.Allocate(64, 16);
    ASSERT_NE(linearPtr, nullptr);
    AllocatorStats linearStats = linear.GetDetailedStats();
    EXPECT_EQ(linearStats.reservedBytes, 256u);
    EXPECT_GT(linearStats.freeBytes, 0u);
    EXPECT_EQ(linear.Allocate(1024, 16), nullptr);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(linear.GetStats().failedAllocationCount, 0u);
    }
    linear.Reset();
    EXPECT_EQ(linear.GetStats().liveBytes, 0u);

    FrameAllocator frame(256);
    void* framePtr = frame.Allocate(64, 16);
    ASSERT_NE(framePtr, nullptr);
    AllocatorStats frameStats = frame.GetDetailedStats();
    EXPECT_EQ(frameStats.reservedBytes, 256u);
    EXPECT_GT(frameStats.freeBytes, 0u);
    frame.BeginFrame();
    EXPECT_EQ(frame.GetStats().liveBytes, 0u);

    StackAllocator stack(256);
    void* stackPtr = stack.Allocate(64, 16);
    ASSERT_NE(stackPtr, nullptr);
    AllocatorStats stackStats = stack.GetDetailedStats();
    EXPECT_EQ(stackStats.reservedBytes, 256u);
    EXPECT_GT(stackStats.freeBytes, 0u);
    stack.Free(stackPtr);
    EXPECT_EQ(stack.GetStats().liveBytes, 0u);
}

TEST(ArenaAllocators, PoolAllocatorReusesFixedBlocks)
{
    PoolAllocator allocator(128, 4, nullptr, 16);
    int stackValue = 0;
    EXPECT_FALSE(allocator.Owns(&stackValue));
    const size_t failuresBeforeInvalidFree = allocator.GetStats().failedAllocationCount;
    allocator.Free(&stackValue);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresBeforeInvalidFree);
    }
    EXPECT_EQ(allocator.Allocate(0, 16), nullptr);
    EXPECT_EQ(allocator.Reallocate(nullptr, 0, 16), nullptr);

    void* a = allocator.Allocate(64, 16);
    void* b = allocator.Allocate(64, 16);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(allocator.Owns(a));
    EXPECT_FALSE(allocator.Owns(static_cast<uint8*>(a) + 1));
    EXPECT_EQ(allocator.GetAllocatedBlockCount(), 2u);

    void* same = allocator.Reallocate(a, 96, 16);
    EXPECT_EQ(same, a);
    EXPECT_EQ(allocator.Reallocate(a, 96, 24), a);
    EXPECT_EQ(allocator.Reallocate(a, 256, 16), nullptr);

    void* c = allocator.Allocate(64, 16);
    void* d = allocator.Allocate(64, 16);
    ASSERT_NE(c, nullptr);
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(allocator.IsFull());
    EXPECT_EQ(allocator.Allocate(64, 16), nullptr);

    allocator.Free(c);
    allocator.Free(d);

    allocator.Free(a);
    EXPECT_FALSE(allocator.Owns(a));
    const size_t failuresBeforeDoubleFree = allocator.GetStats().failedAllocationCount;
    allocator.Free(a);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresBeforeDoubleFree);
    }
    allocator.Free(b);
    EXPECT_EQ(allocator.GetAllocatedBlockCount(), 0u);

    void* resetBlock = allocator.Allocate(64, 16);
    ASSERT_NE(resetBlock, nullptr);
    allocator.Reset();
    EXPECT_EQ(allocator.GetAllocatedBlockCount(), 0u);
    EXPECT_EQ(allocator.GetFreeBlockCount(), allocator.GetBlockCount());
    EXPECT_FALSE(allocator.Owns(resetBlock));
}

TEST(ArenaAllocators, ThreadLinearAllocatorsReset)
{
    ThreadSafeLinearAllocator threadSafe(4096);
    void* a = threadSafe.Allocate(256, 16);
    ASSERT_NE(a, nullptr);
    EXPECT_GT(threadSafe.AllocatedSize(), 0u);
    EXPECT_TRUE(threadSafe.Owns(a));
    EXPECT_FALSE(threadSafe.Owns(static_cast<uint8*>(a) + 1));
    EXPECT_EQ(threadSafe.Reallocate(static_cast<uint8*>(a) + 1, 512, 16), nullptr);
    threadSafe.Reset();
    EXPECT_EQ(threadSafe.AllocatedSize(), 0u);
    EXPECT_FALSE(threadSafe.Owns(a));

    void* normalizedAlignment = threadSafe.Allocate(64, 24);
    ASSERT_NE(normalizedAlignment, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(normalizedAlignment) % alignof(MaxAlignT), 0u);

    ThreadLocalLinearAllocator threadLocal(4096);
    void* b = threadLocal.Allocate(256, 16);
    ASSERT_NE(b, nullptr);
    EXPECT_GT(threadLocal.AllocatedSize(), 0u);
    threadLocal.Reset();
    EXPECT_EQ(threadLocal.AllocatedSize(), 0u);
    EXPECT_FALSE(threadLocal.Owns(b));

    ThreadLocalLinearAllocator emptyThreadLocal(0);
    int localValue = 0;
    EXPECT_EQ(emptyThreadLocal.Allocate(16, 16), nullptr);
    EXPECT_EQ(emptyThreadLocal.Reallocate(nullptr, 16, 16), nullptr);
    EXPECT_FALSE(emptyThreadLocal.Owns(&localValue));
}

TEST(ArenaAllocators, ThreadLocalLinearDestructorReclaimsRegisteredWorkerState)
{
    MallocAllocator backing;
    ThreadSafeAllocator safeBacking(&backing);
    std::atomic<int> workerState{0};
    std::atomic<bool> releaseWorker{false};

    alignas(ThreadLocalLinearAllocator) std::byte storage[sizeof(ThreadLocalLinearAllocator)];
    auto* allocator = new (storage) ThreadLocalLinearAllocator(4096, &safeBacking);

    std::thread worker([&] {
        void* ptr = allocator->Allocate(512, 16);
        if (!ptr) {
            workerState.store(-1, std::memory_order_release);
            return;
        }
        workerState.store(1, std::memory_order_release);
        while (!releaseWorker.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });

    while (workerState.load(std::memory_order_acquire) == 0) {
        std::this_thread::yield();
    }

    ASSERT_EQ(workerState.load(std::memory_order_acquire), 1);
    EXPECT_NE(backing.AllocatedSize(), 0u);

    allocator->~ThreadLocalLinearAllocator();
    EXPECT_EQ(backing.AllocatedSize(), 0u);

    releaseWorker.store(true, std::memory_order_release);
    worker.join();
}

} // namespace Engine::Memory
