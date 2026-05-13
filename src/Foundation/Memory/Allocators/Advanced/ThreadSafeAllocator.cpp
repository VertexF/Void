#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocator.hpp>

namespace Engine::Memory {

ThreadSafeAllocator::ThreadSafeAllocator(IAllocator* backingAllocator)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
}

void* ThreadSafeAllocator::Allocate(size_t size, size_t alignment)
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        return nullptr;
    }
    return m_backingAllocator->Allocate(size, alignment);
}

void* ThreadSafeAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        return nullptr;
    }
    return m_backingAllocator->Reallocate(ptr, newSize, alignment);
}

void ThreadSafeAllocator::Free(void* ptr)
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        return;
    }
    m_backingAllocator->Free(ptr);
}

size_t ThreadSafeAllocator::AllocatedSize() const
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        return 0;
    }
    return m_backingAllocator->AllocatedSize();
}

const char* ThreadSafeAllocator::Name() const
{
    return "ThreadSafeAllocator";
}

bool ThreadSafeAllocator::Owns(void* ptr) const
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        return false;
    }
    return m_backingAllocator->Owns(ptr);
}

AllocatorStats ThreadSafeAllocator::GetStats() const
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        AllocatorStats stats{};
        stats.name = Name();
        return stats;
    }
    AllocatorStats stats = m_backingAllocator->GetStats();
    stats.name = Name();
    return stats;
}

AllocatorStats ThreadSafeAllocator::GetDetailedStats() const
{
    Threading::LockGuard guard(m_mutex);
    if (!m_backingAllocator) {
        AllocatorStats stats{};
        stats.name = Name();
        return stats;
    }
    AllocatorStats stats = m_backingAllocator->GetDetailedStats();
    stats.name = Name();
    return stats;
}

IAllocator* ThreadSafeAllocator::GetBackingAllocator() const noexcept
{
    Threading::LockGuard guard(m_mutex);
    return m_backingAllocator;
}

void ThreadSafeAllocator::SetBackingAllocator(IAllocator* allocator) noexcept
{
    Threading::LockGuard guard(m_mutex);
    m_backingAllocator = allocator;
}

} // namespace Engine::Memory
