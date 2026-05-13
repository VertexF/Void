#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocator.hpp>

namespace Engine::Memory {

ThreadSafeAllocator::ThreadSafeAllocator(IAllocator* backingAllocator)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
}

void* ThreadSafeAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_backingAllocator) {
        return nullptr;
    }
    Threading::LockGuard guard(m_mutex);
    return m_backingAllocator->Allocate(size, alignment);
}

void* ThreadSafeAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (!m_backingAllocator) {
        return nullptr;
    }
    Threading::LockGuard guard(m_mutex);
    return m_backingAllocator->Reallocate(ptr, newSize, alignment);
}

void ThreadSafeAllocator::Free(void* ptr)
{
    if (!m_backingAllocator) {
        return;
    }
    Threading::LockGuard guard(m_mutex);
    m_backingAllocator->Free(ptr);
}

size_t ThreadSafeAllocator::AllocatedSize() const
{
    if (!m_backingAllocator) {
        return 0;
    }
    Threading::LockGuard guard(m_mutex);
    return m_backingAllocator->AllocatedSize();
}

const char* ThreadSafeAllocator::Name() const
{
    return "ThreadSafeAllocator";
}

bool ThreadSafeAllocator::Owns(void* ptr) const
{
    if (!m_backingAllocator) {
        return false;
    }
    Threading::LockGuard guard(m_mutex);
    return m_backingAllocator->Owns(ptr);
}

} // namespace Engine::Memory
