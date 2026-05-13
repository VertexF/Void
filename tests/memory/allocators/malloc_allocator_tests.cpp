#include <Foundation/Memory/Allocators/MallocAllocator.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace Engine::Memory {

TEST(MallocAllocator, OwnsOnlyLiveAllocations)
{
    MallocAllocator allocator;
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));

    void* ptr = allocator.Allocate(64, 32);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 64u);

    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MallocAllocator, RejectsInvalidFreeDoubleFreeAndInvalidReallocate)
{
    MallocAllocator allocator;
    int stackValue = 0;

    const size_t initialFailures = allocator.GetStats().failedAllocationCount;
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(allocator.Reallocate(&stackValue, 128, 16), nullptr);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, initialFailures);
    }

    void* ptr = allocator.Allocate(48, 16);
    ASSERT_NE(ptr, nullptr);
    allocator.Free(ptr);
    const size_t failuresAfterValidFree = allocator.GetStats().failedAllocationCount;
    allocator.Free(ptr);
    if constexpr (kAllocatorDetailedStatsEnabled) {
        EXPECT_GT(allocator.GetStats().failedAllocationCount, failuresAfterValidFree);
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
