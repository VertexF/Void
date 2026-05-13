#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/Allocators/Advanced/BuddyAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/SecuredAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TLSCachingAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TrackedAllocator.hpp>
#include <Foundation/Memory/Allocators/AlignedAllocator.hpp>
#include <Foundation/Memory/Allocators/BinnedAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Allocators/MonotonicAllocator.hpp>
#include <Foundation/Memory/Allocators/PoolAllocator.hpp>
#include <Foundation/Memory/Debug/DebugAllocator.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <thread>

namespace Engine::Memory {
namespace {

struct TortureAllocation {
    void* ptr = nullptr;
    size_t size = 0;
    uint8 pattern = 0;
};

void FillPattern(TortureAllocation allocation)
{
    auto* bytes = static_cast<uint8*>(allocation.ptr);
    for (size_t i = 0; i < allocation.size; ++i) {
        bytes[i] = static_cast<uint8>(allocation.pattern + static_cast<uint8>(i * 13u));
    }
}

void ExpectPatternPrefix(const TortureAllocation& allocation, size_t count)
{
    const auto* bytes = static_cast<const uint8*>(allocation.ptr);
    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(bytes[i], static_cast<uint8>(allocation.pattern + static_cast<uint8>(i * 13u)));
    }
}

void RunAllocatorTorture(
    const char* name,
    IAllocator& allocator,
    uint32 seed,
    size_t maxSize,
    bool freeWithReallocateZero = false)
{
    SCOPED_TRACE(name);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> sizeDist(1, maxSize);
    std::uniform_int_distribution<int> actionDist(0, 99);
    std::uniform_int_distribution<int> alignmentDist(0, 3);
    Vector<TortureAllocation> live;
    live.reserve(256);

    auto nextAlignment = [&]() -> size_t {
        switch (alignmentDist(rng)) {
            case 0: return 8;
            case 1: return 16;
            case 2: return 32;
            default: return 24;
        }
    };

    auto releaseAllocation = [&](TortureAllocation allocation) {
        if (freeWithReallocateZero) {
            EXPECT_EQ(allocator.Reallocate(allocation.ptr, 0, 16), nullptr);
        } else {
            allocator.Free(allocation.ptr);
        }
    };

    for (int iteration = 0; iteration < 1200; ++iteration) {
        const int action = actionDist(rng);
        if (live.empty() || action < 50) {
            TortureAllocation allocation{};
            allocation.size = sizeDist(rng);
            allocation.pattern = static_cast<uint8>((iteration * 31) & 0xFF);
            allocation.ptr = allocator.Allocate(allocation.size, nextAlignment());
            if (allocation.ptr) {
                FillPattern(allocation);
                live.push_back(allocation);
            }
            continue;
        }

        const size_t index = static_cast<size_t>(rng()) % live.size();
        TortureAllocation allocation = live[index];

        if (action < 75) {
            ExpectPatternPrefix(allocation, allocation.size);
            releaseAllocation(allocation);
            live[index] = live.back();
            live.pop_back();
            continue;
        }

        const size_t newSize = sizeDist(rng);
        void* reallocated = allocator.Reallocate(allocation.ptr, newSize, nextAlignment());
        if (!reallocated) {
            ExpectPatternPrefix(allocation, allocation.size);
            continue;
        }

        allocation.ptr = reallocated;
        ExpectPatternPrefix(allocation, allocation.size < newSize ? allocation.size : newSize);
        allocation.size = newSize;
        allocation.pattern = static_cast<uint8>((iteration * 17) & 0xFF);
        FillPattern(allocation);
        live[index] = allocation;
    }

    for (TortureAllocation allocation : live) {
        releaseAllocation(allocation);
    }
    live.clear();

    if (auto* monotonic = dynamic_cast<MonotonicAllocator*>(&allocator)) {
        monotonic->Reset();
    }
    if (auto* tlsCache = dynamic_cast<TLSCachingAllocator*>(&allocator)) {
        tlsCache->FlushCache();
    }

    const AllocatorStats stats = allocator.GetStats();
    EXPECT_EQ(stats.liveBytes, 0u);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_GT(stats.allocationCount, 0u);
}

void ExpectFragmentationInvariant(const char* name, const AllocatorStats& stats)
{
    SCOPED_TRACE(name);
    EXPECT_LE(stats.largestFreeBlockBytes, stats.freeBytes);
    EXPECT_EQ(stats.fragmentationBytes,
        stats.freeBytes > stats.largestFreeBlockBytes ? stats.freeBytes - stats.largestFreeBlockBytes : 0u);
}

void RunFragmentationRecoverability(
    const char* name,
    IAllocator& allocator,
    uint32 seed,
    size_t maxSmallSize,
    size_t recoverySize,
    size_t finalRecoverySize)
{
    SCOPED_TRACE(name);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> sizeDist(8, maxSmallSize);
    Vector<TortureAllocation> live;
    live.reserve(1024);

    for (int i = 0; i < 2048; ++i) {
        TortureAllocation allocation{};
        allocation.size = AlignSize(sizeDist(rng), 8);
        allocation.pattern = static_cast<uint8>((i * 19) & 0xFF);
        allocation.ptr = allocator.Allocate(allocation.size, 16);
        if (allocation.ptr) {
            FillPattern(allocation);
            live.push_back(allocation);
        }
    }
    ASSERT_FALSE(live.empty());

    Vector<TortureAllocation> survivors;
    survivors.reserve(live.size());
    for (size_t i = 0; i < live.size(); ++i) {
        if ((i % 3u) == 0u || (i % 7u) == 0u) {
            ExpectPatternPrefix(live[i], live[i].size);
            allocator.Free(live[i].ptr);
        } else {
            survivors.push_back(live[i]);
        }
    }
    live.clear();
    live.reserve(survivors.size());
    for (TortureAllocation allocation : survivors) {
        live.push_back(allocation);
    }

    ExpectFragmentationInvariant(name, allocator.GetDetailedStats());

    Vector<TortureAllocation> recovered;
    recovered.reserve(64);
    for (int i = 0; i < 64; ++i) {
        TortureAllocation allocation{};
        allocation.size = recoverySize;
        allocation.pattern = static_cast<uint8>((i * 29) & 0xFF);
        allocation.ptr = allocator.Allocate(allocation.size, 16);
        if (allocation.ptr) {
            FillPattern(allocation);
            recovered.push_back(allocation);
        }
    }
    EXPECT_FALSE(recovered.empty());

    for (TortureAllocation allocation : recovered) {
        ExpectPatternPrefix(allocation, allocation.size);
        allocator.Free(allocation.ptr);
    }
    for (TortureAllocation allocation : live) {
        ExpectPatternPrefix(allocation, allocation.size);
        allocator.Free(allocation.ptr);
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    ExpectFragmentationInvariant(name, allocator.GetDetailedStats());

    void* recoveredBlock = allocator.Allocate(finalRecoverySize, 16);
    ASSERT_NE(recoveredBlock, nullptr);
    allocator.Free(recoveredBlock);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

void ExpectFailureCounterIncrements(const char* name, IAllocator& allocator, size_t size, size_t alignment)
{
    SCOPED_TRACE(name);
    const size_t before = allocator.GetStats().failedAllocationCount;
    void* ptr = allocator.Allocate(size, alignment);
    EXPECT_EQ(ptr, nullptr);
    if (ptr) {
        allocator.Free(ptr);
    }

    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, before);
    }
}

} // namespace

TEST(MemoryStress, TLSFDeterministicMixedAllocFree)
{
    TLSFAllocator allocator(512 * 1024);
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<size_t> sizeDist(1, 1024);
    std::uniform_int_distribution<int> actionDist(0, 2);

    Vector<void*> live;
    live.reserve(512);

    for (int i = 0; i < 5000; ++i) {
        if (!live.empty() && actionDist(rng) == 0) {
            const size_t index = static_cast<size_t>(rng()) % live.size();
            allocator.Free(live[index]);
            live[index] = live.back();
            live.pop_back();
            continue;
        }

        void* ptr = allocator.Allocate(sizeDist(rng));
        if (ptr) {
            live.push_back(ptr);
        }
    }

    for (void* ptr : live) {
        allocator.Free(ptr);
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MemoryStress, TLSFConcurrentRepeatedAllocFree)
{
    TLSFAllocator allocator(1024 * 1024);
    constexpr int kThreadCount = 8;
    constexpr int kIterations = 1000;

    auto worker = [&allocator](int threadIndex) {
        Vector<void*> live;
        live.reserve(64);

        for (int i = 0; i < kIterations; ++i) {
            const size_t size = 16u + static_cast<size_t>((threadIndex * 13 + i) % 240);
            void* ptr = allocator.Allocate(size);
            if (ptr) {
                live.push_back(ptr);
            }

            if (live.size() >= 32) {
                allocator.Free(live.front());
                live.erase(live.begin());
            }
        }

        for (void* ptr : live) {
            allocator.Free(ptr);
        }
    };

    Vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back(worker, i);
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MemoryStress, DeterministicAllocatorTorture)
{
    MallocAllocator mallocAllocator;
    RunAllocatorTorture("MallocAllocator", mallocAllocator, 0x1001u, 512);

    BinnedAllocator binnedAllocator;
    RunAllocatorTorture("BinnedAllocator", binnedAllocator, 0x1002u, 1024);

    TLSFAllocator tlsfAllocator(4ull * 1024ull * 1024ull);
    RunAllocatorTorture("TLSFAllocator", tlsfAllocator, 0x1003u, 512);

    BuddyAllocator buddyAllocator(4ull * 1024ull * 1024ull, 256);
    RunAllocatorTorture("BuddyAllocator", buddyAllocator, 0x1004u, 512);

    PoolAllocator poolAllocator(128, 4096, nullptr, 16);
    RunAllocatorTorture("PoolAllocator", poolAllocator, 0x1005u, 128);

    MallocAllocator monotonicBacking;
    MonotonicAllocator monotonicAllocator(monotonicBacking, 2ull * 1024ull * 1024ull);
    RunAllocatorTorture("MonotonicAllocator", monotonicAllocator, 0x1006u, 256, true);

    MallocAllocator debugBacking;
    DebugAllocator debugAllocator(&debugBacking);
    RunAllocatorTorture("DebugAllocator", debugAllocator, 0x1007u, 512);

    MallocAllocator trackedBacking;
    TrackedAllocator trackedAllocator(&trackedBacking);
    RunAllocatorTorture("TrackedAllocator", trackedAllocator, 0x1008u, 512);

    MallocAllocator tlsBacking;
    ThreadSafeAllocator threadSafeAllocator(&tlsBacking);
    TLSCachingAllocator tlsCachingAllocator(&threadSafeAllocator);
    RunAllocatorTorture("TLSCachingAllocator", tlsCachingAllocator, 0x1009u, 512);
}

TEST(MemoryStress, FragmentationRecoverabilityForGeneralPurposeAllocators)
{
    TLSFAllocator tlsfAllocator(4ull * 1024ull * 1024ull);
    RunFragmentationRecoverability("TLSFAllocator", tlsfAllocator, 0xF001u, 2048, 4096, 512 * 1024);

    BinnedAllocator binnedAllocator;
    RunFragmentationRecoverability("BinnedAllocator", binnedAllocator, 0xF002u, 2048, 4096, 256 * 1024);

    BuddyAllocator buddyAllocator(4ull * 1024ull * 1024ull, 256);
    RunFragmentationRecoverability("BuddyAllocator", buddyAllocator, 0xF003u, 2048, 4096, 512 * 1024);
}

TEST(MemoryStress, AllocatorFailureCountersTrackRejectedRequests)
{
    MallocAllocator mallocAllocator;
    ExpectFailureCounterIncrements("MallocAllocator", mallocAllocator, 0, 16);

    BinnedAllocator binnedAllocator;
    ExpectFailureCounterIncrements("BinnedAllocator", binnedAllocator, 0, 16);

    TLSFAllocator tlsfAllocator(1024);
    ExpectFailureCounterIncrements("TLSFAllocator", tlsfAllocator, 2048, 16);

    BuddyAllocator buddyAllocator(1024, 256);
    ExpectFailureCounterIncrements("BuddyAllocator", buddyAllocator, 2048, 16);

    PoolAllocator poolAllocator(64, 2, nullptr, 16);
    void* first = poolAllocator.Allocate(64, 16);
    void* second = poolAllocator.Allocate(64, 16);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ExpectFailureCounterIncrements("PoolAllocator", poolAllocator, 64, 16);
    poolAllocator.Free(second);
    poolAllocator.Free(first);

    MonotonicAllocator monotonicAllocator(mallocAllocator, 64);
    ExpectFailureCounterIncrements(
        "MonotonicAllocator",
        monotonicAllocator,
        (std::numeric_limits<size_t>::max)(),
        16);

    MallocAllocator debugBacking;
    DebugAllocator debugAllocator(&debugBacking);
    ExpectFailureCounterIncrements("DebugAllocator", debugAllocator, 0, 16);

    MallocAllocator trackedBacking;
    TrackedAllocator trackedAllocator(&trackedBacking);
    ExpectFailureCounterIncrements("TrackedAllocator", trackedAllocator, 0, 16);

    MallocAllocator threadSafeBacking;
    ThreadSafeAllocator threadSafeAllocator(&threadSafeBacking);
    ExpectFailureCounterIncrements("ThreadSafeAllocator", threadSafeAllocator, 0, 16);

    TLSCachingAllocator tlsCachingAllocator(&threadSafeAllocator);
    ExpectFailureCounterIncrements("TLSCachingAllocator", tlsCachingAllocator, 0, 16);

    AlignedAllocator alignedAllocator;
    ExpectFailureCounterIncrements("AlignedAllocator", alignedAllocator, 0, 16);

    SecuredAllocator securedAllocator;
    ExpectFailureCounterIncrements("SecuredAllocator", securedAllocator, 0, 16);
}

} // namespace Engine::Memory
