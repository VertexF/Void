// ============================================================================
// Void Engine - Memory Tests
// TLSF Allocator
// ============================================================================

#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <gtest/gtest.h>
#include <thread>


namespace Engine::Memory {

TEST(TLSFAllocator, BasicAllocationAndFree)
{
    TLSFAllocator pool(4096);
    EXPECT_EQ(pool.GetTotalSize(), 4096u);

    void* p1 = pool.Allocate(128);
    ASSERT_NE(p1, nullptr);
    EXPECT_GE(pool.AllocatedSize(), 128u);

    void* p2 = pool.Allocate(256);
    ASSERT_NE(p2, nullptr);

    pool.Free(p1);
    pool.Free(p2);
    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

TEST(TLSFAllocator, LargeAllocations)
{
    const size_t poolSize = 1024 * 1024; // 1MB
    TLSFAllocator pool(poolSize);

    void* p1 = pool.Allocate(poolSize / 2);
    ASSERT_NE(p1, nullptr);

    void* p2 = pool.Allocate(poolSize / 4);
    ASSERT_NE(p2, nullptr);

    pool.Free(p1);
    pool.Free(p2);
    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

TEST(TLSFAllocator, OwnsAndBounds)
{
    TLSFAllocator pool(1024);
    void* p = pool.Allocate(64);
    ASSERT_NE(p, nullptr);

    EXPECT_TRUE(pool.Owns(p));
    
    int dummy = 0;
    EXPECT_FALSE(pool.Owns(&dummy));

    pool.Free(p);
    EXPECT_FALSE(pool.Owns(p));
}

TEST(TLSFAllocator, RejectsOversizedAllocation)
{
    TLSFAllocator pool(1024);
    void* p = pool.Allocate(2048);
    EXPECT_EQ(p, nullptr);
}

TEST(TLSFAllocator, ThreadSafetyConcurrentAllocations)
{
    const size_t poolSize = 64 * 1024;
    TLSFAllocator pool(poolSize);
    
    const int threadCount = 4;
    const int allocationsPerThread = 100;
    
    auto worker = [&]() {
        Vector<void*> ptrs;
        ptrs.reserve(allocationsPerThread);
        for (int i = 0; i < allocationsPerThread; ++i) {
            void* p = pool.Allocate(32);
            if (p) ptrs.push_back(p);
        }
        for (void* p : ptrs) {
            pool.Free(p);
        }
    };

    Vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

TEST(TLSFAllocator, SupportsSmallSizeClasses)
{
    TLSFAllocator pool(4096);

    void* p16 = pool.Allocate(16);
    void* p17 = pool.Allocate(17);
    void* p24 = pool.Allocate(24);
    void* p31 = pool.Allocate(31);

    ASSERT_NE(p16, nullptr);
    ASSERT_NE(p17, nullptr);
    ASSERT_NE(p24, nullptr);
    ASSERT_NE(p31, nullptr);

    pool.Free(p16);
    pool.Free(p17);
    pool.Free(p24);
    pool.Free(p31);
    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

TEST(TLSFAllocator, HandlesZeroAndInvalidAlignmentEdges)
{
    TLSFAllocator pool(4096);

    EXPECT_EQ(pool.Allocate(0), nullptr);
    EXPECT_EQ(pool.Reallocate(nullptr, 0), nullptr);

    void* normalized = pool.Allocate(64, 24);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(normalized) % alignof(MaxAlignT), 0u);
    EXPECT_TRUE(pool.Owns(normalized));
    EXPECT_FALSE(pool.Owns(static_cast<uint8*>(normalized) + 1));

    void* tooAligned = pool.Allocate(64, 32);
    EXPECT_EQ(tooAligned, nullptr);

    void* grown = pool.Reallocate(normalized, 128, 24);
    ASSERT_NE(grown, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(grown) % alignof(MaxAlignT), 0u);
    pool.Free(grown);
    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

TEST(TLSFAllocator, RepeatedSmallAllocFree)
{
    TLSFAllocator pool(128 * 1024);

    for (int i = 0; i < 2000; ++i) {
        void* p16 = pool.Allocate(16);
        ASSERT_NE(p16, nullptr);
        pool.Free(p16);
    }

    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

TEST(TLSFAllocator, RejectsInvalidAndDoubleFreeWithoutTouchingCounters)
{
    TLSFAllocator pool(4096);
    int stackValue = 0;

    EXPECT_FALSE(pool.Owns(&stackValue));
    pool.Free(&stackValue);
    EXPECT_EQ(pool.Reallocate(&stackValue, 64), nullptr);
    EXPECT_EQ(pool.AllocatedSize(), 0u);

    void* ptr = pool.Allocate(128);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(pool.Owns(ptr));
    pool.Free(ptr);
    EXPECT_FALSE(pool.Owns(ptr));
    pool.Free(ptr);

    EXPECT_EQ(pool.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
