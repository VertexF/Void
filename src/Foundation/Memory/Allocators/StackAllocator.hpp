#ifndef FOUNDATION_MEMORY_STACK_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_STACK_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>

namespace Engine::Memory {

/// @brief LIFO stack allocator. Frees must occur in reverse allocation order.
class StackAllocator : public IAllocator {
public:
    explicit StackAllocator(size_t size, IAllocator* backingAllocator = nullptr);
    StackAllocator(void* buffer, size_t size);
    ~StackAllocator() override;

    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    StackAllocator(StackAllocator&& other) noexcept;
    StackAllocator& operator=(StackAllocator&& other) noexcept;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;

    void Reset();
    [[nodiscard]] size_t GetCapacity() const noexcept;
    [[nodiscard]] size_t GetMarker() const noexcept;
    void RewindToMarker(size_t marker);

private:
    [[nodiscard]] bool IsLiveAllocation(void* ptr) const noexcept;

    void* m_buffer = nullptr;
    size_t m_capacity = 0;
    size_t m_offset = 0;
    IAllocator* m_backingAllocator = nullptr;
    bool m_ownsBuffer = false;
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_STACK_ALLOCATOR_HDR
