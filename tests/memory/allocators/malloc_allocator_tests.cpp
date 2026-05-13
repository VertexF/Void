#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/VirtualMemory.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace Engine::Memory {
namespace {

class NoAccessPointerProbe {
public:
    NoAccessPointerProbe()
        : m_vm(CreateVirtualMemory())
    {
        if (m_vm) {
            m_pageSize = m_vm->PageSize();
            m_reserved = m_vm->Reserve(m_pageSize);
        }
    }

    ~NoAccessPointerProbe()
    {
        if (m_vm && m_reserved) {
            m_vm->Release(m_reserved);
        }
    }

    [[nodiscard]] void* Pointer() const noexcept
    {
        return m_reserved ? static_cast<void*>(static_cast<uint8*>(m_reserved) + (m_pageSize / 2u)) : nullptr;
    }

private:
    UniquePtr<IVirtualMemory> m_vm;
    void* m_reserved = nullptr;
    size_t m_pageSize = 0;
};

} // namespace

TEST(MallocAllocator, OwnsOnlyLiveAllocations)
{
    MallocAllocator allocator;
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));

    void* ptr = allocator.Allocate(64, 32);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(allocator.Owns(ptr), kAllocatorOwnershipTrackingEnabled);
    EXPECT_EQ(allocator.AllocatedSize(), 64u);

    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MallocAllocator, OwnsDoesNotProbeForeignMemory)
{
    MallocAllocator allocator;
    NoAccessPointerProbe probe;
    ASSERT_NE(probe.Pointer(), nullptr);

    EXPECT_FALSE(allocator.Owns(probe.Pointer()));
}

TEST(MallocAllocator, RejectsInvalidFreeDoubleFreeAndInvalidReallocate)
{
    MallocAllocator allocator;
    int stackValue = 0;

    const size_t initialFailures = allocator.GetStats().failedAllocationCount;
    if constexpr (kAllocatorOwnershipTrackingEnabled) {
        allocator.Free(&stackValue);
        EXPECT_EQ(allocator.AllocatedSize(), 0u);
        EXPECT_EQ(allocator.Reallocate(&stackValue, 128, 16), nullptr);
        if constexpr (kAllocatorDetailedStatsEnabled) {
            EXPECT_GT(allocator.GetStats().failedAllocationCount, initialFailures);
        }
    } else {
        EXPECT_FALSE(allocator.Owns(&stackValue));
        EXPECT_EQ(allocator.GetStats().failedAllocationCount, initialFailures);
    }

    void* ptr = allocator.Allocate(48, 16);
    ASSERT_NE(ptr, nullptr);
    allocator.Free(ptr);
    const size_t failuresAfterValidFree = allocator.GetStats().failedAllocationCount;
    if constexpr (kAllocatorOwnershipTrackingEnabled) {
        allocator.Free(ptr);
        if constexpr (kAllocatorDetailedStatsEnabled) {
            EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresAfterValidFree);
        }
    } else {
        EXPECT_FALSE(allocator.Owns(ptr));
        EXPECT_EQ(allocator.GetStats().failedAllocationCount, failuresAfterValidFree);
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MallocAllocator, HeaderCorruptionDoesNotUnregisterLiveAllocation)
{
    MallocAllocator allocator;

    void* ptr = allocator.Allocate(64, 16);
    ASSERT_NE(ptr, nullptr);
    auto* header = reinterpret_cast<AllocationHeader*>(static_cast<uint8*>(ptr) - sizeof(AllocationHeader));
    const size_t savedPadding = header->padding;

    const size_t failuresBefore = allocator.GetStats().failedAllocationCount;
    header->padding = 0;
    allocator.Free(ptr);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresBefore);
    }
    EXPECT_EQ(allocator.Owns(ptr), kAllocatorOwnershipTrackingEnabled);
    EXPECT_EQ(allocator.AllocatedSize(), 64u);

    header->padding = savedPadding;
    allocator.Free(ptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MallocAllocator, ConcurrentStatsRemainBalanced)
{
    MallocAllocator allocator;
    constexpr size_t kThreadCount = 8;
    constexpr size_t kAllocationsPerThread = 512;
    std::vector<void*> pointers(kThreadCount * kAllocationsPerThread, nullptr);

    for (void*& ptr : pointers) {
        ptr = allocator.Allocate(32, 16);
        ASSERT_NE(ptr, nullptr);
    }

    std::atomic<size_t> nextIndex{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (size_t threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([&] {
            while (true) {
                const size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
                if (index >= pointers.size()) {
                    return;
                }
                allocator.Free(pointers[index]);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    const AllocatorStats stats = allocator.GetStats();
    EXPECT_EQ(stats.liveBytes, 0u);
    EXPECT_EQ(stats.liveAllocationCount, 0u);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_EQ(stats.freeCount, pointers.size());
    }
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
