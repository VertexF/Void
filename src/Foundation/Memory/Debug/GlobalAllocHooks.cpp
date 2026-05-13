// Overrides global operator new and operator delete to route allocation
// tracking through MemoryProfiler. Uses a lock-free atomic pointer for
// the profiler (set once at startup, read on every alloc/free) and a
// thread-local re-entrancy guard to prevent infinite recursion from the
// profiler's own internal allocations (e.g. HashMap growth).
// ============================================================================

#include <Foundation/Memory/Debug/MemoryProfiler.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Threading/Atomic.hpp>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
    #include <malloc.h>
#endif

using Engine::MemoryOrder;
using Engine::usize;

namespace Engine::Memory {

// Lock-free global profiler pointer. Set once by SetGlobalAllocProfiler(),
// read on every new/delete with relaxed ordering (eventual consistency is fine).
static Atomic<MemoryProfiler*> g_allocProfiler{nullptr};

void SetGlobalAllocProfiler(MemoryProfiler* profiler) noexcept
{
    g_allocProfiler.Store(profiler, MemoryOrder::Release);
}

MemoryProfiler* GetGlobalAllocProfiler() noexcept
{
    return g_allocProfiler.Load(MemoryOrder::Acquire);
}

} // namespace Engine::Memory

namespace {

// Thread-local re-entrancy guard. When the profiler itself allocates
// (e.g. HashMap internals), we must not recurse back into it.
thread_local bool g_inProfiler = false;

struct ProfilerGuard {
    ProfilerGuard() noexcept { g_inProfiler = true; }
    ~ProfilerGuard() noexcept { g_inProfiler = false; }
};

[[noreturn]] void HandleOutOfMemory() noexcept
{
    abort();
}

void TrackGlobalAlloc(void* ptr, usize size) noexcept
{
    if (!ptr || g_inProfiler || Engine::Memory::MemoryManager::IsProfilingSuppressed()) {
        return;
    }

    if (auto* p = Engine::Memory::GetGlobalAllocProfiler()) {
        ProfilerGuard guard;
        p->TrackAlloc(ptr, size);
    }
}

void TrackGlobalFree(void* ptr, usize size) noexcept
{
    if (!ptr || g_inProfiler || Engine::Memory::MemoryManager::IsProfilingSuppressed()) {
        return;
    }

    if (auto* p = Engine::Memory::GetGlobalAllocProfiler()) {
        ProfilerGuard guard;
        p->TrackFree(ptr, size);
    }
}

void* AllocateDefaultAligned(usize size) noexcept
{
    const usize allocSize = (size > 0) ? size : 1;
    return malloc(allocSize);
}

void FreeDefaultAligned(void* ptr) noexcept
{
    free(ptr);
}

void* AllocateOverAligned(usize size, std::align_val_t alignment) noexcept
{
    const usize align = static_cast<usize>(alignment);
    const usize allocSize = (size > 0) ? size : 1;
#if defined(_MSC_VER)
    return _aligned_malloc(allocSize, align);
#else
    const usize roundedSize = (allocSize + align - 1) & ~(align - 1);
    return aligned_alloc(align, roundedSize);
#endif
}

void FreeOverAligned(void* ptr) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

} // namespace

void* operator new(usize size)
{
    void* ptr = AllocateDefaultAligned(size);
    if (!ptr) {
        HandleOutOfMemory();
    }

    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new(usize size, const std::nothrow_t&) noexcept
{
    void* ptr = AllocateDefaultAligned(size);
    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new[](usize size)
{
    void* ptr = AllocateDefaultAligned(size);
    if (!ptr) {
        HandleOutOfMemory();
    }

    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new[](usize size, const std::nothrow_t&) noexcept
{
    void* ptr = AllocateDefaultAligned(size);
    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new(usize size, std::align_val_t alignment)
{
    void* ptr = AllocateOverAligned(size, alignment);
    if (!ptr) {
        HandleOutOfMemory();
    }

    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new(usize size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    void* ptr = AllocateOverAligned(size, alignment);
    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new[](usize size, std::align_val_t alignment)
{
    void* ptr = AllocateOverAligned(size, alignment);
    if (!ptr) {
        HandleOutOfMemory();
    }

    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void* operator new[](usize size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    void* ptr = AllocateOverAligned(size, alignment);
    TrackGlobalAlloc(ptr, size);
    return ptr;
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    operator delete(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    operator delete[](ptr);
}

void operator delete(void* ptr) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, 0);
    FreeDefaultAligned(ptr);
}

void operator delete(void* ptr, usize size) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, size);
    FreeDefaultAligned(ptr);
}

void operator delete[](void* ptr) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, 0);
    FreeDefaultAligned(ptr);
}

void operator delete[](void* ptr, usize size) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, size);
    FreeDefaultAligned(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, 0);
    FreeOverAligned(ptr);
}

void operator delete(void* ptr, usize size, std::align_val_t) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, size);
    FreeOverAligned(ptr);
}

void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, 0);
    FreeOverAligned(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, 0);
    FreeOverAligned(ptr);
}

void operator delete[](void* ptr, usize size, std::align_val_t) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, size);
    FreeOverAligned(ptr);
}

void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept
{
    if (!ptr) return;
    TrackGlobalFree(ptr, 0);
    FreeOverAligned(ptr);
}
