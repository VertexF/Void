#ifndef FOUNDATION_MEMORY_THREAD_SAFE_LINEAR_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_THREAD_SAFE_LINEAR_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

/// @brief Thread-safe linear allocator using atomic bump pointer.
/// @details Shared linear allocator. Individual frees are not supported.
class ThreadSafeLinearAllocator : public IAllocator {
public:
    explicit ThreadSafeLinearAllocator(size_t size, IAllocator* backingAllocator = nullptr);
    ~ThreadSafeLinearAllocator() override;

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
    struct AllocationHeader {
        size_t size = 0;
        uint32 generation = 0;
        uint32 magic = 0;
    };

    [[nodiscard]] bool IsLiveAllocation(void* ptr) const noexcept;
    [[nodiscard]] uint32 CurrentGeneration() const noexcept;
    void AdvanceGeneration() noexcept;

    void* m_buffer = nullptr;
    size_t m_capacity = 0;
    Atomic<size_t> m_offset{0};
    Atomic<uint32> m_generation{1};
    IAllocator* m_backingAllocator = nullptr;
    AllocatorStatsTracker m_stats;
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_THREAD_SAFE_LINEAR_ALLOCATOR_HDR
