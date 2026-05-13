#include <algorithm>
// ============================================================================
// Engine - Memory Module
// Buddy Allocator Implementation
// ============================================================================

#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Allocators/Advanced/BuddyAllocator.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>
#include <Utility/Move.hpp>
#include <limits>
#include <utility>

namespace Engine::Memory {

namespace {
constexpr size_t kMinAlignment = alignof(MaxAlignT);
constexpr uint16 kBuddyMagic = 0xB00Du;

size_t NextPowerOfTwo(size_t value) noexcept
{
    if (value == 0) {
        return 1;
    }
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
#if SIZE_MAX > UINT32_MAX
    value |= value >> 32;
#endif
    return value + 1;
}
} // namespace

BuddyAllocator::BuddyAllocator(size_t size, size_t minBlockSize, IAllocator* backingAllocator)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
    if (size == 0 || !m_backingAllocator) {
        return;
    }

    m_minBlockSize = (std::max)(minBlockSize, sizeof(FreeBlock));
    if (!IsPowerOfTwo(m_minBlockSize)) {
        m_minBlockSize = NextPowerOfTwo(m_minBlockSize);
    }

    m_totalSize = size < m_minBlockSize ? m_minBlockSize : NextPowerOfTwo(size);
    const size_t alignment = (std::max)(m_minBlockSize, kMinAlignment);

    m_buffer = static_cast<byte*>(m_backingAllocator->Allocate(m_totalSize, alignment));
    if (!m_buffer) {
        m_totalSize = 0;
        m_minBlockSize = 0;
        return;
    }

    size_t blockSize = m_minBlockSize;
    m_levels = 1;
    while (blockSize < m_totalSize) {
        blockSize <<= 1;
        ++m_levels;
    }

    m_freeLists.assign(m_levels, nullptr);
    PushFree(m_levels - 1, reinterpret_cast<FreeBlock*>(m_buffer));
}

BuddyAllocator::~BuddyAllocator()
{
    if (m_backingAllocator && m_buffer) {
        m_backingAllocator->Free(m_buffer);
    }
    m_buffer = nullptr;
    m_totalSize = 0;
    m_minBlockSize = 0;
    m_levels = 0;
    m_freeLists.clear();
    m_allocatedBytes = 0;
}

BuddyAllocator::BuddyAllocator(BuddyAllocator&& other) noexcept
    : m_buffer(other.m_buffer),
      m_totalSize(other.m_totalSize),
      m_minBlockSize(other.m_minBlockSize),
      m_levels(other.m_levels),
      m_backingAllocator(other.m_backingAllocator),
      m_freeLists(std::move(other.m_freeLists)),
      m_allocatedBytes(other.m_allocatedBytes)
{
    other.m_buffer = nullptr;
    other.m_totalSize = 0;
    other.m_minBlockSize = 0;
    other.m_levels = 0;
    other.m_allocatedBytes = 0;
    other.m_freeLists.clear();
}

BuddyAllocator& BuddyAllocator::operator=(BuddyAllocator&& other) noexcept
{
    if (this != &other) {
        if (m_backingAllocator && m_buffer) {
            m_backingAllocator->Free(m_buffer);
        }

        m_buffer = other.m_buffer;
        m_totalSize = other.m_totalSize;
        m_minBlockSize = other.m_minBlockSize;
        m_levels = other.m_levels;
        m_backingAllocator = other.m_backingAllocator;
        m_freeLists = std::move(other.m_freeLists);
        m_allocatedBytes = other.m_allocatedBytes;

        other.m_buffer = nullptr;
        other.m_totalSize = 0;
        other.m_minBlockSize = 0;
        other.m_levels = 0;
        other.m_allocatedBytes = 0;
        other.m_freeLists.clear();
    }
    return *this;
}

void* BuddyAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_buffer || size == 0) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "BuddyAllocator: invalid allocation request");
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        alignment = kMinAlignment;
    }
    if (alignment < kMinAlignment) {
        alignment = kMinAlignment;
    }

    const size_t headerSize = sizeof(Header);
    if (size > m_totalSize) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "BuddyAllocator: requested size exceeds arena");
        return nullptr;
    }
    const size_t maxExtra = headerSize + alignment;
    if (size > (std::numeric_limits<size_t>::max)() - maxExtra) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "BuddyAllocator: allocation size overflow");
        return nullptr;
    }
    const size_t requiredSize = size + maxExtra;
    if (requiredSize > m_totalSize) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "BuddyAllocator: required size exceeds arena");
        return nullptr;
    }
    size_t order = OrderForSize(requiredSize);
    
    Threading::SpinLockGuard guard(m_lock);

    if (order >= m_levels) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "BuddyAllocator: allocation order unavailable");
        return nullptr;
    }

    size_t currentOrder = order;
    while (currentOrder < m_levels && m_freeLists[currentOrder] == nullptr) {
        ++currentOrder;
    }
    if (currentOrder >= m_levels) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "BuddyAllocator: no suitable free block");
        return nullptr;
    }

    FreeBlock* block = PopFree(currentOrder);
    while (currentOrder > order) {
        --currentOrder;
        size_t blockSize = SizeForOrder(currentOrder);
        auto* blockBytes = reinterpret_cast<byte*>(block);
        auto* buddy = reinterpret_cast<FreeBlock*>(blockBytes + blockSize);
        PushFree(currentOrder, buddy);
    }

    auto* blockBytes = reinterpret_cast<byte*>(block);
    size_t adjustment = AlignmentAdjustmentWithHeader(blockBytes, alignment, headerSize);
    byte* aligned = blockBytes + adjustment;
    auto* header = reinterpret_cast<Header*>(aligned - headerSize);
    header->order = static_cast<uint32>(order);
    header->adjustment = static_cast<uint32>(adjustment);
    header->size = size;
    header->tag = GetCurrentMemoryTag();
    header->padding = kBuddyMagic;

    m_allocatedBytes += size;
    m_stats.RecordAllocation(size);

    if (MemoryProfiler* profiler = MemoryManager::Profiler()) {
        profiler->TrackAlloc(aligned, size, header->tag);
    }

    return aligned;
}

void* BuddyAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!Owns(ptr)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "BuddyAllocator: reallocate pointer is not live");
        return nullptr;
    }

    if (!IsPowerOfTwo(alignment)) {
        alignment = kMinAlignment;
    }
    if (alignment < kMinAlignment) {
        alignment = kMinAlignment;
    }

    auto* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<Header*>(aligned - sizeof(Header));
    
    // Check if new size fits in current order
    const size_t headerSize = sizeof(Header);
    const size_t requiredSize = newSize + headerSize + alignment;
    size_t newOrder = OrderForSize(requiredSize);

    if (newOrder == header->order) {
        // Fits in the same power-of-two block.
        // Update size for statistics and return.
        {
            Threading::SpinLockGuard guard(m_lock);
            m_allocatedBytes -= header->size;
            m_allocatedBytes += newSize;
        }
        m_stats.RecordResize(header->size, newSize);
        header->size = newSize;
        return ptr;
    }

    // Fallback: Allocate new, copy, free old.
    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        size_t oldSize = header->size;
        size_t copySize = (oldSize < newSize) ? oldSize : newSize;
        MemCopy(newPtr, ptr, copySize);
        Free(ptr);
    } else {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "BuddyAllocator: reallocate allocation failed");
    }
    return newPtr;
}

void BuddyAllocator::Free(void* ptr)
{
    if (!ptr) {
        return;
    }

    Threading::SpinLockGuard guard(m_lock);
    if (!IsLiveAllocation(ptr)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::DoubleFree, "BuddyAllocator: invalid or double free");
        return;
    }

    auto* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<Header*>(aligned - sizeof(Header));
    size_t order = header->order;
    size_t blockSize = SizeForOrder(order);
    byte* block = aligned - header->adjustment;
    size_t offset = static_cast<size_t>(block - m_buffer);

    if (m_allocatedBytes >= header->size) {
        m_allocatedBytes -= header->size;
    }
    m_stats.RecordFree(header->size);

    if (MemoryProfiler* profiler = MemoryManager::Profiler()) {
        profiler->TrackFree(ptr, header->size, header->tag);
    }
    header->padding = 0;

    while (order + 1 < m_levels) {
        size_t buddyOffset = offset ^ blockSize;
        auto* buddy = reinterpret_cast<FreeBlock*>(m_buffer + buddyOffset);
        if (!RemoveFree(order, buddy)) {
            break;
        }

        if (buddyOffset < offset) {
            offset = buddyOffset;
        }
        blockSize <<= 1;
        ++order;
    }

    PushFree(order, reinterpret_cast<FreeBlock*>(m_buffer + offset));
}

size_t BuddyAllocator::AllocatedSize() const
{
    Threading::SpinLockGuard guard(m_lock);
    return m_allocatedBytes;
}

const char* BuddyAllocator::Name() const
{
    return "BuddyAllocator";
}

bool BuddyAllocator::Owns(void* ptr) const
{
    Threading::SpinLockGuard guard(m_lock);
    return IsLiveAllocation(ptr);
}

AllocatorStats BuddyAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    Threading::SpinLockGuard guard(m_lock);
    stats.liveBytes = m_allocatedBytes;
    stats.reservedBytes = m_totalSize;
    stats.committedBytes = m_totalSize;
    for (size_t order = 0; order < m_levels; ++order) {
        const size_t blockSize = SizeForOrder(order);
        for (FreeBlock* block = m_freeLists[order]; block; block = block->next) {
            stats.freeBytes += blockSize;
            if (blockSize > stats.largestFreeBlockBytes) {
                stats.largestFreeBlockBytes = blockSize;
            }
        }
    }
    stats.fragmentationBytes = stats.freeBytes > stats.largestFreeBlockBytes
        ? stats.freeBytes - stats.largestFreeBlockBytes
        : 0;
    return stats;
}

size_t BuddyAllocator::SizeForOrder(size_t order) const noexcept
{
    return m_minBlockSize << order;
}

size_t BuddyAllocator::OrderForSize(size_t blockSize) const noexcept
{
    size_t order = 0;
    size_t size = m_minBlockSize;
    while (size < blockSize && order + 1 < m_levels) {
        size <<= 1;
        ++order;
    }
    return order;
}

BuddyAllocator::FreeBlock* BuddyAllocator::PopFree(size_t order)
{
    FreeBlock* block = m_freeLists[order];
    if (block) {
        m_freeLists[order] = block->next;
        block->next = nullptr;
    }
    return block;
}

void BuddyAllocator::PushFree(size_t order, FreeBlock* block)
{
    if (!block) {
        return;
    }
    block->next = m_freeLists[order];
    m_freeLists[order] = block;
}

bool BuddyAllocator::RemoveFree(size_t order, FreeBlock* block)
{
    FreeBlock* prev = nullptr;
    FreeBlock* current = m_freeLists[order];
    while (current) {
        if (current == block) {
            if (prev) {
                prev->next = current->next;
            } else {
                m_freeLists[order] = current->next;
            }
            current->next = nullptr;
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

bool BuddyAllocator::IsLiveAllocation(void* ptr) const noexcept
{
    if (!m_buffer || !ptr) {
        return false;
    }

    const auto* start = m_buffer;
    const auto* end = m_buffer + m_totalSize;
    const auto* p = static_cast<const byte*>(ptr);
    if (p < start + sizeof(Header) || p >= end) {
        return false;
    }

    const auto* header = reinterpret_cast<const Header*>(p - sizeof(Header));
    if (header->padding != kBuddyMagic || header->order >= m_levels) {
        return false;
    }

    const size_t blockSize = SizeForOrder(header->order);
    if (header->adjustment < sizeof(Header) || header->adjustment >= blockSize || header->size > blockSize) {
        return false;
    }

    const auto* block = p - header->adjustment;
    if (block < start || block >= end || block + blockSize > end) {
        return false;
    }

    const size_t blockOffset = static_cast<size_t>(block - start);
    return (blockOffset % blockSize) == 0;
}

} // namespace Engine::Memory
