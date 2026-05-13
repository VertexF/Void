#include <Foundation/Memory/Debug/MemoryProfiler.hpp>

#include <algorithm>

namespace Engine::Memory {

namespace {
[[nodiscard]] size_t TagIndex(MemoryTag tag) noexcept
{
    const size_t index = static_cast<size_t>(tag);
    return index < static_cast<size_t>(MemoryTag::Count) ? index : 0;
}
}

void MemoryProfiler::TrackAlloc(void* address, size_t size, MemoryTag tag)
{
    if (!address || size == 0) {
        return;
    }

    Threading::SpinLockGuard guard(m_lock);
    const auto [it, inserted] = m_liveAllocations.emplace(address,
        LiveAllocation{size, tag, m_currentFrame, m_currentFrame});
    if (!inserted) {
        const size_t oldSize = it->second.size;
        const MemoryTag oldTag = it->second.tag;
        m_liveBytes = oldSize <= m_liveBytes ? (m_liveBytes - oldSize) : 0;
        TagStats& oldStats = m_tagStats[TagIndex(oldTag)];
        oldStats.liveBytes = oldSize <= oldStats.liveBytes ? (oldStats.liveBytes - oldSize) : 0;
        ++m_totalFrees;
        m_liveAllocations[address] = {size, tag, m_currentFrame, m_currentFrame};
    }

    ++m_totalAllocations;
    m_liveBytes += size;
    m_peakBytes = (std::max)(m_peakBytes, m_liveBytes);

    TagStats& stats = m_tagStats[TagIndex(tag)];
    stats.liveBytes += size;
    stats.peakBytes = (std::max)(stats.peakBytes, stats.liveBytes);
}

void MemoryProfiler::TrackFree(void* address, size_t size, MemoryTag tag)
{
    if (!address) {
        return;
    }

    Threading::SpinLockGuard guard(m_lock);
    const auto it = m_liveAllocations.find(address);
    if (it == m_liveAllocations.end()) {
        ++m_missedFrees;
    } else {
        size = it->second.size;
        tag = it->second.tag;
        m_liveAllocations.erase(it);
    }

    ++m_totalFrees;
    m_liveBytes = size <= m_liveBytes ? (m_liveBytes - size) : 0;

    TagStats& stats = m_tagStats[TagIndex(tag)];
    stats.liveBytes = size <= stats.liveBytes ? (stats.liveBytes - size) : 0;
}

void MemoryProfiler::TrackBinnedAlloc(void* address, size_t size, uint32, MemoryTag tag)
{
    TrackAlloc(address, size, tag);
}

void MemoryProfiler::TrackBinnedFree(void* address, size_t size, uint32, MemoryTag tag)
{
    TrackFree(address, size, tag);
}

void MemoryProfiler::TrackVramAlloc(size_t size, VramResourceType)
{
    Threading::SpinLockGuard guard(m_lock);
    m_visualStats.totalVramBytes += size;
}

void MemoryProfiler::TrackVramFree(size_t size, VramResourceType)
{
    Threading::SpinLockGuard guard(m_lock);
    m_visualStats.totalVramBytes = size <= m_visualStats.totalVramBytes ? (m_visualStats.totalVramBytes - size) : 0;
}

void MemoryProfiler::SetVramBudget(size_t budget)
{
    Threading::SpinLockGuard guard(m_lock);
    m_visualStats.totalVramBudget = budget;
}

void MemoryProfiler::TrackVmChange(void* address, size_t size, bool committed, const char* name)
{
    Threading::SpinLockGuard guard(m_lock);
    m_vmRegions.push_back({address, size, committed, name});
}

Vector<MemoryProfiler::VmRegionInfo> MemoryProfiler::GetVmRegions() const
{
    Threading::SpinLockGuard guard(m_lock);
    return m_vmRegions;
}

void MemoryProfiler::SetCallstackTracking(bool, uint32) noexcept {}

Vector<MemoryProfiler::StackAllocationStats> MemoryProfiler::GetStackHotspots() const
{
    return {};
}

void MemoryProfiler::TakeSnapshot(std::string name)
{
    Threading::SpinLockGuard guard(m_lock);
    m_snapshots.push_back({std::move(name), m_currentFrame, m_liveBytes, m_liveAllocations.size(),
        m_totalAllocations, m_totalFrees});
}

const MemoryProfiler::MemorySnapshot* MemoryProfiler::GetSnapshot(const std::string& name) const
{
    Threading::SpinLockGuard guard(m_lock);
    for (const MemorySnapshot& snapshot : m_snapshots) {
        if (snapshot.name == name) {
            return &snapshot;
        }
    }
    return nullptr;
}

void MemoryProfiler::DeleteSnapshot(const std::string& name)
{
    Threading::SpinLockGuard guard(m_lock);
    m_snapshots.erase(
        std::remove_if(m_snapshots.begin(), m_snapshots.end(),
            [&name](const MemorySnapshot& snapshot) { return snapshot.name == name; }),
        m_snapshots.end());
}

Vector<std::string> MemoryProfiler::GetSnapshotNames() const
{
    Threading::SpinLockGuard guard(m_lock);
    Vector<std::string> names;
    names.reserve(m_snapshots.size());
    for (const MemorySnapshot& snapshot : m_snapshots) {
        names.push_back(snapshot.name);
    }
    return names;
}

MemoryProfiler::MemoryDiff MemoryProfiler::DiffSnapshots(const std::string& nameA, const std::string& nameB) const
{
    Threading::SpinLockGuard guard(m_lock);
    const MemorySnapshot* a = nullptr;
    const MemorySnapshot* b = nullptr;
    for (const MemorySnapshot& snapshot : m_snapshots) {
        if (snapshot.name == nameA) {
            a = &snapshot;
        } else if (snapshot.name == nameB) {
            b = &snapshot;
        }
    }
    if (!a || !b) {
        return {nameA, nameB, 0, 0};
    }

    return {nameA, nameB,
        static_cast<int64>(b->liveBytes) - static_cast<int64>(a->liveBytes),
        static_cast<int32>(b->liveCount) - static_cast<int32>(a->liveCount)};
}

void MemoryProfiler::TrackAccess(void* address)
{
    Threading::SpinLockGuard guard(m_lock);
    const auto it = m_liveAllocations.find(address);
    if (it != m_liveAllocations.end()) {
        it->second.lastAccessFrame = m_currentFrame;
    }
}

void MemoryProfiler::UpdateFrameCount(uint64 frameIndex)
{
    Threading::SpinLockGuard guard(m_lock);
    m_currentFrame = frameIndex;
}

Vector<MemoryProfiler::ZombieAllocation> MemoryProfiler::DetectZombies(uint32 minAgeFrames) const
{
    Threading::SpinLockGuard guard(m_lock);
    Vector<ZombieAllocation> zombies;
    for (const auto& entry : m_liveAllocations) {
        const uint64 lastAccess = entry.second.lastAccessFrame;
        const uint64 age = m_currentFrame >= lastAccess ? (m_currentFrame - lastAccess) : 0;
        if (age >= minAgeFrames) {
            zombies.push_back({entry.first, entry.second.size, lastAccess, static_cast<uint32>(age)});
        }
    }
    return zombies;
}

void MemoryProfiler::TrackPageCommit(size_t pageSize)
{
    Threading::SpinLockGuard guard(m_lock);
    m_visualStats.totalCommitted += pageSize;
}

void MemoryProfiler::TrackPageDecommit(size_t pageSize)
{
    Threading::SpinLockGuard guard(m_lock);
    m_visualStats.totalCommitted = pageSize <= m_visualStats.totalCommitted ? (m_visualStats.totalCommitted - pageSize) : 0;
}

void MemoryProfiler::UpdateVisualStats(const VisualMemoryStats& stats)
{
    Threading::SpinLockGuard guard(m_lock);
    m_visualStats = stats;
}

MemoryProfiler::VisualMemoryStats MemoryProfiler::GetVisualStats() const
{
    Threading::SpinLockGuard guard(m_lock);
    return m_visualStats;
}

void MemoryProfiler::Reset()
{
    Threading::SpinLockGuard guard(m_lock);
    m_liveBytes = 0;
    m_peakBytes = 0;
    m_totalAllocations = 0;
    m_totalFrees = 0;
    m_missedFrees = 0;
    for (TagStats& stats : m_tagStats) {
        stats = {};
    }
    m_liveAllocations.clear();
    m_visualStats = {};
    m_vmRegions.clear();
    m_snapshots.clear();
    m_currentFrame = 0;
}

size_t MemoryProfiler::GetTotalAllocations() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_totalAllocations;
}

size_t MemoryProfiler::GetTotalFrees() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_totalFrees;
}

size_t MemoryProfiler::GetLiveBytes() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_liveBytes;
}

size_t MemoryProfiler::GetPeakBytes() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_peakBytes;
}

size_t MemoryProfiler::GetMissedFrees() const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_missedFrees;
}

size_t MemoryProfiler::GetLiveBytesByTag(MemoryTag tag) const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_tagStats[TagIndex(tag)].liveBytes;
}

size_t MemoryProfiler::GetPeakBytesByTag(MemoryTag tag) const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_tagStats[TagIndex(tag)].peakBytes;
}

size_t MemoryProfiler::GetBudgetByTag(MemoryTag tag) const noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    return m_tagStats[TagIndex(tag)].budgetBytes;
}

Vector<MemoryProfiler::BudgetStatus> MemoryProfiler::GetBudgetStatuses() const
{
    Threading::SpinLockGuard guard(m_lock);
    Vector<BudgetStatus> statuses;
    statuses.reserve(static_cast<size_t>(MemoryTag::Count));
    for (size_t i = 0; i < static_cast<size_t>(MemoryTag::Count); ++i) {
        const TagStats& stats = m_tagStats[i];
        statuses.push_back({static_cast<MemoryTag>(i), stats.liveBytes, stats.budgetBytes,
            stats.budgetBytes != 0 && stats.liveBytes > stats.budgetBytes});
    }
    return statuses;
}

void MemoryProfiler::SetBudgetByTag(MemoryTag tag, size_t budgetBytes) noexcept
{
    Threading::SpinLockGuard guard(m_lock);
    m_tagStats[TagIndex(tag)].budgetBytes = budgetBytes;
}

} // namespace Engine::Memory
