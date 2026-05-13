#ifndef FOUNDATION_MEMORY_THREAD_SAFE_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_THREAD_SAFE_ALLOCATOR_HDR

// Thread-Safe Allocator (mutex wrapper)
// ============================================================================

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Mutex.hpp>

namespace Engine::Memory {

class ThreadSafeAllocator : public IAllocator {
public:
    explicit ThreadSafeAllocator(IAllocator* backingAllocator = nullptr);
    ~ThreadSafeAllocator() override = default;

    ThreadSafeAllocator(const ThreadSafeAllocator&) = delete;
    ThreadSafeAllocator& operator=(const ThreadSafeAllocator&) = delete;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;
    [[nodiscard]] AllocatorStats GetDetailedStats() const override;

    [[nodiscard]] IAllocator* GetBackingAllocator() const noexcept;
    void SetBackingAllocator(IAllocator* allocator) noexcept;

private:
    IAllocator* m_backingAllocator = nullptr;
    mutable Threading::Mutex m_mutex;
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_THREAD_SAFE_ALLOCATOR_HDR
