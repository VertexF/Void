#include <Foundation/Memory/Allocators/Advanced/ThreadSafeLinearAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Threading/Atomic.hpp>

#include <limits>

namespace Engine::Memory {

namespace {
constexpr size_t kMinAlignment = alignof(MaxAlignT);
constexpr uint32 kThreadSafeLinearMagic = 0x54534C41u; // TSLA
}

ThreadSafeLinearAllocator::ThreadSafeLinearAllocator(size_t size, IAllocator* backingAllocator)
    : m_capacity(size),
      m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
    if (m_capacity > 0) {
        m_buffer = m_backingAllocator->Allocate(m_capacity, alignof(MaxAlignT));
    }
}

ThreadSafeLinearAllocator::~ThreadSafeLinearAllocator()
{
    if (m_buffer && m_backingAllocator) {
        m_backingAllocator->Free(m_buffer);
    }
}

void* ThreadSafeLinearAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_buffer || size == 0) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        alignment = kMinAlignment;
    }
    if (alignment < kMinAlignment) {
        alignment = kMinAlignment;
    }

    size_t currentOffset = m_offset.Load(MemoryOrder::Acquire);
    size_t newOffset = 0;
    void* ptr = nullptr;

    while (true) {
        void* currentPtr = static_cast<uint8*>(m_buffer) + currentOffset;
        const size_t alignmentAdjustment = AlignmentAdjustmentWithHeader(
            currentPtr,
            alignment,
            sizeof(AllocationHeader));
        if (currentOffset > (std::numeric_limits<size_t>::max)() - alignmentAdjustment ||
            size > (std::numeric_limits<size_t>::max)() - currentOffset - alignmentAdjustment) {
            m_stats.RecordFailedAllocation();
            return nullptr;
        }
        void* alignedPtr = static_cast<uint8*>(currentPtr) + alignmentAdjustment;
        newOffset = currentOffset + alignmentAdjustment + size;
        
        if (newOffset > m_capacity) {
            m_stats.RecordFailedAllocation();
            return nullptr;
        }

        if (m_offset.CompareExchangeWeak(currentOffset, newOffset,
                                          MemoryOrder::Release,
                                          MemoryOrder::Acquire)) {
            ptr = alignedPtr;
            auto* header = reinterpret_cast<AllocationHeader*>(static_cast<uint8*>(ptr) - sizeof(AllocationHeader));
            header->size = size;
            header->generation = CurrentGeneration();
            header->magic = kThreadSafeLinearMagic;
            m_stats.RecordAllocation(newOffset - currentOffset);
            break;
        }
        // If compare_exchange fails, currentOffset is updated with the new value of m_offset
    }

    return ptr;
}

void* ThreadSafeLinearAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!IsLiveAllocation(ptr)) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    m_stats.RecordFailedAllocation();
    return nullptr;
}

void ThreadSafeLinearAllocator::Free(void*)
{
    // No-op for linear allocator
}

size_t ThreadSafeLinearAllocator::AllocatedSize() const
{
    return m_offset.Load(MemoryOrder::Relaxed);
}

const char* ThreadSafeLinearAllocator::Name() const
{
    return "ThreadSafeLinearAllocator";
}

bool ThreadSafeLinearAllocator::Owns(void* ptr) const
{
    return IsLiveAllocation(ptr);
}

AllocatorStats ThreadSafeLinearAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    stats.liveBytes = m_offset.Load(MemoryOrder::Relaxed);
    if (stats.peakBytes < stats.liveBytes) {
        stats.peakBytes = stats.liveBytes;
    }
    return stats;
}

AllocatorStats ThreadSafeLinearAllocator::GetDetailedStats() const
{
    AllocatorStats stats = GetStats();
    stats.reservedBytes = m_capacity;
    stats.committedBytes = m_capacity;
    stats.freeBytes = m_capacity >= stats.liveBytes ? m_capacity - stats.liveBytes : 0;
    stats.largestFreeBlockBytes = stats.freeBytes;
    return stats;
}

bool ThreadSafeLinearAllocator::IsLiveAllocation(void* ptr) const noexcept
{
    if (!m_buffer || !ptr) return false;
    const size_t offset = m_offset.Load(MemoryOrder::Acquire);
    const auto* start = static_cast<const uint8*>(m_buffer);
    const auto* end = start + offset;
    const auto* p = static_cast<const uint8*>(ptr);
    if (p < start + sizeof(AllocationHeader) || p >= end) {
        return false;
    }

    const auto* header = reinterpret_cast<const AllocationHeader*>(p - sizeof(AllocationHeader));
    if (header->magic != kThreadSafeLinearMagic || header->generation != CurrentGeneration()) {
        return false;
    }
    return header->size <= static_cast<size_t>(end - p);
}

void ThreadSafeLinearAllocator::Reset()
{
    m_offset.Store(0, MemoryOrder::Release);
    m_stats.ResetLive();
    AdvanceGeneration();
}

uint32 ThreadSafeLinearAllocator::CurrentGeneration() const noexcept
{
    return m_generation.Load(MemoryOrder::Acquire);
}

void ThreadSafeLinearAllocator::AdvanceGeneration() noexcept
{
    uint32 current = m_generation.Load(MemoryOrder::Acquire);
    uint32 next = current + 1;
    if (next == 0) {
        next = 1;
    }
    m_generation.Store(next, MemoryOrder::Release);
}

} // namespace Engine::Memory
