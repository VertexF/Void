#include <Foundation/Memory/Allocators/Advanced/SecuredAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Allocators/MonotonicAllocator.hpp>
#include <Foundation/Memory/Binned/CoreHeap.hpp>
#include <Foundation/Memory/Binned/PageHeap.hpp>
#include <Foundation/Memory/VirtualMemory.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace Engine::Memory {
namespace {

[[nodiscard]] bool IsAligned(void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

} // namespace

TEST(SecuredAllocator, AllocatesOverAlignedGuardedMemoryAndTracksOwnership)
{
    SecuredAllocator allocator;

    void* ptr = allocator.Allocate(257, 4096);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsAligned(ptr, 4096));
    EXPECT_TRUE(allocator.Owns(ptr));
    EXPECT_EQ(allocator.AllocatedSize(), 257u);

    auto* bytes = static_cast<uint8*>(ptr);
    bytes[0] = 0x11u;
    bytes[256] = 0xEEu;
    EXPECT_EQ(bytes[0], 0x11u);
    EXPECT_EQ(bytes[256], 0xEEu);

    allocator.MakeReadOnly(ptr);
    allocator.MakeReadWrite(ptr);
    bytes[128] = 0x7Au;
    EXPECT_EQ(bytes[128], 0x7Au);

    allocator.Free(ptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(SecuredAllocator, ReallocateMovesAndPreservesPayload)
{
    SecuredAllocator allocator;

    auto* ptr = static_cast<uint8*>(allocator.Allocate(32, 64));
    ASSERT_NE(ptr, nullptr);
    for (uint32 i = 0; i < 32; ++i) {
        ptr[i] = static_cast<uint8>(i);
    }

    auto* grown = static_cast<uint8*>(allocator.Reallocate(ptr, 128, 128));
    ASSERT_NE(grown, nullptr);
    EXPECT_TRUE(IsAligned(grown, 128));
    EXPECT_FALSE(allocator.Owns(ptr));
    EXPECT_TRUE(allocator.Owns(grown));

    for (uint32 i = 0; i < 32; ++i) {
        EXPECT_EQ(grown[i], static_cast<uint8>(i));
    }
    EXPECT_EQ(allocator.AllocatedSize(), 128u);

    allocator.MakeReadOnly(grown);
    allocator.ScrubAndFree(grown);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(SecuredAllocator, RejectsInvalidInputsWithoutTouchingCounters)
{
    SecuredAllocator allocator;
    int stackValue = 0;

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));
    EXPECT_EQ(allocator.Allocate(0, 64), nullptr);
    EXPECT_EQ(allocator.Reallocate(nullptr, 0, 64), nullptr);

    allocator.Free(&stackValue);
    allocator.MakeReadOnly(&stackValue);
    allocator.MakeReadWrite(&stackValue);
    allocator.ScrubAndFree(&stackValue);
    EXPECT_EQ(allocator.Reallocate(&stackValue, 64, 16), nullptr);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);

    void* normalized = allocator.Allocate(96, 24);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(normalized) % alignof(MaxAlignT), 0u);
    allocator.Free(normalized);
    allocator.Free(normalized);
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MonotonicAllocator, UserBufferResetsAndReallocatePreservesPayload)
{
    MallocAllocator upstream;
    alignas(64) uint8 buffer[512]{};
    MonotonicAllocator allocator(buffer, sizeof(buffer), upstream);

    auto* values = static_cast<uint32*>(allocator.Allocate(sizeof(uint32) * 4, alignof(uint32)));
    ASSERT_NE(values, nullptr);
    EXPECT_TRUE(allocator.Owns(values));
    values[0] = 3;
    values[1] = 5;
    values[2] = 8;
    values[3] = 13;

    auto* grown = static_cast<uint32*>(allocator.Reallocate(values, sizeof(uint32) * 8, alignof(uint32)));
    ASSERT_NE(grown, nullptr);
    EXPECT_FALSE(allocator.Owns(values));
    EXPECT_TRUE(allocator.Owns(grown));
    EXPECT_EQ(grown[0], 3u);
    EXPECT_EQ(grown[1], 5u);
    EXPECT_EQ(grown[2], 8u);
    EXPECT_EQ(grown[3], 13u);
    EXPECT_EQ(allocator.AllocatedSize(), sizeof(uint32) * 8);

    allocator.Reset();
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_FALSE(allocator.Owns(grown));
    EXPECT_EQ(allocator.Reallocate(grown, sizeof(uint32) * 16, alignof(uint32)), nullptr);

    void* reused = allocator.Allocate(64, 64);
    ASSERT_NE(reused, nullptr);
    EXPECT_TRUE(IsAligned(reused, 64));
    EXPECT_TRUE(allocator.Owns(reused));
}

TEST(MonotonicAllocator, OverflowBlocksReturnToUpstreamOnReset)
{
    MallocAllocator upstream;
    alignas(16) uint8 buffer[128]{};
    MonotonicAllocator allocator(buffer, sizeof(buffer), upstream);

    void* large = allocator.Allocate(1024, 16);
    ASSERT_NE(large, nullptr);
    EXPECT_TRUE(allocator.Owns(large));
    EXPECT_GT(upstream.AllocatedSize(), 0u);

    allocator.Reset();
    EXPECT_EQ(allocator.AllocatedSize(), 0u);
    EXPECT_EQ(upstream.AllocatedSize(), 0u);
    EXPECT_FALSE(allocator.Owns(large));
}

TEST(MonotonicAllocator, OwnsOnlyLiveAllocationStarts)
{
    MallocAllocator upstream;
    alignas(64) uint8 buffer[256]{};
    MonotonicAllocator allocator(buffer, sizeof(buffer), upstream);
    int stackValue = 0;

    auto* bytes = static_cast<uint8*>(allocator.Allocate(64, 16));
    ASSERT_NE(bytes, nullptr);

    EXPECT_FALSE(allocator.Owns(nullptr));
    EXPECT_FALSE(allocator.Owns(&stackValue));
    EXPECT_TRUE(allocator.Owns(bytes));
    EXPECT_FALSE(allocator.Owns(bytes + 1));

    allocator.Free(bytes);
    EXPECT_TRUE(allocator.Owns(bytes));

    allocator.Reset();
    EXPECT_FALSE(allocator.Owns(bytes));
}

TEST(PageHeap, AllocatesCommitsAndRecyclesPages)
{
    UniquePtr<IVirtualMemory> vm = CreateVirtualMemory();
    ASSERT_TRUE(vm);

    const size_t pageSize = 64ull * 1024ull;
    const size_t reservedSize = pageSize * 4;
    void* base = vm->Reserve(reservedSize);
    ASSERT_NE(base, nullptr);

    PageHeap pageHeap;
    pageHeap.Initialize(vm.Get(), base, reservedSize);

    void* first = pageHeap.AllocatePage(pageSize);
    ASSERT_NE(first, nullptr);
    auto* bytes = static_cast<uint8*>(first);
    bytes[0] = 0x44u;
    bytes[pageSize - 1] = 0x55u;
    EXPECT_EQ(bytes[0], 0x44u);
    EXPECT_EQ(bytes[pageSize - 1], 0x55u);

    pageHeap.FreePage(first, pageSize);
    void* recycled = pageHeap.AllocatePage(pageSize);
    EXPECT_EQ(recycled, first);

    vm->Release(base);
}

TEST(PageHeap, FailedOversizedAllocationDoesNotConsumeReservedSpace)
{
    UniquePtr<IVirtualMemory> vm = CreateVirtualMemory();
    ASSERT_TRUE(vm);

    const size_t pageSize = 64ull * 1024ull;
    const size_t reservedSize = pageSize * 2;
    void* base = vm->Reserve(reservedSize);
    ASSERT_NE(base, nullptr);

    PageHeap pageHeap;
    pageHeap.Initialize(vm.Get(), base, reservedSize);

    EXPECT_EQ(pageHeap.AllocatePage(0), nullptr);
    EXPECT_EQ(pageHeap.AllocatePage(reservedSize + pageSize), nullptr);

    void* first = pageHeap.AllocatePage(pageSize);
    void* second = pageHeap.AllocatePage(pageSize);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_EQ(pageHeap.AllocatePage(pageSize), nullptr);

    vm->Release(base);
}

TEST(CoreHeap, CachesPagesBeforeReturningToPageHeap)
{
    UniquePtr<IVirtualMemory> vm = CreateVirtualMemory();
    ASSERT_TRUE(vm);

    const size_t pageSize = 64ull * 1024ull;
    const size_t reservedSize = pageSize * 4;
    void* base = vm->Reserve(reservedSize);
    ASSERT_NE(base, nullptr);

    PageHeap pageHeap;
    pageHeap.Initialize(vm.Get(), base, reservedSize);

    CoreHeap coreHeap;
    coreHeap.Initialize(&pageHeap);

    void* page = coreHeap.AllocatePage(pageSize, 0);
    ASSERT_NE(page, nullptr);
    coreHeap.FreePage(page, pageSize);
    EXPECT_EQ(coreHeap.AllocatePage(pageSize, 0), page);

    vm->Release(base);
}

} // namespace Engine::Memory
