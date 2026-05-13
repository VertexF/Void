#ifndef FOUNDATION_MEMORY_MANAGER_HDR
#define FOUNDATION_MEMORY_MANAGER_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/MemoryTag.hpp>
#include <Foundation/String.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <string>
#include <unordered_map>

namespace Engine::Memory {

class MemoryProfiler;

class MemoryManager final {
public:
    using OOMCallback = void(*)(MemoryTag tag, size_t requestedSize, size_t currentUsage, size_t budget);

    static void Initialize();
    static void Shutdown();
    static bool IsInitialized();
    static void RegisterAllocator(StringView name, IAllocator* allocator);
    static void UnregisterAllocator(StringView name);
    static IAllocator* AllocatorByName(StringView name);
    static bool HasAllocator(StringView name);
    static void Budget(MemoryTag tag, size_t budgetLimit);
    static bool IsBudgetExceeded(MemoryTag tag);
    static bool ReportBudgetPressure(MemoryTag tag, size_t requestedSize);
    static size_t BudgetUsage(MemoryTag tag);
    static void RegisterOOMCallback(OOMCallback callback);
    static void Profiler(MemoryProfiler* profiler);
    static MemoryProfiler* Profiler();
    static bool IsProfilingSuppressed() noexcept;
    static void PushProfilingSuppression() noexcept;
    static void PopProfilingSuppression() noexcept;

private:
    static std::string ToString(StringView name);

    std::unordered_map<std::string, IAllocator*> m_allocators;
    size_t m_budgets[static_cast<size_t>(MemoryTag::Count)] = {0};
    OOMCallback m_oomCallback = nullptr;
    MemoryProfiler* m_profiler = nullptr;
    mutable Threading::SpinLock m_lock;
};

class MemoryProfilingSuppressionScope final {
public:
    MemoryProfilingSuppressionScope() noexcept { MemoryManager::PushProfilingSuppression(); }
    ~MemoryProfilingSuppressionScope() noexcept { MemoryManager::PopProfilingSuppression(); }

    MemoryProfilingSuppressionScope(const MemoryProfilingSuppressionScope&) = delete;
    MemoryProfilingSuppressionScope& operator=(const MemoryProfilingSuppressionScope&) = delete;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_MANAGER_HDR
