#include <Foundation/Memory/Allocators/Advanced/BuddyAllocator.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace Engine::Memory {
namespace {

[[nodiscard]] bool IsAligned(void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

} // namespace

TEST(BuddyAllocator, AllocatesAlignedBlocksAndTracksAccounting)
{
    BuddyAllocator allocator(1ull * 1024ull * 1024ull, 256);

    void* a = allocator.Allocate(64, 64);
    void* b = allocator.Allocate(4096, 128);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(IsAligned(a, 64));
    EXPECT_TRUE(IsAligned(b, 128));
    EXPECT_TRUE(allocator.Owns(a));
    EXPECT_TRUE(allocator.Owns(b));
    EXPECT_EQ(allocator.AllocatedSize(), 64u + 4096u);

    allocator.Free(a);
    EXPECT_EQ(allocator.AllocatedSize(), 4096u);
    allocator.Free(b);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(BuddyAllocator, ReallocatePreservesPayload)
{
    BuddyAllocator allocator(1ull * 1024ull * 1024ull, 256);

    EXPECT_EQ(allocator.Allocate(0, 16), nullptr);
    EXPECT_EQ(allocator.Reallocate(nullptr, 0, 16), nullptr);

    auto* values = static_cast<uint32*>(allocator.Allocate(sizeof(uint32) * 4, alignof(uint32)));
    ASSERT_NE(values, nullptr);
    values[0] = 13;
    values[1] = 21;
    values[2] = 34;
    values[3] = 55;

    auto* grown = static_cast<uint32*>(allocator.Reallocate(values, sizeof(uint32) * 64, alignof(uint32)));
    ASSERT_NE(grown, nullptr);
    EXPECT_EQ(grown[0], 13u);
    EXPECT_EQ(grown[1], 21u);
    EXPECT_EQ(grown[2], 34u);
    EXPECT_EQ(grown[3], 55u);
    EXPECT_EQ(allocator.AllocatedSize(), sizeof(uint32) * 64);

    auto* sameBlock = static_cast<uint32*>(allocator.Reallocate(grown, sizeof(uint32) * 32, alignof(uint32)));
    ASSERT_NE(sameBlock, nullptr);
    EXPECT_EQ(sameBlock[0], 13u);
    EXPECT_EQ(allocator.AllocatedSize(), sizeof(uint32) * 32);

    auto* normalizedAlignment = static_cast<uint32*>(allocator.Reallocate(sameBlock, sizeof(uint32) * 16, 24));
    ASSERT_NE(normalizedAlignment, nullptr);
    EXPECT_EQ(normalizedAlignment[0], 13u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(normalizedAlignment) % alignof(MaxAlignT), 0u);
    EXPECT_EQ(allocator.AllocatedSize(), sizeof(uint32) * 16);

    allocator.Free(normalizedAlignment);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(BuddyAllocator, CoalescesFreedBuddiesForLargeAllocation)
{
    BuddyAllocator allocator(1ull * 1024ull * 1024ull, 256);
    std::vector<void*> blocks;
    blocks.reserve(128);

    for (int i = 0; i < 128; ++i) {
        void* ptr = allocator.Allocate(1024, 64);
        ASSERT_NE(ptr, nullptr);
        blocks.push_back(ptr);
    }

    for (void* ptr : blocks) {
        allocator.Free(ptr);
    }
    EXPECT_EQ(allocator.AllocatedSize(), 0u);

    void* large = allocator.Allocate(256ull * 1024ull, 256);
    ASSERT_NE(large, nullptr);
    EXPECT_TRUE(IsAligned(large, 256));
    allocator.Free(large);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(BuddyAllocator, RejectsOversizedAllocation)
{
    BuddyAllocator allocator(64ull * 1024ull, 256);
    EXPECT_EQ(allocator.Allocate(128ull * 1024ull, 16), nullptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(BuddyAllocator, RejectsInvalidAndDoubleFreeWithoutTouchingCounters)
{
    BuddyAllocator allocator(64ull * 1024ull, 256);
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.Reallocate(&stackValue, 64, 16), nullptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);

    void* ptr = allocator.Allocate(512, 64);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(allocator.Owns(ptr));
    allocator.Free(ptr);
    EXPECT_FALSE(allocator.Owns(ptr));
    allocator.Free(ptr);

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
