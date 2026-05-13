#include <Foundation/Memory/Allocators/Advanced/ThreadLocalLinearAllocator.hpp>

namespace Engine::Memory {

struct ThreadLocalLinearAllocatorState {
    ThreadLocalLinearAllocator* owner = nullptr;
    LinearAllocator* allocator = nullptr;
    ThreadLocalLinearAllocatorState* previous = nullptr;
    ThreadLocalLinearAllocatorState* next = nullptr;

    ~ThreadLocalLinearAllocatorState()
    {
        Release();
    }

    void Release()
    {
        ThreadLocalLinearAllocator* currentOwner = owner;
        if (currentOwner && allocator) {
            const size_t liveBytes = allocator->AllocatedSize();
            if (liveBytes > 0) {
                currentOwner->ReleaseTrackedBytes(liveBytes);
                currentOwner->m_stats.RecordFree(liveBytes);
            }
        }
        if (currentOwner) {
            currentOwner->UnregisterState(this);
        }
        if (allocator) {
            Delete(GetDefaultAllocator(), allocator);
        }
        owner = nullptr;
        allocator = nullptr;
        previous = nullptr;
        next = nullptr;
    }
};

namespace {
thread_local ThreadLocalLinearAllocatorState t_localLinearState;
}

ThreadLocalLinearAllocator::ThreadLocalLinearAllocator(size_t perThreadSize, IAllocator* backingAllocator)
    : m_perThreadSize(perThreadSize)
    , m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
}

ThreadLocalLinearAllocator::~ThreadLocalLinearAllocator()
{
    Threading::SpinLockGuard guard(m_stateLock);
    ThreadLocalLinearAllocatorState* state = m_stateHead;
    while (state) {
        ThreadLocalLinearAllocatorState* next = state->next;
        if (state->allocator) {
            Delete(GetDefaultAllocator(), state->allocator);
        }
        state->owner = nullptr;
        state->allocator = nullptr;
        state->previous = nullptr;
        state->next = nullptr;
        state = next;
    }
    m_stateHead = nullptr;
}

void* ThreadLocalLinearAllocator::Allocate(size_t size, size_t alignment)
{
    LinearAllocator* allocator = GetLocalAllocator();
    if (!allocator) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    const size_t before = allocator->AllocatedSize();
    void* ptr = allocator->Allocate(size, alignment);
    if (!ptr) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    const size_t after = allocator->AllocatedSize();
    const size_t consumed = after >= before ? after - before : size;
    AddTrackedBytes(consumed);
    m_stats.RecordAllocation(consumed);
    return ptr;
}

void* ThreadLocalLinearAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    LinearAllocator* allocator = GetLocalAllocator();
    if (!allocator) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    const size_t before = allocator->AllocatedSize();
    void* result = allocator->Reallocate(ptr, newSize, alignment);
    if (!result && newSize != 0) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    const size_t after = allocator->AllocatedSize();
    if (after > before) {
        AddTrackedBytes(after - before);
    } else if (before > after) {
        ReleaseTrackedBytes(before - after);
    }
    m_stats.RecordResize(before, after);
    return result;
}

void ThreadLocalLinearAllocator::Free(void* ptr)
{
    LinearAllocator* allocator = FindLocalAllocator();
    if (allocator) {
        allocator->Free(ptr);
    }
}

size_t ThreadLocalLinearAllocator::AllocatedSize() const
{
    return m_allocatedBytes.Load(MemoryOrder::Relaxed);
}

const char* ThreadLocalLinearAllocator::Name() const
{
    return "ThreadLocalLinearAllocator";
}

bool ThreadLocalLinearAllocator::Owns(void* ptr) const
{
    LinearAllocator* allocator = FindLocalAllocator();
    return allocator ? allocator->Owns(ptr) : false;
}

AllocatorStats ThreadLocalLinearAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    stats.liveBytes = AllocatedSize();
    if (stats.peakBytes < stats.liveBytes) {
        stats.peakBytes = stats.liveBytes;
    }
    return stats;
}

AllocatorStats ThreadLocalLinearAllocator::GetDetailedStats() const
{
    AllocatorStats stats = GetStats();
    size_t stateCount = 0;
    {
        Threading::SpinLockGuard guard(m_stateLock);
        for (ThreadLocalLinearAllocatorState* state = m_stateHead; state; state = state->next) {
            if (state->allocator) {
                ++stateCount;
            }
        }
    }
    stats.reservedBytes = stateCount * m_perThreadSize;
    stats.committedBytes = stats.reservedBytes;
    stats.freeBytes = stats.reservedBytes > stats.liveBytes ? stats.reservedBytes - stats.liveBytes : 0;
    stats.largestFreeBlockBytes = stats.freeBytes > 0 ? m_perThreadSize : 0;
    stats.fragmentationBytes = 0;
    return stats;
}

void ThreadLocalLinearAllocator::Reset()
{
    LinearAllocator* allocator = FindLocalAllocator();
    if (!allocator) {
        return;
    }

    const size_t before = allocator->AllocatedSize();
    allocator->Reset();
    if (before > 0) {
        ReleaseTrackedBytes(before);
        m_stats.RecordFree(before);
    }
}

LinearAllocator* ThreadLocalLinearAllocator::FindLocalAllocator() const noexcept
{
    return t_localLinearState.owner == this ? t_localLinearState.allocator : nullptr;
}

LinearAllocator* ThreadLocalLinearAllocator::GetLocalAllocator() const
{
    if (t_localLinearState.owner != this) {
        t_localLinearState.Release();
        t_localLinearState.allocator = New<LinearAllocator>(GetDefaultAllocator(), m_perThreadSize, m_backingAllocator);
        if (!t_localLinearState.allocator) {
            return nullptr;
        }
        t_localLinearState.owner = const_cast<ThreadLocalLinearAllocator*>(this);
        RegisterState(&t_localLinearState);
    }
    return t_localLinearState.allocator;
}

void ThreadLocalLinearAllocator::RegisterState(ThreadLocalLinearAllocatorState* state) const
{
    Threading::SpinLockGuard guard(m_stateLock);
    state->previous = nullptr;
    state->next = m_stateHead;
    if (m_stateHead) {
        m_stateHead->previous = state;
    }
    m_stateHead = state;
}

void ThreadLocalLinearAllocator::UnregisterState(ThreadLocalLinearAllocatorState* state) const
{
    Threading::SpinLockGuard guard(m_stateLock);
    if (state->previous) {
        state->previous->next = state->next;
    } else if (m_stateHead == state) {
        m_stateHead = state->next;
    }
    if (state->next) {
        state->next->previous = state->previous;
    }
    state->previous = nullptr;
    state->next = nullptr;
}

void ThreadLocalLinearAllocator::AddTrackedBytes(size_t bytes) noexcept
{
    if (bytes > 0) {
        m_allocatedBytes.FetchAdd(bytes, MemoryOrder::Relaxed);
    }
}

void ThreadLocalLinearAllocator::ReleaseTrackedBytes(size_t bytes) noexcept
{
    size_t current = m_allocatedBytes.Load(MemoryOrder::Relaxed);
    while (true) {
        const size_t desired = bytes <= current ? current - bytes : 0;
        if (m_allocatedBytes.CompareExchangeWeak(current, desired, MemoryOrder::Relaxed, MemoryOrder::Relaxed)) {
            return;
        }
    }
}

} // namespace Engine::Memory
