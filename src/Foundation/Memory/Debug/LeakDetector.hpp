#ifndef FOUNDATION_MEMORY_DEBUG_LEAK_DETECTOR_HDR
#define FOUNDATION_MEMORY_DEBUG_LEAK_DETECTOR_HDR

#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/MemoryTag.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <unordered_map>

namespace Engine::Memory {

struct LeakRecord {
    void* ptr = nullptr;
    size_t size = 0;
    MemoryTag tag = MemoryTag::Default;
};

class LeakDetector final {
public:
    void TrackAlloc(void* ptr, size_t size, MemoryTag tag = MemoryTag::Default);
    void TrackFree(void* ptr);
    void Reset();

    [[nodiscard]] size_t GetLiveAllocationCount() const noexcept;
    [[nodiscard]] size_t GetLiveBytes() const noexcept;
    [[nodiscard]] Vector<LeakRecord> GetLiveAllocations() const;

private:
    std::unordered_map<void*, LeakRecord> m_live;
    size_t m_liveBytes = 0;
    mutable Threading::SpinLock m_lock;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_DEBUG_LEAK_DETECTOR_HDR
