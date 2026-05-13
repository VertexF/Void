#include <Foundation/Memory/Allocators/Advanced/ThreadLocalLinearAllocator.hpp>

namespace Engine::Memory {

namespace {
struct LocalLinearState {
    const ThreadLocalLinearAllocator* owner = nullptr;
    LinearAllocator* allocator = nullptr;
};

thread_local LocalLinearState t_localLinearState;
}

ThreadLocalLinearAllocator::ThreadLocalLinearAllocator(size_t perThreadSize, IAllocator* backingAllocator)
    : m_perThreadSize(perThreadSize)
    , m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
}

ThreadLocalLinearAllocator::~ThreadLocalLinearAllocator()
{
    if (t_localLinearState.owner == this) {
        Delete(GetDefaultAllocator(), t_localLinearState.allocator);
        t_localLinearState.allocator = nullptr;
        t_localLinearState.owner = nullptr;
    }
}

void* ThreadLocalLinearAllocator::Allocate(size_t size, size_t alignment)
{
    return GetLocalAllocator().Allocate(size, alignment);
}

void* ThreadLocalLinearAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    return GetLocalAllocator().Reallocate(ptr, newSize, alignment);
}

void ThreadLocalLinearAllocator::Free(void* ptr)
{
    GetLocalAllocator().Free(ptr);
}

size_t ThreadLocalLinearAllocator::AllocatedSize() const
{
    return GetLocalAllocator().AllocatedSize();
}

const char* ThreadLocalLinearAllocator::Name() const
{
    return "ThreadLocalLinearAllocator";
}

bool ThreadLocalLinearAllocator::Owns(void* ptr) const
{
    return GetLocalAllocator().Owns(ptr);
}

void ThreadLocalLinearAllocator::Reset()
{
    GetLocalAllocator().Reset();
}

LinearAllocator& ThreadLocalLinearAllocator::GetLocalAllocator() const
{
    if (t_localLinearState.owner != this) {
        if (t_localLinearState.allocator) {
            Delete(GetDefaultAllocator(), t_localLinearState.allocator);
        }
        t_localLinearState.allocator = New<LinearAllocator>(GetDefaultAllocator(), m_perThreadSize, m_backingAllocator);
        t_localLinearState.owner = this;
    }
    return *t_localLinearState.allocator;
}

} // namespace Engine::Memory
