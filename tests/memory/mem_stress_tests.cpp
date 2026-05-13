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
#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <thread>
#include <unordered_map>

namespace Engine::Memory {
namespace {

struct TortureAllocation {
    void* ptr = nullptr;
    size_t size = 0;
    uint8 pattern = 0;
};

[[nodiscard]] int MemoryStressMultiplier() noexcept
{
    const char* value = std::getenv("VOID_MEMORY_STRESS_MULTIPLIER");
    if (!value || *value == '\0') {
        return 1;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || parsed == 0) {
        return 1;
    }

    return parsed > 64ul ? 64 : static_cast<int>(parsed);
}

[[nodiscard]] int ScaledIterations(int base) noexcept
{
    const int multiplier = MemoryStressMultiplier();
    if (base > (std::numeric_limits<int>::max)() / multiplier) {
        return (std::numeric_limits<int>::max)();
    }
    return base * multiplier;
}

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

bool PatternPrefixMatches(const TortureAllocation& allocation, size_t count) noexcept
{
    const auto* bytes = static_cast<const uint8*>(allocation.ptr);
    for (size_t i = 0; i < count; ++i) {
        if (bytes[i] != static_cast<uint8>(allocation.pattern + static_cast<uint8>(i * 13u))) {
            return false;
        }
    }
    return true;
}

void RunAllocatorTorture(
    const char* name,
    IAllocator& allocator,
    uint32 seed,
    size_t maxSize,
    bool freeWithReallocateZero = false,
    int iterations = 1200,
    size_t liveReserve = 256)
{
    SCOPED_TRACE(name);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> sizeDist(1, maxSize);
    std::uniform_int_distribution<int> actionDist(0, 99);
    std::uniform_int_distribution<int> alignmentDist(0, 3);
    Vector<TortureAllocation> live;
    live.reserve(liveReserve);

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

    for (int iteration = 0; iteration < iterations; ++iteration) {
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
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(stats.allocationCount, 0u);
    }
}

class FaultInjectionAllocator final : public IAllocator {
public:
    explicit FaultInjectionAllocator(size_t successfulAllocationsBeforeFailure)
        : m_successfulAllocationsBeforeFailure(successfulAllocationsBeforeFailure)
    {}

    void* Allocate(size_t size, size_t alignment) override
    {
        if (m_successfulAllocations >= m_successfulAllocationsBeforeFailure) {
            ++m_failedAllocations;
            return nullptr;
        }

        void* ptr = m_upstream.Allocate(size, alignment);
        if (!ptr) {
            ++m_failedAllocations;
            return nullptr;
        }

        ++m_successfulAllocations;
        m_liveBytes += size;
        m_liveAllocations[ptr] = size;
        return ptr;
    }

    void* Reallocate(void* ptr, size_t newSize, size_t alignment) override
    {
        if (!ptr) {
            return Allocate(newSize, alignment);
        }
        if (newSize == 0) {
            Free(ptr);
            return nullptr;
        }

        const auto it = m_liveAllocations.find(ptr);
        if (it == m_liveAllocations.end()) {
            ++m_failedAllocations;
            return nullptr;
        }

        const size_t oldSize = it->second;
        void* replacement = Allocate(newSize, alignment);
        if (!replacement) {
            return nullptr;
        }

        MemCopy(replacement, ptr, oldSize < newSize ? oldSize : newSize);
        Free(ptr);
        return replacement;
    }

    void Free(void* ptr) override
    {
        if (!ptr) {
            return;
        }

        const auto it = m_liveAllocations.find(ptr);
        if (it != m_liveAllocations.end()) {
            m_liveBytes -= it->second;
            m_liveAllocations.erase(it);
        }
        m_upstream.Free(ptr);
    }

    size_t AllocatedSize() const override { return m_liveBytes; }
    const char* Name() const override { return "FaultInjectionAllocator"; }
    bool Owns(void* ptr) const override { return m_liveAllocations.find(ptr) != m_liveAllocations.end(); }

    [[nodiscard]] size_t GetFailedAllocationCount() const noexcept { return m_failedAllocations; }
    [[nodiscard]] size_t GetSuccessfulAllocationCount() const noexcept { return m_successfulAllocations; }

private:
    MallocAllocator m_upstream;
    std::unordered_map<void*, size_t> m_liveAllocations;
    size_t m_successfulAllocationsBeforeFailure = 0;
    size_t m_successfulAllocations = 0;
    size_t m_failedAllocations = 0;
    size_t m_liveBytes = 0;
};

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

void RunConcurrentAllocatorTorture(
    const char* name,
    IAllocator& allocator,
    uint32 seed,
    size_t maxSize,
    int threadCount = 8,
    int iterations = 2000,
    size_t liveReserve = 256)
{
    SCOPED_TRACE(name);
    std::atomic<size_t> corruptions{0};
    std::atomic<size_t> successfulAllocations{0};
    Vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(threadCount));

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        threads.emplace_back([&, threadIndex] {
            std::mt19937 rng(seed + static_cast<uint32>(threadIndex * 0x9E37u));
            std::uniform_int_distribution<size_t> sizeDist(1, maxSize);
            std::uniform_int_distribution<int> actionDist(0, 99);
            std::uniform_int_distribution<int> alignmentDist(0, 3);
            Vector<TortureAllocation> live;
            live.reserve(liveReserve);

            auto nextAlignment = [&]() -> size_t {
                switch (alignmentDist(rng)) {
                    case 0: return 8;
                    case 1: return 16;
                    case 2: return 32;
                    default: return 24;
                }
            };

            for (int iteration = 0; iteration < iterations; ++iteration) {
                const int action = actionDist(rng);
                if (live.empty() || action < 55) {
                    TortureAllocation allocation{};
                    allocation.size = sizeDist(rng);
                    allocation.pattern = static_cast<uint8>((iteration + threadIndex * 37) & 0xFF);
                    allocation.ptr = allocator.Allocate(allocation.size, nextAlignment());
                    if (allocation.ptr) {
                        FillPattern(allocation);
                        live.push_back(allocation);
                        successfulAllocations.fetch_add(1, std::memory_order_relaxed);
                    }
                    continue;
                }

                const size_t index = static_cast<size_t>(rng()) % live.size();
                TortureAllocation allocation = live[index];
                if (!PatternPrefixMatches(allocation, allocation.size)) {
                    corruptions.fetch_add(1, std::memory_order_relaxed);
                }

                if (action < 80) {
                    allocator.Free(allocation.ptr);
                    live[index] = live.back();
                    live.pop_back();
                    continue;
                }

                const size_t newSize = sizeDist(rng);
                void* reallocated = allocator.Reallocate(allocation.ptr, newSize, nextAlignment());
                if (!reallocated) {
                    if (!PatternPrefixMatches(allocation, allocation.size)) {
                        corruptions.fetch_add(1, std::memory_order_relaxed);
                    }
                    continue;
                }

                allocation.ptr = reallocated;
                if (!PatternPrefixMatches(allocation, allocation.size < newSize ? allocation.size : newSize)) {
                    corruptions.fetch_add(1, std::memory_order_relaxed);
                }
                allocation.size = newSize;
                allocation.pattern = static_cast<uint8>((iteration * 17 + threadIndex * 13) & 0xFF);
                FillPattern(allocation);
                live[index] = allocation;
            }

            for (TortureAllocation allocation : live) {
                if (!PatternPrefixMatches(allocation, allocation.size)) {
                    corruptions.fetch_add(1, std::memory_order_relaxed);
                }
                allocator.Free(allocation.ptr);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }
    if (auto* tlsCache = dynamic_cast<TLSCachingAllocator*>(&allocator)) {
        tlsCache->FlushCache();
    }

    EXPECT_EQ(corruptions.load(std::memory_order_relaxed), 0u);
    EXPECT_GT(successfulAllocations.load(std::memory_order_relaxed), 0u);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

void RunCrossThreadFreeSoak(
    const char* name,
    IAllocator& allocator,
    uint32 seed,
    size_t maxSize,
    int threadCount = 8,
    size_t allocationsPerThread = 256,
    int rounds = 4)
{
    SCOPED_TRACE(name);
    const int scaledRounds = ScaledIterations(rounds);
    const size_t totalAllocations = static_cast<size_t>(threadCount) * allocationsPerThread;
    std::atomic<size_t> corruptions{0};
    std::atomic<size_t> successfulAllocations{0};
    std::atomic<size_t> successfulReallocations{0};

    for (int round = 0; round < scaledRounds; ++round) {
        Vector<TortureAllocation> allocations;
        allocations.Resize(totalAllocations);

        std::atomic<int> producersReady{0};
        std::atomic<bool> startProducers{false};
        Vector<std::thread> producers;
        producers.reserve(static_cast<size_t>(threadCount));

        for (int producerIndex = 0; producerIndex < threadCount; ++producerIndex) {
            producers.emplace_back([&, producerIndex] {
                std::mt19937 rng(seed +
                    static_cast<uint32>(round * 0x1F123BB5u) +
                    static_cast<uint32>(producerIndex * 0x9E3779B9u));
                std::uniform_int_distribution<size_t> sizeDist(1, maxSize);
                std::uniform_int_distribution<int> alignmentDist(0, 3);

                auto nextAlignment = [&]() -> size_t {
                    switch (alignmentDist(rng)) {
                        case 0: return 8;
                        case 1: return 16;
                        case 2: return 32;
                        default: return 24;
                    }
                };

                producersReady.fetch_add(1, std::memory_order_release);
                while (!startProducers.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                const size_t base = static_cast<size_t>(producerIndex) * allocationsPerThread;
                for (size_t slot = 0; slot < allocationsPerThread; ++slot) {
                    TortureAllocation allocation{};
                    allocation.size = sizeDist(rng);
                    allocation.pattern = static_cast<uint8>((round * 31 + producerIndex * 17 + static_cast<int>(slot)) & 0xFF);
                    allocation.ptr = allocator.Allocate(allocation.size, nextAlignment());
                    if (allocation.ptr) {
                        FillPattern(allocation);
                        successfulAllocations.fetch_add(1, std::memory_order_relaxed);
                    }
                    allocations[base + slot] = allocation;
                }
            });
        }

        while (producersReady.load(std::memory_order_acquire) != threadCount) {
            std::this_thread::yield();
        }
        startProducers.store(true, std::memory_order_release);

        for (std::thread& producer : producers) {
            producer.join();
        }

        std::atomic<int> consumersReady{0};
        std::atomic<bool> startConsumers{false};
        Vector<std::thread> consumers;
        consumers.reserve(static_cast<size_t>(threadCount));

        for (int consumerIndex = 0; consumerIndex < threadCount; ++consumerIndex) {
            consumers.emplace_back([&, consumerIndex] {
                std::mt19937 rng(seed ^
                    static_cast<uint32>(round * 0x85EBCA6Bu) ^
                    static_cast<uint32>(consumerIndex * 0xC2B2AE35u));
                std::uniform_int_distribution<size_t> sizeDist(1, maxSize);
                std::uniform_int_distribution<int> alignmentDist(0, 3);

                auto nextAlignment = [&]() -> size_t {
                    switch (alignmentDist(rng)) {
                        case 0: return 8;
                        case 1: return 16;
                        case 2: return 32;
                        default: return 24;
                    }
                };

                consumersReady.fetch_add(1, std::memory_order_release);
                while (!startConsumers.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                const int producerIndex = (consumerIndex + 1) % threadCount;
                const size_t base = static_cast<size_t>(producerIndex) * allocationsPerThread;
                for (size_t slot = 0; slot < allocationsPerThread; ++slot) {
                    TortureAllocation allocation = allocations[base + slot];
                    if (!allocation.ptr) {
                        continue;
                    }

                    if (!PatternPrefixMatches(allocation, allocation.size)) {
                        corruptions.fetch_add(1, std::memory_order_relaxed);
                    }

                    if (((slot + static_cast<size_t>(round) + static_cast<size_t>(consumerIndex)) % 4u) == 0u) {
                        const size_t newSize = sizeDist(rng);
                        void* reallocated = allocator.Reallocate(allocation.ptr, newSize, nextAlignment());
                        if (reallocated) {
                            allocation.ptr = reallocated;
                            if (!PatternPrefixMatches(allocation, allocation.size < newSize ? allocation.size : newSize)) {
                                corruptions.fetch_add(1, std::memory_order_relaxed);
                            }
                            allocation.size = newSize;
                            allocation.pattern = static_cast<uint8>((round * 43 + consumerIndex * 23 + static_cast<int>(slot)) & 0xFF);
                            FillPattern(allocation);
                            successfulReallocations.fetch_add(1, std::memory_order_relaxed);
                        } else if (!PatternPrefixMatches(allocation, allocation.size)) {
                            corruptions.fetch_add(1, std::memory_order_relaxed);
                        }
                    }

                    if (!PatternPrefixMatches(allocation, allocation.size)) {
                        corruptions.fetch_add(1, std::memory_order_relaxed);
                    }
                    allocator.Free(allocation.ptr);
                    allocations[base + slot] = {};
                }
            });
        }

        while (consumersReady.load(std::memory_order_acquire) != threadCount) {
            std::this_thread::yield();
        }
        startConsumers.store(true, std::memory_order_release);

        for (std::thread& consumer : consumers) {
            consumer.join();
        }

        if (auto* tlsCache = dynamic_cast<TLSCachingAllocator*>(&allocator)) {
            tlsCache->FlushCache();
        }

        EXPECT_EQ(allocator.AllocatedSize(), 0u);
    }

    EXPECT_EQ(corruptions.load(std::memory_order_relaxed), 0u);
    EXPECT_GT(successfulAllocations.load(std::memory_order_relaxed), 0u);
    EXPECT_GT(successfulReallocations.load(std::memory_order_relaxed), 0u);
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

TEST(MemoryStress, LongDeterministicAllocatorTorturePresets)
{
    const int kIterations = ScaledIterations(5000);
    constexpr size_t kLiveReserve = 1024;

    MallocAllocator mallocAllocator;
    RunAllocatorTorture("MallocAllocator/Long", mallocAllocator, 0xA001u, 1024, false, kIterations, kLiveReserve);

    BinnedAllocator binnedAllocator;
    RunAllocatorTorture("BinnedAllocator/Long", binnedAllocator, 0xA002u, 2048, false, kIterations, kLiveReserve);

    TLSFAllocator tlsfAllocator(8ull * 1024ull * 1024ull);
    RunAllocatorTorture("TLSFAllocator/Long", tlsfAllocator, 0xA003u, 1024, false, kIterations, kLiveReserve);

    BuddyAllocator buddyAllocator(8ull * 1024ull * 1024ull, 256);
    RunAllocatorTorture("BuddyAllocator/Long", buddyAllocator, 0xA004u, 1024, false, kIterations, kLiveReserve);

    PoolAllocator poolAllocator(256, 8192, nullptr, 16);
    RunAllocatorTorture("PoolAllocator/Long", poolAllocator, 0xA005u, 256, false, kIterations, kLiveReserve);

    MallocAllocator monotonicBacking;
    MonotonicAllocator monotonicAllocator(monotonicBacking, 4ull * 1024ull * 1024ull);
    RunAllocatorTorture("MonotonicAllocator/Long", monotonicAllocator, 0xA006u, 256, true, kIterations, kLiveReserve);

    MallocAllocator debugBacking;
    DebugAllocator debugAllocator(&debugBacking);
    RunAllocatorTorture("DebugAllocator/Long", debugAllocator, 0xA007u, 1024, false, kIterations, kLiveReserve);

    MallocAllocator trackedBacking;
    TrackedAllocator trackedAllocator(&trackedBacking);
    RunAllocatorTorture("TrackedAllocator/Long", trackedAllocator, 0xA008u, 1024, false, kIterations, kLiveReserve);

    MallocAllocator tlsBacking;
    ThreadSafeAllocator threadSafeAllocator(&tlsBacking);
    TLSCachingAllocator tlsCachingAllocator(&threadSafeAllocator);
    RunAllocatorTorture("TLSCachingAllocator/Long", tlsCachingAllocator, 0xA009u, 1024, false, kIterations, kLiveReserve);
}

TEST(MemoryStress, ConcurrentAllocatorTortureForSharedAllocators)
{
    const int kIterations = ScaledIterations(2000);

    BinnedAllocator binnedAllocator;
    RunConcurrentAllocatorTorture("BinnedAllocator/Concurrent", binnedAllocator, 0xB001u, 1024, 8, kIterations);

    TLSFAllocator tlsfAllocator(8ull * 1024ull * 1024ull);
    RunConcurrentAllocatorTorture("TLSFAllocator/Concurrent", tlsfAllocator, 0xB002u, 1024, 8, kIterations);

    BuddyAllocator buddyAllocator(8ull * 1024ull * 1024ull, 256);
    RunConcurrentAllocatorTorture("BuddyAllocator/Concurrent", buddyAllocator, 0xB003u, 1024, 8, kIterations);

    PoolAllocator poolAllocator(256, 8192, nullptr, 16);
    RunConcurrentAllocatorTorture("PoolAllocator/Concurrent", poolAllocator, 0xB004u, 256, 8, kIterations);

    MallocAllocator tlsBacking;
    ThreadSafeAllocator threadSafeBacking(&tlsBacking);
    TLSCachingAllocator tlsCachingAllocator(&threadSafeBacking);
    RunConcurrentAllocatorTorture("TLSCachingAllocator/Concurrent", tlsCachingAllocator, 0xB005u, 1024, 8, kIterations);
    EXPECT_EQ(tlsBacking.AllocatedSize(), 0u);
}

TEST(MemoryStress, CrossThreadFreeSoakForSharedAllocators)
{
    MallocAllocator mallocAllocator;
    RunCrossThreadFreeSoak("MallocAllocator/CrossThreadFree", mallocAllocator, 0xC001u, 1024);

    BinnedAllocator binnedAllocator;
    RunCrossThreadFreeSoak("BinnedAllocator/CrossThreadFree", binnedAllocator, 0xC002u, 1024);

    TLSFAllocator tlsfAllocator(8ull * 1024ull * 1024ull);
    RunCrossThreadFreeSoak("TLSFAllocator/CrossThreadFree", tlsfAllocator, 0xC003u, 1024);

    BuddyAllocator buddyAllocator(8ull * 1024ull * 1024ull, 256);
    RunCrossThreadFreeSoak("BuddyAllocator/CrossThreadFree", buddyAllocator, 0xC004u, 1024);

    PoolAllocator poolAllocator(256, 8192, nullptr, 16);
    RunCrossThreadFreeSoak("PoolAllocator/CrossThreadFree", poolAllocator, 0xC005u, 256);

    MallocAllocator trackedBacking;
    TrackedAllocator trackedAllocator(&trackedBacking);
    RunCrossThreadFreeSoak("TrackedAllocator/CrossThreadFree", trackedAllocator, 0xC006u, 1024);
    EXPECT_EQ(trackedBacking.AllocatedSize(), 0u);

    MallocAllocator debugBacking;
    DebugAllocator debugAllocator(&debugBacking);
    RunCrossThreadFreeSoak("DebugAllocator/CrossThreadFree", debugAllocator, 0xC007u, 1024);
    EXPECT_EQ(debugBacking.AllocatedSize(), 0u);

    MallocAllocator threadSafeBacking;
    ThreadSafeAllocator threadSafeAllocator(&threadSafeBacking);
    RunCrossThreadFreeSoak("ThreadSafeAllocator/CrossThreadFree", threadSafeAllocator, 0xC008u, 1024);
    EXPECT_EQ(threadSafeBacking.AllocatedSize(), 0u);

    MallocAllocator tlsBacking;
    ThreadSafeAllocator tlsThreadSafeBacking(&tlsBacking);
    TLSCachingAllocator tlsCachingAllocator(&tlsThreadSafeBacking);
    RunCrossThreadFreeSoak("TLSCachingAllocator/CrossThreadFree", tlsCachingAllocator, 0xC009u, 1024);
    EXPECT_EQ(tlsBacking.AllocatedSize(), 0u);
}

TEST(MemoryStress, FaultInjectedBackingAllocatorsRecoverWithoutLeaking)
{
    {
        FaultInjectionAllocator backing(1);
        BinnedAllocator allocator(&backing);
        EXPECT_EQ(allocator.Allocate(64, 16), nullptr);
        EXPECT_EQ(backing.AllocatedSize(), 0u);
        EXPECT_EQ(backing.GetSuccessfulAllocationCount(), 1u);
        EXPECT_GT(backing.GetFailedAllocationCount(), 0u);
    }

    {
        FaultInjectionAllocator backing(1);
        PoolAllocator allocator(64, 16, &backing, 16);
        EXPECT_EQ(allocator.Allocate(64, 16), nullptr);
        EXPECT_EQ(allocator.GetBlockCount(), 0u);
        EXPECT_EQ(backing.AllocatedSize(), 0u);
        EXPECT_EQ(backing.GetSuccessfulAllocationCount(), 1u);
        EXPECT_GT(backing.GetFailedAllocationCount(), 0u);
    }

    {
        FaultInjectionAllocator backing(0);
        BuddyAllocator allocator(1024, 256, &backing);
        EXPECT_EQ(allocator.Allocate(64, 16), nullptr);
        EXPECT_EQ(allocator.GetTotalSize(), 0u);
        EXPECT_EQ(backing.AllocatedSize(), 0u);
        EXPECT_GT(backing.GetFailedAllocationCount(), 0u);
    }

    {
        FaultInjectionAllocator backing(0);
        TLSCachingAllocator allocator(&backing);
        EXPECT_EQ(allocator.Allocate(64, 16), nullptr);
        allocator.FlushCache();
        EXPECT_EQ(backing.AllocatedSize(), 0u);
        EXPECT_GT(backing.GetFailedAllocationCount(), 0u);
    }

    {
        FaultInjectionAllocator backing(0);
        AlignedAllocator allocator(64, &backing);
        EXPECT_EQ(allocator.Allocate(64, 64), nullptr);
        EXPECT_EQ(backing.AllocatedSize(), 0u);
        EXPECT_GT(backing.GetFailedAllocationCount(), 0u);
    }

    {
        FaultInjectionAllocator backing(0);
        MonotonicAllocator allocator(backing, 0);
        EXPECT_EQ(allocator.Allocate(64, 16), nullptr);
        allocator.Reset();
        EXPECT_EQ(backing.AllocatedSize(), 0u);
        EXPECT_GT(backing.GetFailedAllocationCount(), 0u);
    }
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
