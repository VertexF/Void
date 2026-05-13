#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Allocators/PoolAllocator.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

PoolAllocator::PoolAllocator(size_t blockSize, size_t blockCount, IAllocator* backingAllocator, size_t blockAlignment)
    : m_blockSize(blockSize < sizeof(FreeBlock) ? sizeof(FreeBlock) : blockSize),
      m_blockCount(blockCount),
      m_blockAlignment(blockAlignment),
      m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
    if (!IsPowerOfTwo(m_blockAlignment)) {
        m_blockAlignment = alignof(MaxAlignT);
    }
    if (m_blockAlignment < alignof(MaxAlignT)) {
        m_blockAlignment = alignof(MaxAlignT);
    }

    if (m_blockCount == 0 || m_blockSize == 0 || !m_backingAllocator) {
        return;
    }

    m_blockSize = AlignSize(m_blockSize, m_blockAlignment);

    const size_t bufferSize = m_blockSize * m_blockCount;
    m_buffer = m_backingAllocator->Allocate(bufferSize, m_blockAlignment);

    m_allocatedBitmapBytes = (m_blockCount + 7u) / 8u;
    if (m_allocatedBitmapBytes > 0) {
        m_allocatedBitmap = static_cast<uint8*>(
            m_backingAllocator->Allocate(m_allocatedBitmapBytes, alignof(uint8)));
        if (m_allocatedBitmap) {
            MemSet(m_allocatedBitmap, 0, m_allocatedBitmapBytes);
        }
    }

    m_blockTags = static_cast<MemoryTag*>(
        m_backingAllocator->Allocate(m_blockCount * sizeof(MemoryTag), alignof(MemoryTag)));
    if (m_blockTags) {
        for (size_t i = 0; i < m_blockCount; ++i) {
            m_blockTags[i] = MemoryTag::Default;
        }
    }

    InitializeFreeList();
}

PoolAllocator::~PoolAllocator()
{
    if (m_backingAllocator) {
        if (m_blockTags) {
            m_backingAllocator->Free(m_blockTags);
            m_blockTags = nullptr;
        }
        if (m_allocatedBitmap) {
            m_backingAllocator->Free(m_allocatedBitmap);
            m_allocatedBitmap = nullptr;
        }
        if (m_buffer) {
            m_backingAllocator->Free(m_buffer);
            m_buffer = nullptr;
        }
    }

    m_freeList.Store(nullptr, MemoryOrder::Relaxed);
    m_allocatedCount.Store(0, MemoryOrder::Relaxed);
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
    : m_buffer(other.m_buffer),
      m_blockSize(other.m_blockSize),
      m_blockCount(other.m_blockCount),
      m_blockAlignment(other.m_blockAlignment),
      m_backingAllocator(other.m_backingAllocator),
      m_allocatedBitmap(other.m_allocatedBitmap),
      m_allocatedBitmapBytes(other.m_allocatedBitmapBytes),
      m_blockTags(other.m_blockTags)
{
    m_allocatedCount.Store(other.m_allocatedCount.Load(MemoryOrder::Relaxed), MemoryOrder::Relaxed);
    m_freeList.Store(other.m_freeList.Load(MemoryOrder::Relaxed), MemoryOrder::Relaxed);

    other.m_buffer = nullptr;
    other.m_allocatedBitmap = nullptr;
    other.m_blockTags = nullptr;
    other.m_freeList.Store(nullptr, MemoryOrder::Relaxed);
    other.m_blockCount = 0;
    other.m_allocatedCount.Store(0, MemoryOrder::Relaxed);
    other.m_allocatedBitmapBytes = 0;
}

PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept
{
    if (this != &other) {
        // Free current resources
        if (m_backingAllocator) {
            if (m_blockTags) {
                m_backingAllocator->Free(m_blockTags);
            }
            if (m_allocatedBitmap) {
                m_backingAllocator->Free(m_allocatedBitmap);
            }
            if (m_buffer) {
                m_backingAllocator->Free(m_buffer);
            }
        }

        m_buffer = other.m_buffer;
        m_blockSize = other.m_blockSize;
        m_blockCount = other.m_blockCount;
        m_blockAlignment = other.m_blockAlignment;
        m_backingAllocator = other.m_backingAllocator;
        m_allocatedBitmap = other.m_allocatedBitmap;
        m_allocatedBitmapBytes = other.m_allocatedBitmapBytes;
        m_blockTags = other.m_blockTags;
        
        m_allocatedCount.Store(other.m_allocatedCount.Load(MemoryOrder::Relaxed), MemoryOrder::Relaxed);
        m_freeList.Store(other.m_freeList.Load(MemoryOrder::Relaxed), MemoryOrder::Relaxed);

        other.m_buffer = nullptr;
        other.m_allocatedBitmap = nullptr;
        other.m_blockTags = nullptr;
        other.m_freeList.Store(nullptr, MemoryOrder::Relaxed);
        other.m_blockCount = 0;
        other.m_allocatedCount.Store(0, MemoryOrder::Relaxed);
        other.m_allocatedBitmapBytes = 0;
    }
    return *this;
}

void* PoolAllocator::Allocate(size_t size, size_t alignment)
{
    if (size == 0 || size > m_blockSize) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "PoolAllocator: invalid allocation size");
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        alignment = m_blockAlignment;
    }
    if (alignment > m_blockAlignment) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "PoolAllocator: unsupported alignment");
        return nullptr;
    }

    Threading::SpinLockGuard guard(m_lock);

    FreeBlock* block = m_freeList.Load(MemoryOrder::Relaxed);
    if (!block) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "PoolAllocator: pool exhausted");
        return nullptr;
    }

    m_freeList.Store(block->next, MemoryOrder::Relaxed);
    m_allocatedCount.FetchAdd(1, MemoryOrder::Relaxed);

    const size_t index = (static_cast<uint8*>(static_cast<void*>(block)) -
                          static_cast<uint8*>(m_buffer)) / m_blockSize;
    
    if (m_allocatedBitmap) {
        SetAllocatedIndex(index, true);
    }
    if (m_blockTags) {
        m_blockTags[index] = GetCurrentMemoryTag();
    }

    m_stats.RecordAllocation(m_blockSize);
    return block;
}

void* PoolAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!Owns(ptr)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "PoolAllocator: reallocate pointer is not live");
        return nullptr;
    }

    if (!IsPowerOfTwo(alignment)) {
        alignment = m_blockAlignment;
    }

    // Pool blocks are fixed size. If new size fits, we just return the same block.
    if (newSize <= m_blockSize && alignment <= m_blockAlignment) {
        return ptr;
    }

    // Resizing beyond block size is not supported by PoolAllocator.
    // We could potentially allocate a new block and copy, but usually 
    // pool users expect fixed size. If we return nullptr, user handles it.
    ReportAllocatorFailure(m_stats, AllocatorFailureKind::UnsupportedOperation, "PoolAllocator: resize exceeds fixed block size");
    return nullptr;
}

void PoolAllocator::Free(void* ptr)
{
    if (!ptr) {
        return;
    }

    Threading::SpinLockGuard guard(m_lock);
    const auto* start = static_cast<const uint8*>(m_buffer);
    const auto* end = start + (m_blockSize * m_blockCount);
    const auto* p = static_cast<const uint8*>(ptr);
    if (!m_buffer || p < start || p >= end) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "PoolAllocator: free pointer outside pool");
        return;
    }

    const size_t offset = static_cast<size_t>(p - start);
    if ((offset % m_blockSize) != 0) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "PoolAllocator: free pointer is not a block start");
        return;
    }

    const size_t index = offset / m_blockSize;
    if (m_allocatedBitmap) {
        const size_t byteIndex = index / 8;
        const uint8 mask = static_cast<uint8>(1u << (index % 8));
        if ((m_allocatedBitmap[byteIndex] & mask) == 0) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::DoubleFree, "PoolAllocator: invalid or double free");
            return;
        }
    }

    if (m_allocatedBitmap) {
        SetAllocatedIndex(index, false);
    }
    if (m_blockTags) {
        m_blockTags[index] = MemoryTag::Default;
    }

    FreeBlock* block = static_cast<FreeBlock*>(ptr);
    block->next = m_freeList.Load(MemoryOrder::Relaxed);
    m_freeList.Store(block, MemoryOrder::Relaxed);

    m_allocatedCount.FetchSub(1, MemoryOrder::Relaxed);
    m_stats.RecordFree(m_blockSize);
}

size_t PoolAllocator::AllocatedSize() const
{
    return m_allocatedCount.Load(MemoryOrder::Relaxed) * m_blockSize;
}

const char* PoolAllocator::Name() const
{
    return "PoolAllocator";
}

bool PoolAllocator::Owns(void* ptr) const
{
    if (!m_buffer || !ptr) {
        return false;
    }
    Threading::SpinLockGuard guard(m_lock);
    const auto* start = static_cast<const uint8*>(m_buffer);
    const auto* end = start + (m_blockSize * m_blockCount);
    const auto* p = static_cast<const uint8*>(ptr);
    if (p < start || p >= end) {
        return false;
    }

    const size_t offset = static_cast<size_t>(p - start);
    if ((offset % m_blockSize) != 0) {
        return false;
    }

    const size_t index = offset / m_blockSize;
    if (!m_allocatedBitmap) {
        return false;
    }
    const size_t byteIndex = index / 8;
    const uint8 mask = static_cast<uint8>(1u << (index % 8));
    return (m_allocatedBitmap[byteIndex] & mask) != 0;
}

AllocatorStats PoolAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    stats.liveBytes = AllocatedSize();
    stats.liveAllocationCount = GetAllocatedBlockCount();
    return stats;
}

AllocatorStats PoolAllocator::GetDetailedStats() const
{
    AllocatorStats stats = GetStats();
    const size_t freeBlocks = GetFreeBlockCount();
    stats.reservedBytes = m_blockSize * m_blockCount;
    stats.committedBytes = stats.reservedBytes;
    stats.freeBytes = freeBlocks * m_blockSize;
    stats.largestFreeBlockBytes = freeBlocks > 0 ? m_blockSize : 0;
    stats.fragmentationBytes = stats.freeBytes > stats.largestFreeBlockBytes
        ? stats.freeBytes - stats.largestFreeBlockBytes
        : 0;
    return stats;
}

size_t PoolAllocator::GetBlockSize() const
{
    return m_blockSize;
}

size_t PoolAllocator::GetBlockCount() const
{
    return m_blockCount;
}

size_t PoolAllocator::GetFreeBlockCount() const
{
    const size_t allocated = m_allocatedCount.Load(MemoryOrder::Relaxed);
    return m_blockCount > allocated ? m_blockCount - allocated : 0;
}

size_t PoolAllocator::GetAllocatedBlockCount() const
{
    return m_allocatedCount.Load(MemoryOrder::Relaxed);
}

bool PoolAllocator::IsFull() const
{
    return m_allocatedCount.Load(MemoryOrder::Relaxed) >= m_blockCount;
}

void PoolAllocator::Reset()
{
    Threading::SpinLockGuard guard(m_lock);
    InitializeFreeList();
    if (m_allocatedBitmap) {
        MemSet(m_allocatedBitmap, 0, m_allocatedBitmapBytes);
    }
    if (m_blockTags) {
        for (size_t i = 0; i < m_blockCount; ++i) {
            m_blockTags[i] = MemoryTag::Default;
        }
    }
    m_allocatedCount.Store(0, MemoryOrder::Relaxed);
    m_stats.ResetLive();
}

void PoolAllocator::InitializeFreeList()
{
    // Usually called inside a lock or from constructor
    if (!m_buffer || m_blockCount == 0) {
        m_freeList.Store(nullptr, MemoryOrder::Relaxed);
        return;
    }

    auto* bufferBytes = static_cast<uint8*>(m_buffer);
    FreeBlock* head = nullptr;

    for (size_t i = 0; i < m_blockCount; ++i) {
        auto* block = reinterpret_cast<FreeBlock*>(bufferBytes + i * m_blockSize);
        block->next = head;
        head = block;
    }

    m_freeList.Store(head, MemoryOrder::Relaxed);
}

bool PoolAllocator::IsAllocatedIndex(size_t index) const noexcept
{
    // For AAA, we'd use atomics for everything, but here IsAllocatedIndex is 
    // mostly used for debugging/checks. We'll use the lock for safety if called concurrently.
    Threading::SpinLockGuard guard(m_lock);
    if (!m_allocatedBitmap || index >= m_blockCount) {
        return false;
    }
    const size_t byteIndex = index / 8;
    const uint8 mask = static_cast<uint8>(1u << (index % 8));
    return (m_allocatedBitmap[byteIndex] & mask) != 0;
}

void PoolAllocator::SetAllocatedIndex(size_t index, bool value) noexcept
{
    // Assumed to be called inside a lock
    if (!m_allocatedBitmap || index >= m_blockCount) {
        return;
    }
    const size_t byteIndex = index / 8;
    const uint8 mask = static_cast<uint8>(1u << (index % 8));
    if (value) {
        m_allocatedBitmap[byteIndex] |= mask;
    } else {
        m_allocatedBitmap[byteIndex] &= static_cast<uint8>(~mask);
    }
}

} // namespace Engine::Memory
