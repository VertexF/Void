#include <Foundation/Memory/Allocator.hpp>

#include <cstdlib>

#if defined(_MSC_VER)
    #include <malloc.h>
#endif

namespace Engine::Memory {
namespace {

class SystemAllocator final : public IAllocator {
public:
    void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override
    {
        if (size == 0) {
            return nullptr;
        }

        if (alignment < alignof(void*)) {
            alignment = alignof(void*);
        }

#if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
#else
        const size_t alignedSize = AlignSize(size, alignment);
        return std::aligned_alloc(alignment, alignedSize);
#endif
    }

    void Free(void* ptr) override
    {
        if (!ptr) {
            return;
        }

#if defined(_MSC_VER)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override
    {
        if (newSize == 0) {
            Free(ptr);
            return nullptr;
        }
        if (!ptr) {
            return Allocate(newSize, alignment);
        }
        if (alignment < alignof(void*)) {
            alignment = alignof(void*);
        }

#if defined(_MSC_VER)
        return _aligned_realloc(ptr, newSize, alignment);
#else
        (void)ptr;
        return nullptr;
#endif
    }

    size_t AllocatedSize() const override { return 0; }
    const char* Name() const override { return "SystemAllocator"; }
    bool Owns(void*) const override { return false; }
};

SystemAllocator g_systemAllocator;
IAllocator* g_defaultAllocator = &g_systemAllocator;

} // namespace

IAllocator& GetDefaultAllocator()
{
    return *g_defaultAllocator;
}

void SetDefaultAllocator(IAllocator* allocator)
{
    g_defaultAllocator = allocator ? allocator : &g_systemAllocator;
}

} // namespace Engine::Memory
