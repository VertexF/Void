#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/Allocators/Advanced/TrackedAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

namespace {
constexpr size_t kTrackedMagic = 0x54524143u; // TRAC
}

TrackedAllocator::TrackedAllocator(IAllocator* backingAllocator)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
}

void* TrackedAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_backingAllocator || size == 0) {
        m_failedAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        alignment = alignof(MaxAlignT);
    }

    const size_t headerSize = sizeof(AllocationHeader);
    const size_t totalSize = size + headerSize + alignment;
    void* raw = m_backingAllocator->Allocate(totalSize, alignment);
    if (!raw) {
        m_failedAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
        return nullptr;
    }

    byte* rawBytes = static_cast<byte*>(raw);
    byte* aligned = static_cast<byte*>(AlignPointer(rawBytes + headerSize, alignment));
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - headerSize);
    header->size = size;
    header->adjustment = static_cast<size_t>(aligned - rawBytes);
    header->tag = GetCurrentMemoryTag();
    header->padding = kTrackedMagic;

    const size_t newTotal = m_allocatedBytes.FetchAdd(size, MemoryOrder::Relaxed) + size;
    size_t peak = m_peakBytes.Load(MemoryOrder::Relaxed);
    while (newTotal > peak &&
           !m_peakBytes.CompareExchangeWeak(peak, newTotal, MemoryOrder::Relaxed)) {
    }

    m_allocationCount.FetchAdd(1, MemoryOrder::Relaxed);
    {
        Threading::SpinLockGuard guard(m_liveLock);
        m_liveAllocations.insert(aligned);
    }
    return aligned;
}

void* TrackedAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!Owns(ptr)) {
        m_failedAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
        return nullptr;
    }

    byte* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));
    if (header->padding != kTrackedMagic) {
        return nullptr;
    }
    size_t oldSize = header->size;

    if (oldSize == newSize) {
        return ptr;
    }

    // Fallback: Allocate new, copy, free old
    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        size_t copySize = (oldSize < newSize) ? oldSize : newSize;
        MemCopy(newPtr, ptr, copySize);
        Free(ptr);
    } else {
        m_failedAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
    }
    return newPtr;
}

void TrackedAllocator::Free(void* ptr)
{
    if (!m_backingAllocator || !ptr) {
        return;
    }

    {
        Threading::SpinLockGuard guard(m_liveLock);
        const auto it = m_liveAllocations.find(ptr);
        if (it == m_liveAllocations.end()) {
            m_failedAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
            return;
        }
        m_liveAllocations.erase(it);
    }

    byte* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));
    if (header->padding != kTrackedMagic) {
        return;
    }
    const size_t size = header->size;

    m_allocatedBytes.FetchSub(size, MemoryOrder::Relaxed);
    m_allocationCount.FetchSub(1, MemoryOrder::Relaxed);
    m_freeCount.FetchAdd(1, MemoryOrder::Relaxed);

    byte* raw = aligned - header->adjustment;
    m_backingAllocator->Free(raw);
}

size_t TrackedAllocator::AllocatedSize() const
{
    return m_allocatedBytes.Load(MemoryOrder::Relaxed);
}

const char* TrackedAllocator::Name() const
{
    return "TrackedAllocator";
}

bool TrackedAllocator::Owns(void* ptr) const
{
    if (!ptr) {
        return false;
    }
    Threading::SpinLockGuard guard(m_liveLock);
    return m_liveAllocations.find(ptr) != m_liveAllocations.end();
}

AllocatorStats TrackedAllocator::GetStats() const
{
    AllocatorStats stats{};
    stats.name = Name();
    stats.liveBytes = m_allocatedBytes.Load(MemoryOrder::Relaxed);
    stats.peakBytes = m_peakBytes.Load(MemoryOrder::Relaxed);
    stats.allocationCount = m_allocationCount.Load(MemoryOrder::Relaxed) + m_freeCount.Load(MemoryOrder::Relaxed);
    stats.freeCount = m_freeCount.Load(MemoryOrder::Relaxed);
    stats.failedAllocationCount = m_failedAllocationCount.Load(MemoryOrder::Relaxed);
    stats.liveAllocationCount = m_allocationCount.Load(MemoryOrder::Relaxed);
    return stats;
}

} // namespace Engine::Memory
