#include <Foundation/Containers/ChunkedArray.hpp>

#include <gtest/gtest.h>

namespace Engine::Containers {
namespace {

class CountingAllocator final : public Memory::IAllocator {
public:
    void* Allocate(size_t size, size_t alignment = alignof(Memory::MaxAlignT)) override
    {
        ++allocations;
        allocatedBytes += size;
        return Memory::GetDefaultAllocator().Allocate(size, alignment);
    }

    void Free(void* ptr) override
    {
        if (ptr) {
            ++frees;
        }
        Memory::GetDefaultAllocator().Free(ptr);
    }

    size_t AllocatedSize() const override { return allocatedBytes; }
    const char* Name() const override { return "CountingAllocator"; }
    bool Owns(void*) const override { return false; }

    size_t allocations = 0;
    size_t frees = 0;
    size_t allocatedBytes = 0;
};

struct TrackedValue {
    explicit TrackedValue(int inValue = 0) : value(inValue) { ++live; }
    TrackedValue(const TrackedValue& other) : value(other.value) { ++live; ++copies; }
    TrackedValue(TrackedValue&& other) noexcept : value(other.value) { other.value = -1; ++live; ++moves; }
    TrackedValue& operator=(const TrackedValue& other) { value = other.value; ++copies; return *this; }
    TrackedValue& operator=(TrackedValue&& other) noexcept { value = other.value; other.value = -1; ++moves; return *this; }
    ~TrackedValue() { --live; ++destructions; }

    static void Reset()
    {
        live = 0;
        copies = 0;
        moves = 0;
        destructions = 0;
    }

    int value = 0;
    static inline int live = 0;
    static inline int copies = 0;
    static inline int moves = 0;
    static inline int destructions = 0;
};

} // namespace

TEST(ChunkedArrayContainer, PushesAcrossChunkBoundariesWithoutMovingExistingChunks)
{
    CountingAllocator allocator;
    ChunkedArray<int, 4> values(allocator);

    for (uint32 i = 0; i < 10; ++i) {
        values.PushBack(static_cast<int>(i * 3));
    }

    ASSERT_EQ(values.Size(), 10u);
    EXPECT_EQ(values.ChunkCount(), 3u);
    EXPECT_EQ(values.Capacity(), 12u);
    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[3], 9);
    EXPECT_EQ(values[4], 12);
    EXPECT_EQ(values[9], 27);
}

TEST(ChunkedArrayContainer, AppendRangeCommitsBulkAcrossChunks)
{
    ChunkedArray<int, 4> values;
    int source[] = {1, 2, 3, 4, 5, 6, 7};

    values.AppendRange(source, 7);

    ASSERT_EQ(values.Size(), 7u);
    EXPECT_EQ(values.ChunkCount(), 2u);
    for (uint32 i = 0; i < values.Size(); ++i) {
        EXPECT_EQ(values[i], source[i]);
    }
}

TEST(ChunkedArrayContainer, AppendWriterCommitsOnceAcrossChunks)
{
    TrackedValue::Reset();
    ChunkedArray<TrackedValue, 2> values;

    {
        auto writer = values.BeginAppend(5);
        writer.Emplace(10);
        writer.Emplace(20);
        writer.Emplace(30);
        writer.Emplace(40);
        writer.Emplace(50);

        EXPECT_EQ(values.Size(), 0u);
        EXPECT_EQ(writer.Count(), 5u);
        EXPECT_EQ(TrackedValue::live, 5);

        writer.Commit();
    }

    ASSERT_EQ(values.Size(), 5u);
    EXPECT_EQ(values.ChunkCount(), 3u);
    EXPECT_EQ(values[0].value, 10);
    EXPECT_EQ(values[2].value, 30);
    EXPECT_EQ(values[4].value, 50);
    EXPECT_EQ(TrackedValue::live, 5);
}

TEST(ChunkedArrayContainer, AppendWriterRollbackDestroysUncommittedObjects)
{
    TrackedValue::Reset();

    {
        ChunkedArray<TrackedValue, 2> values;
        {
            auto writer = values.BeginAppend(3);
            writer.Emplace(1);
            writer.Emplace(2);
            writer.Emplace(3);
            EXPECT_EQ(TrackedValue::live, 3);
        }

        EXPECT_TRUE(values.IsEmpty());
        EXPECT_EQ(TrackedValue::live, 0);
    }

    EXPECT_EQ(TrackedValue::live, 0);
}

TEST(ChunkedArrayContainer, ClearKeepsChunksForReuseAndReleaseMemoryFreesThem)
{
    CountingAllocator allocator;
    ChunkedArray<int, 4> values(allocator);

    values.Reserve(9);
    EXPECT_EQ(values.ChunkCount(), 3u);

    int source[] = {1, 2, 3, 4, 5};
    values.AppendRange(source, 5);
    values.Clear();

    EXPECT_TRUE(values.IsEmpty());
    EXPECT_EQ(values.ChunkCount(), 3u);

    values.ReleaseMemory();
    EXPECT_EQ(values.ChunkCount(), 0u);
    EXPECT_EQ(allocator.allocations, allocator.frees);
}

} // namespace Engine::Containers
