#ifndef FOUNDATION_MEMORY_MEMORY_PROFILER_HDR
#define FOUNDATION_MEMORY_MEMORY_PROFILER_HDR

#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/MemoryTag.hpp>
#include <Foundation/Platform.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <string>
#include <unordered_map>

namespace Engine::Memory {

class MemoryProfiler final {
public:
    struct StackAllocationStats {
        size_t liveBytes = 0;
        uint32 allocationCount = 0;
        uint32 liveCount = 0;
    };

    struct MemorySnapshot {
        std::string name;
        uint64 timestamp = 0;
        size_t liveBytes = 0;
        size_t liveCount = 0;
        size_t totalAllocations = 0;
        size_t totalFrees = 0;
    };

    struct MemoryDiff {
        std::string nameA;
        std::string nameB;
        int64 totalByteDelta = 0;
        int32 totalCountDelta = 0;
    };

    struct ZombieAllocation {
        void* address = nullptr;
        size_t size = 0;
        uint64 lastAccessFrame = 0;
        uint32 ageFrames = 0;
    };

    struct VisualMemoryStats {
        size_t totalReserved = 0;
        size_t totalCommitted = 0;
        uint32 activePages = 0;
        uint32 activeBins[128] = {0};
        uint32 activeBlocks[128] = {0};
        uint32 totalBlocks[128] = {0};
        uint64 totalAllocations = 0;
        uint64 totalFrees = 0;
        size_t totalVramBytes = 0;
        size_t totalVramBudget = 0;
        size_t bufferVramBytes = 0;
        size_t textureVramBytes = 0;
        size_t rtVramBytes = 0;
        uint32 ageHistogram[5] = {0};
    };

    enum class VramResourceType : uint8 {
        Buffer,
        Texture,
        AccelerationStructure,
        Internal
    };

    struct VmRegionInfo {
        void* baseAddress = nullptr;
        size_t size = 0;
        bool isCommitted = false;
        const char* name = nullptr;
    };

    struct BudgetStatus {
        MemoryTag tag = MemoryTag::Default;
        size_t liveBytes = 0;
        size_t budgetBytes = 0;
        bool isExceeded = false;
    };

    void TrackAlloc(void* address, size_t size, MemoryTag tag = MemoryTag::Default);
    void TrackFree(void* address, size_t size, MemoryTag tag = MemoryTag::Default);
    void TrackBinnedAlloc(void* address, size_t size, uint32 binIndex, MemoryTag tag = MemoryTag::Default);
    void TrackBinnedFree(void* address, size_t size, uint32 binIndex, MemoryTag tag = MemoryTag::Default);
    void TrackVramAlloc(size_t size, VramResourceType type);
    void TrackVramFree(size_t size, VramResourceType type);
    void SetVramBudget(size_t budget);
    void TrackVmChange(void* address, size_t size, bool committed, const char* name);
    Vector<VmRegionInfo> GetVmRegions() const;
    void SetCallstackTracking(bool enabled, uint32 samplingRate = 1) noexcept;
    Vector<StackAllocationStats> GetStackHotspots() const;
    void TakeSnapshot(std::string name);
    const MemorySnapshot* GetSnapshot(const std::string& name) const;
    void DeleteSnapshot(const std::string& name);
    Vector<std::string> GetSnapshotNames() const;
    [[nodiscard]] MemoryDiff DiffSnapshots(const std::string& nameA, const std::string& nameB) const;
    void TrackAccess(void* address);
    void UpdateFrameCount(uint64 frameIndex);
    Vector<ZombieAllocation> DetectZombies(uint32 minAgeFrames) const;
    void TrackPageCommit(size_t pageSize);
    void TrackPageDecommit(size_t pageSize);
    void UpdateVisualStats(const VisualMemoryStats& stats);
    [[nodiscard]] VisualMemoryStats GetVisualStats() const;
    void Reset();

    [[nodiscard]] size_t GetTotalAllocations() const noexcept;
    [[nodiscard]] size_t GetTotalFrees() const noexcept;
    [[nodiscard]] size_t GetLiveBytes() const noexcept;
    [[nodiscard]] size_t GetPeakBytes() const noexcept;
    [[nodiscard]] size_t GetMissedFrees() const noexcept;
    [[nodiscard]] size_t GetLiveBytesByTag(MemoryTag tag) const noexcept;
    [[nodiscard]] size_t GetPeakBytesByTag(MemoryTag tag) const noexcept;
    [[nodiscard]] size_t GetBudgetByTag(MemoryTag tag) const noexcept;
    [[nodiscard]] Vector<BudgetStatus> GetBudgetStatuses() const;
    void SetBudgetByTag(MemoryTag tag, size_t budgetBytes) noexcept;

private:
    struct TagStats {
        size_t liveBytes = 0;
        size_t peakBytes = 0;
        size_t budgetBytes = 0;
    };

    struct LiveAllocation {
        size_t size = 0;
        MemoryTag tag = MemoryTag::Default;
        uint64 allocationFrame = 0;
        uint64 lastAccessFrame = 0;
    };

    size_t m_liveBytes = 0;
    size_t m_peakBytes = 0;
    size_t m_totalAllocations = 0;
    size_t m_totalFrees = 0;
    size_t m_missedFrees = 0;
    TagStats m_tagStats[static_cast<size_t>(MemoryTag::Count)]{};
    std::unordered_map<void*, LiveAllocation> m_liveAllocations;
    VisualMemoryStats m_visualStats{};
    Vector<VmRegionInfo> m_vmRegions;
    Vector<MemorySnapshot> m_snapshots;
    uint64 m_currentFrame = 0;
    mutable Threading::SpinLock m_lock;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_MEMORY_PROFILER_HDR
