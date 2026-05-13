#include <Foundation/Memory/Allocators/BinnedAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace Engine::Memory {

TEST(BinnedAllocatorDetails, ReallocatePreservesPayloadAndAccounting)
{
    MallocAllocator backing;
    BinnedAllocator allocator(&backing);

    auto* values = static_cast<uint32*>(allocator.Allocate(sizeof(uint32) * 4, alignof(uint32)));
    ASSERT_NE(values, nullptr);
    values[0] = 3;
    values[1] = 5;
    values[2] = 7;
    values[3] = 11;

    auto* grown = static_cast<uint32*>(allocator.Reallocate(values, sizeof(uint32) * 16, alignof(uint32)));
    ASSERT_NE(grown, nullptr);
    EXPECT_EQ(grown[0], 3u);
    EXPECT_EQ(grown[1], 5u);
    EXPECT_EQ(grown[2], 7u);
    EXPECT_EQ(grown[3], 11u);
    EXPECT_EQ(allocator.AllocatedSize(), sizeof(uint32) * 16);

    auto* shrunk = static_cast<uint32*>(allocator.Reallocate(grown, sizeof(uint32) * 2, alignof(uint32)));
    ASSERT_EQ(shrunk, grown);
    EXPECT_EQ(shrunk[0], 3u);
    EXPECT_EQ(shrunk[1], 5u);
    EXPECT_EQ(allocator.AllocatedSize(), sizeof(uint32) * 2);

    allocator.Free(shrunk);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(BinnedAllocatorDetails, RejectsInvalidAndDoubleFreeForSmallAndLargeAllocations)
{
    BinnedAllocator allocator;
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);
    EXPECT_EQ(allocator.Reallocate(&stackValue, 128, 16), nullptr);

    void* smallBlock = allocator.Allocate(64, 16);
    ASSERT_NE(smallBlock, nullptr);
    EXPECT_TRUE(allocator.Owns(smallBlock));
    allocator.Free(smallBlock);
    EXPECT_FALSE(allocator.Owns(smallBlock));
    allocator.Free(smallBlock);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);

    void* largeBlock = allocator.Allocate(SizeClassTable::kMaxSize + 1024, 64);
    ASSERT_NE(largeBlock, nullptr);
    EXPECT_TRUE(allocator.Owns(largeBlock));
    allocator.Free(largeBlock);
    EXPECT_FALSE(allocator.Owns(largeBlock));
    allocator.Free(largeBlock);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(BinnedAllocatorDetails, SustainsMixedSmallAllocationStress)
{
    BinnedAllocator allocator;
    std::vector<void*> allocations;
    allocations.reserve(2048);

    for (size_t i = 0; i < 2048; ++i) {
        const size_t size = 1 + ((i * 37) % 2048);
        void* ptr = allocator.Allocate(size, 32);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 32u, 0u);
        allocations.push_back(ptr);
    }

    for (size_t i = 0; i < allocations.size(); i += 2) {
        allocator.Free(allocations[i]);
        allocations[i] = nullptr;
    }
    for (size_t i = 0; i < 1024; ++i) {
        void* ptr = allocator.Allocate(64 + (i % 512), 16);
        ASSERT_NE(ptr, nullptr);
        allocator.Free(ptr);
    }
    for (void* ptr : allocations) {
        allocator.Free(ptr);
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
