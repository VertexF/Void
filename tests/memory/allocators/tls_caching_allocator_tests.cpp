#include <Foundation/Memory/Allocators/Advanced/TLSCachingAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace Engine::Memory {
namespace {

[[nodiscard]] bool IsAligned(void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

} // namespace

TEST(TLSCachingAllocator, ReusesMatchingBlocksAndFlushesToBackingAllocator)
{
    MallocAllocator mallocAllocator;
    ThreadSafeAllocator safeAllocator(&mallocAllocator);
    TLSCachingAllocator allocator(&safeAllocator);

    void* first = allocator.Allocate(64, 64);
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(IsAligned(first, 64));
    EXPECT_TRUE(allocator.Owns(first));
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
    EXPECT_TRUE(allocator.Owns(ptr));

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
