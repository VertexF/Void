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

#include <cstdint>

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

TEST(ArenaAllocators, PoolAllocatorReusesFixedBlocks)
{
    PoolAllocator allocator(128, 4, nullptr, 16);
    int stackValue = 0;
    EXPECT_FALSE(allocator.Owns(&stackValue));
    allocator.Free(&stackValue);

    void* a = allocator.Allocate(64, 16);
    void* b = allocator.Allocate(64, 16);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(allocator.Owns(a));
    EXPECT_FALSE(allocator.Owns(static_cast<uint8*>(a) + 1));
    EXPECT_EQ(allocator.GetAllocatedBlockCount(), 2u);

    void* same = allocator.Reallocate(a, 96, 16);
    EXPECT_EQ(same, a);
    EXPECT_EQ(allocator.Reallocate(a, 256, 16), nullptr);

    allocator.Free(a);
    EXPECT_FALSE(allocator.Owns(a));
    allocator.Free(a);
    allocator.Free(b);
    EXPECT_EQ(allocator.GetAllocatedBlockCount(), 0u);
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
}

} // namespace Engine::Memory
