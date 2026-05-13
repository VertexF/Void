#ifndef FOUNDATION_MEMORY_MALLOC_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_MALLOC_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Atomic.hpp>

#if ENGINE_MEMORY_TRACK_OWNERSHIP
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <unordered_set>
#endif

namespace Engine::Memory {

/// @brief Allocator that forwards to malloc/free.
class MallocAllocator : public IAllocator {
public:
    MallocAllocator() = default;
    ~MallocAllocator() override = default;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;

private:
    Atomic<size_t> m_allocated{0};
    AllocatorStatsTracker m_stats;
#if ENGINE_MEMORY_TRACK_OWNERSHIP
    std::unordered_set<void*> m_liveAllocations;
    mutable Threading::SpinLock m_liveLock;
#endif
};

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_MALLOC_ALLOCATOR_HDR
