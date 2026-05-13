#ifndef FOUNDATION_MEMORY_DEBUG_GLOBAL_ALLOC_HOOKS_HDR
#define FOUNDATION_MEMORY_DEBUG_GLOBAL_ALLOC_HOOKS_HDR

#include <Utility/Macros.hpp>

namespace Engine::Memory {

class MemoryProfiler;

/// Set the profiler that global operator new/delete will report to.
/// Pass nullptr to disable tracking. Lock-free (atomic pointer store).
ENGINE_API void SetGlobalAllocProfiler(MemoryProfiler* profiler) noexcept;

/// Get the currently active global allocation profiler (lock-free read).
ENGINE_API MemoryProfiler* GetGlobalAllocProfiler() noexcept;

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_DEBUG_GLOBAL_ALLOC_HOOKS_HDR