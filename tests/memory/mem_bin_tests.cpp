#include <Foundation/Memory/Allocators/BinnedAllocator.hpp>

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

TEST(BinnedAllocator, AllocatesAlignedSmallBlocksAndReusesFreedPages)
{
    BinnedAllocator allocator;

    void* a = allocator.Allocate(24, 16);
    void* b = allocator.Allocate(24, 16);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(IsAligned(a, 16));
    EXPECT_TRUE(IsAligned(b, 16));
    EXPECT_TRUE(allocator.Owns(a));
    EXPECT_TRUE(allocator.Owns(b));
    EXPECT_EQ(allocator.AllocatedSize(), 48u);

    allocator.Free(a);
    allocator.Free(b);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);

    void* c = allocator.Allocate(24, 16);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(IsAligned(c, 16));
    allocator.Free(c);
}

TEST(BinnedAllocator, HandlesLargeAndOverAlignedAllocations)
{
    BinnedAllocator allocator;

    void* large = allocator.Allocate(SizeClassTable::kMaxSize + 4096, 64);
    ASSERT_NE(large, nullptr);
    EXPECT_TRUE(IsAligned(large, 64));
    EXPECT_TRUE(allocator.Owns(large));

    void* overAligned = allocator.Allocate(256, 256);
    ASSERT_NE(overAligned, nullptr);
    EXPECT_TRUE(IsAligned(overAligned, 256));
    EXPECT_TRUE(allocator.Owns(overAligned));

    allocator.Free(large);
    allocator.Free(overAligned);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
