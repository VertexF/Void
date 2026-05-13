#include <Foundation/Memory/Allocator.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>

#if defined(_MSC_VER)
    #include <malloc.h>
#endif

namespace Engine::Memory {
namespace {

class CountingAllocator final : public IAllocator {
public:
    void* Allocate(size_t size, size_t alignment) override
    {
        ++AllocCount;
        BytesRequested += size;

        if (alignment < alignof(void*)) {
            alignment = alignof(void*);
        }

#if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
#else
        return std::aligned_alloc(alignment, AlignSize(size, alignment));
#endif
    }

    void Free(void* ptr) override
    {
        if (ptr) {
            ++FreeCount;
        }

#if defined(_MSC_VER)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    size_t AllocatedSize() const override { return 0; }
    const char* Name() const override { return "CountingAllocator"; }
    bool Owns(void*) const override { return false; }

    size_t AllocCount = 0;
    size_t FreeCount = 0;
    size_t BytesRequested = 0;
};

struct DefaultAllocatorGuard {
    explicit DefaultAllocatorGuard(IAllocator* allocator)
    {
        SetDefaultAllocator(allocator);
    }

    ~DefaultAllocatorGuard()
    {
        SetDefaultAllocator(nullptr);
    }
};

} // namespace

TEST(DefaultAllocator, SetDefaultAllocatorOverridesGlobalAllocator)
{
    CountingAllocator custom;

    {
        DefaultAllocatorGuard guard(&custom);
        IAllocator& allocator = GetDefaultAllocator();
        EXPECT_EQ(&allocator, &custom);

        void* ptr = allocator.Allocate(96, 16);
        ASSERT_NE(ptr, nullptr);
        allocator.Free(ptr);
    }

    EXPECT_EQ(custom.AllocCount, 1u);
    EXPECT_EQ(custom.FreeCount, 1u);
    EXPECT_EQ(custom.BytesRequested, 96u);
}

TEST(DefaultAllocator, GuardRestoresSystemAllocator)
{
    {
        CountingAllocator custom;
        DefaultAllocatorGuard guard(&custom);
        EXPECT_EQ(&GetDefaultAllocator(), &custom);
    }

    IAllocator& allocator = GetDefaultAllocator();
    void* ptr = allocator.Allocate(32, 8);
    ASSERT_NE(ptr, nullptr);
    allocator.Free(ptr);
}

} // namespace Engine::Memory
