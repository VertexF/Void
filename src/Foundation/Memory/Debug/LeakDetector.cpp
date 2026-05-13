#include <Foundation/Memory/Debug/LeakDetector.hpp>

namespace Engine::Memory {

void LeakDetector::TrackAlloc(void* ptr, size_t size, MemoryTag tag)
{
    if (!ptr) {
        return;
    }

    Threading::SpinLockGuard guard(m_lock);
    const auto existing = m_live.find(ptr);
    if (existing != m_live.end()) {
        m_liveBytes = existing->second.size <= m_liveBytes ? (m_liveBytes - existing->second.size) : 0;
    }
    m_live[ptr] = {ptr, size, tag};
    m_liveBytes += size;
}

void LeakDetector::TrackFree(void* ptr)
{
    Threading::SpinLockGuard guard(m_lock);
    const auto it = m_live.find(ptr);
    if (it == m_live.end()) {
        return;
    }
    m_liveBytes = it->second.size <= m_liveBytes ? (m_liveBytes - it->second.size) : 0;
    m_live.erase(it);
}

void LeakDetector::Reset()
{
    Threading::SpinLockGuard guard(m_lock);
    m_live.clear();
    m_liveBytes = 0;
}

size_t LeakDetector::GetLiveAllocationCount() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_live.size();
}

size_t LeakDetector::GetLiveBytes() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_liveBytes;
}

Vector<LeakRecord> LeakDetector::GetLiveAllocations() const
{
    Threading::SpinLockGuard guard(m_lock);
    Vector<LeakRecord> records;
    records.reserve(m_live.size());
    for (const auto& entry : m_live) {
        records.push_back(entry.second);
    }
    return records;
}

} // namespace Engine::Memory
