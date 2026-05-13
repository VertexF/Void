#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>

namespace Engine::Memory {

namespace {
MemoryManager* g_instance = nullptr;
Threading::SpinLock g_instanceLock;
thread_local uint32 g_profilingSuppressionDepth = 0;

[[nodiscard]] size_t TagIndex(MemoryTag tag) noexcept
{
    const size_t index = static_cast<size_t>(tag);
    return index < static_cast<size_t>(MemoryTag::Count) ? index : 0;
}

void AppendJsonEscaped(std::string& out, const std::string& text)
{
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
}

void AppendJsonField(std::string& out, const char* name, size_t value, bool trailingComma)
{
    out += "\"";
    out += name;
    out += "\":";
    out += std::to_string(value);
    if (trailingComma) {
        out += ",";
    }
}

void AppendTextField(std::string& out, const char* name, size_t value)
{
    out += " ";
    out += name;
    out += "=";
    out += std::to_string(value);
}
}

std::string MemoryManager::ToString(StringView name)
{
    if (!name.text || name.length == 0) {
        return {};
    }
    return std::string(name.text, name.length);
}

std::vector<MemoryManager::NamedAllocatorStats> MemoryManager::SnapshotRegisteredAllocatorStats(bool publishToProfiler)
{
    MemoryManager* instance = g_instance;
    if (!instance) {
        return {};
    }

    std::unordered_map<std::string, IAllocator*> allocators;
    MemoryProfiler* profiler = nullptr;
    {
        Threading::SpinLockGuard guard(instance->m_lock);
        allocators = instance->m_allocators;
        profiler = instance->m_profiler;
    }

    std::vector<NamedAllocatorStats> snapshots;
    snapshots.reserve(allocators.size());
    for (const auto& entry : allocators) {
        if (!entry.second) {
            continue;
        }

        AllocatorStats stats = entry.second->GetStats();
        if (!stats.name || stats.name[0] == '\0') {
            stats.name = entry.second->Name();
        }
        if (profiler && publishToProfiler) {
            profiler->UpdateAllocatorStats(entry.first.c_str(), stats);
        }
        snapshots.push_back({entry.first, stats});
    }
    return snapshots;
}

void MemoryManager::Initialize()
{
    Threading::SpinLockGuard guard(g_instanceLock);
    if (!g_instance) {
        g_instance = new MemoryManager();
    }
}

void MemoryManager::Shutdown()
{
    Threading::SpinLockGuard guard(g_instanceLock);
    MemoryManager* instance = g_instance;
    g_instance = nullptr;
    delete instance;
}

bool MemoryManager::IsInitialized()
{
    Threading::SpinLockGuard guard(g_instanceLock);
    return g_instance != nullptr;
}

void MemoryManager::RegisterAllocator(StringView name, IAllocator* allocator)
{
    if (!allocator || !name.text || name.length == 0) {
        return;
    }
    Initialize();
    Threading::SpinLockGuard guard(g_instance->m_lock);
    g_instance->m_allocators[ToString(name)] = allocator;
}

void MemoryManager::UnregisterAllocator(StringView name)
{
    if (!g_instance) {
        return;
    }
    Threading::SpinLockGuard guard(g_instance->m_lock);
    g_instance->m_allocators.erase(ToString(name));
}

IAllocator* MemoryManager::AllocatorByName(StringView name)
{
    if (!g_instance) {
        return nullptr;
    }
    Threading::SpinLockGuard guard(g_instance->m_lock);
    const auto it = g_instance->m_allocators.find(ToString(name));
    return it != g_instance->m_allocators.end() ? it->second : nullptr;
}

bool MemoryManager::HasAllocator(StringView name)
{
    if (!g_instance) {
        return false;
    }
    Threading::SpinLockGuard guard(g_instance->m_lock);
    return g_instance->m_allocators.find(ToString(name)) != g_instance->m_allocators.end();
}

void MemoryManager::Budget(MemoryTag tag, size_t budgetLimit)
{
    Initialize();
    Threading::SpinLockGuard guard(g_instance->m_lock);
    g_instance->m_budgets[TagIndex(tag)] = budgetLimit;
}

bool MemoryManager::IsBudgetExceeded(MemoryTag tag)
{
    Initialize();
    size_t budget = 0;
    MemoryProfiler* profiler = nullptr;
    {
        Threading::SpinLockGuard guard(g_instance->m_lock);
        budget = g_instance->m_budgets[TagIndex(tag)];
        profiler = g_instance->m_profiler;
    }
    const size_t usage = profiler ? profiler->GetLiveBytesByTag(tag) : 0;
    return budget != 0 && usage > budget;
}

bool MemoryManager::ReportBudgetPressure(MemoryTag tag, size_t requestedSize)
{
    Initialize();
    size_t budget = 0;
    size_t usage = 0;
    OOMCallback callback = nullptr;
    {
        Threading::SpinLockGuard guard(g_instance->m_lock);
        budget = g_instance->m_budgets[TagIndex(tag)];
        callback = g_instance->m_oomCallback;
        usage = g_instance->m_profiler ? g_instance->m_profiler->GetLiveBytesByTag(tag) : 0;
    }

    const bool exceeded = budget != 0 && requestedSize > budget - (usage < budget ? usage : budget);
    if (exceeded && callback) {
        callback(tag, requestedSize, usage, budget);
    }
    return exceeded;
}

size_t MemoryManager::BudgetUsage(MemoryTag tag)
{
    Initialize();
    MemoryProfiler* profiler = nullptr;
    {
        Threading::SpinLockGuard guard(g_instance->m_lock);
        profiler = g_instance->m_profiler;
    }
    return profiler ? profiler->GetLiveBytesByTag(tag) : 0;
}

void MemoryManager::RegisterOOMCallback(OOMCallback callback)
{
    Initialize();
    Threading::SpinLockGuard guard(g_instance->m_lock);
    g_instance->m_oomCallback = callback;
}

void MemoryManager::Profiler(MemoryProfiler* profiler)
{
    Initialize();
    Threading::SpinLockGuard guard(g_instance->m_lock);
    g_instance->m_profiler = profiler;
}

MemoryProfiler* MemoryManager::Profiler()
{
    MemoryManager* instance = g_instance;
    if (!instance) {
        return nullptr;
    }
    Threading::SpinLockGuard guard(instance->m_lock);
    return instance->m_profiler;
}

bool MemoryManager::CaptureAllocatorStats(StringView name)
{
    Initialize();
    IAllocator* allocator = nullptr;
    MemoryProfiler* profiler = nullptr;
    std::string key;
    {
        Threading::SpinLockGuard guard(g_instance->m_lock);
        key = ToString(name);
        const auto it = g_instance->m_allocators.find(key);
        if (it == g_instance->m_allocators.end()) {
            return false;
        }
        allocator = it->second;
        profiler = g_instance->m_profiler;
    }

    if (!allocator || !profiler) {
        return false;
    }

    AllocatorStats stats = allocator->GetStats();
    profiler->UpdateAllocatorStats(key.c_str(), stats);
    return true;
}

void MemoryManager::CaptureAllAllocatorStats()
{
    Initialize();
    (void)SnapshotRegisteredAllocatorStats(true);
}

bool MemoryManager::GetAllocatorStats(StringView name, AllocatorStats& outStats)
{
    MemoryManager* instance = g_instance;
    if (!instance) {
        return false;
    }
    MemoryProfiler* profiler = nullptr;
    {
        Threading::SpinLockGuard guard(instance->m_lock);
        profiler = instance->m_profiler;
    }
    return profiler ? profiler->GetAllocatorStats(ToString(name), outStats) : false;
}

Vector<AllocatorStats> MemoryManager::GetAllocatorStatsSnapshots()
{
    MemoryManager* instance = g_instance;
    if (!instance) {
        return {};
    }
    MemoryProfiler* profiler = nullptr;
    {
        Threading::SpinLockGuard guard(instance->m_lock);
        profiler = instance->m_profiler;
    }
    return profiler ? profiler->GetAllocatorStatsSnapshots() : Vector<AllocatorStats>{};
}

Vector<AllocatorStats> MemoryManager::SnapshotAllocatorStats()
{
    Vector<AllocatorStats> stats;
    const std::vector<NamedAllocatorStats> snapshots = SnapshotRegisteredAllocatorStats(true);
    stats.reserve(snapshots.size());
    for (const NamedAllocatorStats& snapshot : snapshots) {
        stats.push_back(snapshot.stats);
    }
    return stats;
}

std::string MemoryManager::DumpAllocatorStatsJson()
{
    const std::vector<NamedAllocatorStats> snapshots = SnapshotRegisteredAllocatorStats(true);

    std::string out;
    out.reserve(128 + snapshots.size() * 320);
    out += "{\"allocators\":[";
    for (size_t i = 0; i < snapshots.size(); ++i) {
        const NamedAllocatorStats& snapshot = snapshots[i];
        const AllocatorStats& stats = snapshot.stats;
        out += "{\"name\":\"";
        AppendJsonEscaped(out, snapshot.name);
        out += "\",";
        AppendJsonField(out, "liveBytes", stats.liveBytes, true);
        AppendJsonField(out, "peakBytes", stats.peakBytes, true);
        AppendJsonField(out, "allocationCount", stats.allocationCount, true);
        AppendJsonField(out, "freeCount", stats.freeCount, true);
        AppendJsonField(out, "failedAllocationCount", stats.failedAllocationCount, true);
        AppendJsonField(out, "liveAllocationCount", stats.liveAllocationCount, true);
        AppendJsonField(out, "reservedBytes", stats.reservedBytes, true);
        AppendJsonField(out, "committedBytes", stats.committedBytes, true);
        AppendJsonField(out, "freeBytes", stats.freeBytes, true);
        AppendJsonField(out, "largestFreeBlockBytes", stats.largestFreeBlockBytes, true);
        AppendJsonField(out, "fragmentationBytes", stats.fragmentationBytes, false);
        out += "}";
        if (i + 1 < snapshots.size()) {
            out += ",";
        }
    }
    out += "]}";
    return out;
}

std::string MemoryManager::DumpAllocatorStatsText()
{
    const std::vector<NamedAllocatorStats> snapshots = SnapshotRegisteredAllocatorStats(true);

    std::string out;
    out.reserve(snapshots.size() * 240);
    for (const NamedAllocatorStats& snapshot : snapshots) {
        const AllocatorStats& stats = snapshot.stats;
        out += snapshot.name;
        AppendTextField(out, "live", stats.liveBytes);
        AppendTextField(out, "peak", stats.peakBytes);
        AppendTextField(out, "allocs", stats.allocationCount);
        AppendTextField(out, "frees", stats.freeCount);
        AppendTextField(out, "failures", stats.failedAllocationCount);
        AppendTextField(out, "liveAllocs", stats.liveAllocationCount);
        AppendTextField(out, "reserved", stats.reservedBytes);
        AppendTextField(out, "committed", stats.committedBytes);
        AppendTextField(out, "free", stats.freeBytes);
        AppendTextField(out, "largestFree", stats.largestFreeBlockBytes);
        AppendTextField(out, "fragmentation", stats.fragmentationBytes);
        out += "\n";
    }
    return out;
}

bool MemoryManager::IsProfilingSuppressed() noexcept
{
    return g_profilingSuppressionDepth != 0;
}

void MemoryManager::PushProfilingSuppression() noexcept
{
    ++g_profilingSuppressionDepth;
}

void MemoryManager::PopProfilingSuppression() noexcept
{
    if (g_profilingSuppressionDepth > 0) {
        --g_profilingSuppressionDepth;
    }
}

} // namespace Engine::Memory
