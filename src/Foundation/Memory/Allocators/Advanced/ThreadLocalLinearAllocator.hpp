#ifndef FOUNDATION_MEMORY_THREAD_LOCAL_LINEAR_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_THREAD_LOCAL_LINEAR_ALLOCATOR_HDR

#include <Foundation/Memory/Allocators/LinearAllocator.hpp>

namespace Engine::Memory {

/// @brief Thread-local linear allocator for zero-contention fast temporary memory.
/// @details Each thread has its own private instance.
class ThreadLocalLinearAllocator : public IAllocator {
public:
    /// @param perThreadSize Size of buffer for EACH thread.
    /// @param backingAllocator Allocator for the per-thread buffers.
    explicit ThreadLocalLinearAllocator(size_t perThreadSize, IAllocator* backingAllocator = nullptr);
    ~ThreadLocalLinearAllocator() override;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;

    void Reset();

private:
    [[nodiscard]] LinearAllocator& GetLocalAllocator() const;

    size_t m_perThreadSize;
    IAllocator* m_backingAllocator;
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_THREAD_LOCAL_LINEAR_ALLOCATOR_HDR