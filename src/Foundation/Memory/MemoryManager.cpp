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
}

std::string MemoryManager::ToString(StringView name)
{
    if (!name.text || name.length == 0) {
        return {};
    }
    return std::string(name.text, name.length);
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
