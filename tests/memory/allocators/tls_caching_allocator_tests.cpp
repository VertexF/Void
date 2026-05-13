#include <Foundation/Memory/Allocators/Advanced/TLSCachingAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/VirtualMemory.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <thread>

namespace Engine::Memory {
namespace {

[[nodiscard]] bool IsAligned(void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

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

struct TLSHeaderMirror {
    size_t size = 0;
    size_t alignment = 0;
    size_t adjustment = 0;
    uint32 magic = 0;
};

} // namespace

TEST(TLSCachingAllocator, ReusesMatchingBlocksAndFlushesToBackingAllocator)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    TLSCachingAllocator allocator(&safeAllocator);

    void* first = allocator.Allocate(64, 64);
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(IsAligned(first, 64));
    EXPECT_EQ(allocator.Owns(first), kAllocatorOwnershipTrackingEnabled);
    EXPECT_EQ(allocator.GetCacheMisses(), 1u);

    allocator.Free(first);
    EXPECT_FALSE(allocator.Owns(first));

    void* second = allocator.Allocate(64, 64);
    EXPECT_EQ(second, first);
    EXPECT_TRUE(IsAligned(second, 64));
    EXPECT_EQ(allocator.GetCacheHits(), 1u);

    allocator.Free(second);
    allocator.FlushCache();
    EXPECT_EQ(mallocAllocator.AllocatedSize(), 0u);
}

TEST(TLSCachingAllocator, RejectsDoubleFreeAndStaleReallocate)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    TLSCachingAllocator allocator(&safeAllocator);
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.Reallocate(&stackValue, 192, 32), nullptr);

    void* ptr = allocator.Allocate(96, 32);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(allocator.Owns(ptr), kAllocatorOwnershipTrackingEnabled);

    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.Reallocate(ptr, 192, 32), nullptr);
    allocator.Free(ptr);

    void* reused = allocator.Allocate(96, 32);
    EXPECT_EQ(reused, ptr);
    allocator.Free(reused);
    allocator.FlushCache();
    EXPECT_EQ(mallocAllocator.AllocatedSize(), 0u);
}

TEST(TLSCachingAllocator, HeaderCorruptionDoesNotUnregisterLiveAllocation)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    TLSCachingAllocator allocator(&safeAllocator);

    void* ptr = allocator.Allocate(96, 32);
    ASSERT_NE(ptr, nullptr);
    auto* header = reinterpret_cast<TLSHeaderMirror*>(static_cast<uint8*>(ptr) - sizeof(TLSHeaderMirror));
    const uint32 savedMagic = header->magic;

    const size_t failuresBefore = allocator.GetStats().failedAllocationCount;
    header->magic = 0;
    allocator.Free(ptr);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresBefore);
    }
    EXPECT_EQ(allocator.Owns(ptr), kAllocatorOwnershipTrackingEnabled);

    header->magic = savedMagic;
    allocator.Free(ptr);
    allocator.FlushCache();
    EXPECT_EQ(mallocAllocator.AllocatedSize(), 0u);
}

TEST(TLSCachingAllocator, OwnsDoesNotProbeForeignMemory)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    TLSCachingAllocator allocator(&safeAllocator);
    NoAccessPointerProbe probe;
    ASSERT_NE(probe.Pointer(), nullptr);

    EXPECT_FALSE(allocator.Owns(probe.Pointer()));
}

TEST(TLSCachingAllocator, DestructorFlushesCurrentThreadCache)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);

    {
        TLSCachingAllocator allocator(&safeAllocator);
        void* ptr = allocator.Allocate(128, 64);
        ASSERT_NE(ptr, nullptr);
        allocator.Free(ptr);
        EXPECT_NE(mallocAllocator.AllocatedSize(), 0u);
    }

    EXPECT_EQ(mallocAllocator.AllocatedSize(), 0u);
}

TEST(TLSCachingAllocator, ThreadLocalCacheDestructorDoesNotDependOnAllocatorLifetime)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    std::atomic<int> workerState{0};
    std::atomic<bool> releaseWorker{false};

    alignas(TLSCachingAllocator) std::byte storage[sizeof(TLSCachingAllocator)];
    auto* allocator = new (storage) TLSCachingAllocator(&safeAllocator);

    std::thread worker([&]() {
        void* ptr = allocator->Allocate(192, 64);
        if (!ptr || !IsAligned(ptr, 64)) {
            workerState.store(-1, std::memory_order_release);
            return;
        }

        allocator->Free(ptr);
        workerState.store(1, std::memory_order_release);
        while (!releaseWorker.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });

    while (workerState.load(std::memory_order_acquire) == 0) {
        std::this_thread::yield();
    }

    ASSERT_EQ(workerState.load(std::memory_order_acquire), 1);
    EXPECT_NE(mallocAllocator.AllocatedSize(), 0u);

    allocator->~TLSCachingAllocator();
    std::memset(storage, 0xDD, sizeof(storage));

    releaseWorker.store(true, std::memory_order_release);
    worker.join();

    EXPECT_EQ(mallocAllocator.AllocatedSize(), 0u);
}

TEST(TLSCachingAllocator, DoesNotReuseUnderAlignedCachedBlock)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    TLSCachingAllocator allocator(&safeAllocator);

    void* lowAlignment = allocator.Allocate(128, 16);
    ASSERT_NE(lowAlignment, nullptr);
    allocator.Free(lowAlignment);

    void* highAlignment = allocator.Allocate(128, 64);
    ASSERT_NE(highAlignment, nullptr);
    EXPECT_TRUE(IsAligned(highAlignment, 64));
    EXPECT_EQ(allocator.GetCacheHits(), 0u);
    EXPECT_EQ(allocator.GetCacheMisses(), 2u);

    allocator.Free(highAlignment);
    allocator.FlushCache();
    EXPECT_EQ(mallocAllocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
