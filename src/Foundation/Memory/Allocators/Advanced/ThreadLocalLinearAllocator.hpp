#ifndef FOUNDATION_MEMORY_THREAD_LOCAL_LINEAR_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_THREAD_LOCAL_LINEAR_ALLOCATOR_HDR

#include <Foundation/Memory/Allocators/LinearAllocator.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

namespace Engine::Memory {

struct ThreadLocalLinearAllocatorState;

/// @brief Thread-local linear allocator for temporary memory.
/// @details Each thread gets a private arena; allocator lifetime cleanup is coordinated through a state registry.
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
    [[nodiscard]] AllocatorStats GetStats() const override;
    [[nodiscard]] AllocatorStats GetDetailedStats() const override;

    void Reset();

private:
    friend struct ThreadLocalLinearAllocatorState;

    [[nodiscard]] LinearAllocator* GetLocalAllocator() const;
    [[nodiscard]] LinearAllocator* FindLocalAllocator() const noexcept;
    void RegisterState(ThreadLocalLinearAllocatorState* state) const;
    void UnregisterState(ThreadLocalLinearAllocatorState* state) const;
    void AddTrackedBytes(size_t bytes) noexcept;
    void ReleaseTrackedBytes(size_t bytes) noexcept;

    size_t m_perThreadSize;
    IAllocator* m_backingAllocator;
    mutable Threading::SpinLock m_stateLock;
    mutable ThreadLocalLinearAllocatorState* m_stateHead = nullptr;
    Atomic<size_t> m_allocatedBytes{0};
    AllocatorStatsTracker m_stats;
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_THREAD_LOCAL_LINEAR_ALLOCATOR_HDR
