#ifndef FOUNDATION_MEMORY_DEBUG_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_DEBUG_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Atomic.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <unordered_set>

namespace Engine::Memory {

class LeakDetector;
class MemoryProfiler;

class DebugAllocator : public IAllocator {
public:
    explicit DebugAllocator(IAllocator* backingAllocator = nullptr,
                            LeakDetector* leakDetector = nullptr,
                            MemoryProfiler* profiler = nullptr);
    ~DebugAllocator() override = default;

    DebugAllocator(const DebugAllocator&) = delete;
    DebugAllocator& operator=(const DebugAllocator&) = delete;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;

    void SetLeakDetector(LeakDetector* detector) noexcept { m_leakDetector = detector; }
    void Profiler(MemoryProfiler* profiler) noexcept { m_profiler = profiler; }

private:
    IAllocator* m_backingAllocator = nullptr;
    LeakDetector* m_leakDetector = nullptr;
    MemoryProfiler* m_profiler = nullptr;
    Atomic<size_t> m_allocatedBytes{0};
    std::unordered_set<void*> m_liveAllocations;
    mutable Threading::SpinLock m_liveLock;
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_DEBUG_ALLOCATOR_HDR
