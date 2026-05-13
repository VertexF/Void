#ifndef FOUNDATION_MEMORY_TRACKED_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_TRACKED_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Atomic.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <unordered_set>

namespace Engine::Memory {

class TrackedAllocator : public IAllocator {
public:
    explicit TrackedAllocator(IAllocator* backingAllocator = nullptr);
    ~TrackedAllocator() override = default;

    TrackedAllocator(const TrackedAllocator&) = delete;
    TrackedAllocator& operator=(const TrackedAllocator&) = delete;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;

    [[nodiscard]] size_t GetAllocationCount() const noexcept { return m_allocationCount.Load(MemoryOrder::Relaxed); }
    [[nodiscard]] size_t GetPeakAllocatedSize() const noexcept { return m_peakBytes.Load(MemoryOrder::Relaxed); }

private:
    IAllocator* m_backingAllocator = nullptr;
    Atomic<size_t> m_allocatedBytes{0};
    Atomic<size_t> m_peakBytes{0};
    Atomic<size_t> m_allocationCount{0};
    Atomic<size_t> m_freeCount{0};
    Atomic<size_t> m_failedAllocationCount{0};
    std::unordered_set<void*> m_liveAllocations;
    mutable Threading::SpinLock m_liveLock;
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_TRACKED_ALLOCATOR_HDR
