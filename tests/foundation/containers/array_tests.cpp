#include <Foundation/Containers/Array.hpp>

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

struct alignas(64) OverAlignedValue {
    int value = 0;
};

} // namespace

TEST(ArrayContainer, InlineCapacityAvoidsHeapUntilExceeded)
{
    TrackedValue::Reset();
    CountingAllocator allocator;

    {
        Array<TrackedValue, 2> values(allocator);
        EXPECT_TRUE(values.UsingInlineStorage());
        EXPECT_EQ(values.Capacity(), 2u);

        values.EmplaceBack(1);
        values.EmplaceBack(2);
        EXPECT_EQ(allocator.allocations, 0u);
        EXPECT_TRUE(values.UsingInlineStorage());

        values.EmplaceBack(3);
        EXPECT_FALSE(values.UsingInlineStorage());
        EXPECT_EQ(allocator.allocations, 1u);
        EXPECT_EQ(values.Size(), 3u);
        EXPECT_EQ(values[0].value, 1);
        EXPECT_EQ(values[1].value, 2);
        EXPECT_EQ(values[2].value, 3);
    }

    EXPECT_EQ(allocator.allocations, allocator.frees);
    EXPECT_EQ(TrackedValue::live, 0);
}

TEST(ArrayContainer, MoveStealsHeapStorageWithoutReallocation)
{
    TrackedValue::Reset();
    CountingAllocator allocator;

    {
        Array<TrackedValue, 1> source(allocator);
        source.EmplaceBack(10);
        source.EmplaceBack(20);
        source.EmplaceBack(30);
        const size_t allocationsAfterGrowth = allocator.allocations;

        Array<TrackedValue, 1> moved(std::move(source));

        EXPECT_TRUE(source.IsEmpty());
        EXPECT_TRUE(source.UsingInlineStorage());
        EXPECT_EQ(allocator.allocations, allocationsAfterGrowth);
        EXPECT_EQ(moved.Size(), 3u);
        EXPECT_EQ(moved[0].value, 10);
        EXPECT_EQ(moved[1].value, 20);
        EXPECT_EQ(moved[2].value, 30);
    }

    EXPECT_EQ(allocator.allocations, allocator.frees);
    EXPECT_EQ(TrackedValue::live, 0);
}

TEST(ArrayContainer, EraseAndInsertPreserveOrder)
{
    Array<int, 4> values{1, 2, 3, 4, 5};

    values.erase(values.begin() + 1, values.begin() + 3);
    ASSERT_EQ(values.Size(), 3u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 4);
    EXPECT_EQ(values[2], 5);

    values.insert(values.begin() + 1, 9);
    ASSERT_EQ(values.Size(), 4u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 9);
    EXPECT_EQ(values[2], 4);
    EXPECT_EQ(values[3], 5);
}

TEST(ArrayContainer, BulkAppendAndUninitializedAddCommitSizeOnce)
{
    Array<int> values;

    int source[] = {1, 2, 3, 4};
    values.AppendRange(source, 4);
    ASSERT_EQ(values.Size(), 4u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[3], 4);

    int* tail = values.AddUninitialized(3);
    ASSERT_NE(tail, nullptr);
    tail[0] = 5;
    tail[1] = 6;
    tail[2] = 7;

    ASSERT_EQ(values.Size(), 7u);
    EXPECT_EQ(values[4], 5);
    EXPECT_EQ(values[5], 6);
    EXPECT_EQ(values[6], 7);
}

TEST(ArrayContainer, AppendWriterCommitsSizeOnce)
{
    Array<TrackedValue> values;
    TrackedValue::Reset();

    {
        auto writer = values.BeginAppend(3);
        writer.Emplace(10);
        writer.Emplace(20);
        writer.Emplace(30);

        EXPECT_EQ(values.Size(), 0u);
        EXPECT_EQ(writer.Count(), 3u);

        writer.Commit();
    }

    ASSERT_EQ(values.Size(), 3u);
    EXPECT_EQ(values[0].value, 10);
    EXPECT_EQ(values[1].value, 20);
    EXPECT_EQ(values[2].value, 30);
    EXPECT_EQ(TrackedValue::live, 3);
}

TEST(ArrayContainer, AppendWriterRollbackDestroysUncommittedObjects)
{
    TrackedValue::Reset();

    {
        Array<TrackedValue> values;
        {
            auto writer = values.BeginAppend(2);
            writer.Emplace(10);
            writer.Emplace(20);
            EXPECT_EQ(TrackedValue::live, 2);
        }

        EXPECT_TRUE(values.IsEmpty());
        EXPECT_EQ(TrackedValue::live, 0);
    }

    EXPECT_EQ(TrackedValue::live, 0);
}

TEST(ArrayContainer, SupportsOverAlignedElementsInInlineAndHeapStorage)
{
    CountingAllocator allocator;
    Array<OverAlignedValue, 1> values(allocator);

    values.EmplaceBack(OverAlignedValue{1});
    EXPECT_EQ(reinterpret_cast<uintptr>(&values[0]) % alignof(OverAlignedValue), 0u);
    EXPECT_TRUE(values.UsingInlineStorage());

    values.EmplaceBack(OverAlignedValue{2});
    EXPECT_FALSE(values.UsingInlineStorage());
    EXPECT_EQ(reinterpret_cast<uintptr>(values.Data()) % alignof(OverAlignedValue), 0u);
    EXPECT_EQ(reinterpret_cast<uintptr>(&values[0]) % alignof(OverAlignedValue), 0u);
    EXPECT_EQ(reinterpret_cast<uintptr>(&values[1]) % alignof(OverAlignedValue), 0u);
}

} // namespace Engine::Containers
