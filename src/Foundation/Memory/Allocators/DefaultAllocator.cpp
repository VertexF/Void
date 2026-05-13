#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>
#include <Foundation/Threading/Atomic.hpp>
#include <stdlib.h> // For malloc/free

namespace Engine::Memory {

namespace {

class MallocAllocator final : public IAllocator {
public:
    [[nodiscard]] void* Allocate(size_t size, size_t alignment) override
    {
        if (size == 0) {
            return nullptr;
        }
        if (!IsPowerOfTwo(alignment)) {
            alignment = alignof(MaxAlignT);
        }

        const size_t headerSize = sizeof(AllocationHeader);
        const size_t totalSize = size + headerSize + alignment;

        void* raw = malloc(totalSize);
        if (!raw) {
            return nullptr;
        }

        byte* rawBytes = static_cast<byte*>(raw);
        byte* aligned = static_cast<byte*>(AlignPointer(rawBytes + headerSize, alignment));
        auto* header = reinterpret_cast<AllocationHeader*>(aligned - headerSize);
        header->size = size;
        header->adjustment = static_cast<size_t>(aligned - rawBytes);
        header->tag = GetCurrentMemoryTag();
        header->padding = 0;

        m_allocated.FetchAdd(size, MemoryOrder::Relaxed);
        return aligned;
    }

    void Free(void* ptr) override
    {
        if (!ptr) {
            return;
        }
        byte* aligned = static_cast<byte*>(ptr);
        auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));

        m_allocated.FetchSub(header->size, MemoryOrder::Relaxed);
        byte* raw = aligned - header->adjustment;
        free(raw);
    }

    [[nodiscard]] size_t AllocatedSize() const override
    {
        return m_allocated.Load(MemoryOrder::Relaxed);
    }

    [[nodiscard]] const char* Name() const override
    {
        return "DefaultMalloc";
    }

    [[nodiscard]] bool Owns(void* ptr) const override
    {
        return ptr != nullptr;
    }

private:
    Atomic<size_t> m_allocated{0};
};

MallocAllocator& GetFallbackAllocator() noexcept
{
    static MallocAllocator fallback;
    return fallback;
}

Atomic<IAllocator*>& GetDefaultAllocatorPtr() noexcept
{
    static Atomic<IAllocator*> defaultAllocator{nullptr};
    return defaultAllocator;
}

} // namespace

IAllocator& GetDefaultAllocator()
{
    if (IAllocator* current = GetDefaultAllocatorPtr().Load(MemoryOrder::Relaxed)) {
        return *current;
    }
    return GetFallbackAllocator();
}

void SetDefaultAllocator(IAllocator* allocator)
{
    GetDefaultAllocatorPtr().Store(allocator, MemoryOrder::Relaxed);
}

} // namespace Engine::Memory
