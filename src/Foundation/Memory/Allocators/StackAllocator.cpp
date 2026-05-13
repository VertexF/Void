#include <Foundation/Memory/Allocators/StackAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>

#include <limits>

namespace Engine::Memory {

namespace {
constexpr size_t kMinAlignment = alignof(MaxAlignT);
constexpr uint32 kStackMagic = 0x5354414Bu; // STAK

struct StackHeader {
    size_t previousOffset = 0;
    size_t adjustment = 0;
    size_t size = 0;
    uint32 magic = 0;
};
} // namespace

StackAllocator::StackAllocator(size_t size, IAllocator* backingAllocator)
    : m_capacity(size),
      m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator()),
      m_ownsBuffer(true)
{
    if (m_capacity == 0 || !m_backingAllocator) {
        m_ownsBuffer = false;
        return;
    }

    m_buffer = m_backingAllocator->Allocate(m_capacity, kMinAlignment);
    if (!m_buffer) {
        m_capacity = 0;
        m_ownsBuffer = false;
    }
}

StackAllocator::StackAllocator(void* buffer, size_t size)
    : m_buffer(buffer),
      m_capacity(size),
      m_ownsBuffer(false)
{
}

StackAllocator::~StackAllocator()
{
    if (m_ownsBuffer && m_backingAllocator && m_buffer) {
        m_backingAllocator->Free(m_buffer);
    }

    m_buffer = nullptr;
    m_capacity = 0;
    m_offset = 0;
}

StackAllocator::StackAllocator(StackAllocator&& other) noexcept
    : m_buffer(other.m_buffer),
      m_capacity(other.m_capacity),
      m_offset(other.m_offset),
      m_backingAllocator(other.m_backingAllocator),
      m_ownsBuffer(other.m_ownsBuffer)
{
    other.m_buffer = nullptr;
    other.m_capacity = 0;
    other.m_offset = 0;
    other.m_ownsBuffer = false;
}

StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept
{
    if (this != &other) {
        this->~StackAllocator();

        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        m_offset = other.m_offset;
        m_backingAllocator = other.m_backingAllocator;
        m_ownsBuffer = other.m_ownsBuffer;

        other.m_buffer = nullptr;
        other.m_capacity = 0;
        other.m_offset = 0;
        other.m_ownsBuffer = false;
    }
    return *this;
}

void* StackAllocator::Allocate(size_t size, size_t alignment)
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

    auto* base = static_cast<byte*>(m_buffer);
    byte* current = base + m_offset;
    size_t prevOffset = m_offset;
    size_t adjustment = AlignmentAdjustmentWithHeader(current, alignment, sizeof(StackHeader));
    if (m_offset > (std::numeric_limits<size_t>::max)() - adjustment ||
        size > (std::numeric_limits<size_t>::max)() - m_offset - adjustment ||
        m_offset + adjustment + size > m_capacity) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    size_t alignedOffset = m_offset + adjustment;
    auto* header = reinterpret_cast<StackHeader*>(base + alignedOffset - sizeof(StackHeader));
    header->previousOffset = prevOffset;
    header->adjustment = adjustment;
    header->size = size;
    header->magic = kStackMagic;

    m_offset = alignedOffset + size;
    m_stats.RecordAllocation(m_offset - prevOffset);
    return base + alignedOffset;
}

void* StackAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
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

    auto* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<StackHeader*>(aligned - sizeof(StackHeader));
    
    size_t alignedOffset = static_cast<size_t>(aligned - static_cast<byte*>(m_buffer));

    // True top-of-stack check:
    if (alignedOffset + header->size == m_offset) {
        if (alignedOffset + newSize <= m_capacity) {
            const size_t oldOffset = m_offset;
            header->size = newSize;
            m_offset = alignedOffset + newSize;
            m_stats.RecordResize(oldOffset, m_offset);
            return ptr;
        }
    }

    m_stats.RecordFailedAllocation();
    return nullptr;
}

void StackAllocator::Free(void* ptr)
{
    if (!ptr || !IsLiveAllocation(ptr)) {
        if (ptr) {
            m_stats.RecordFailedAllocation();
        }
        return;
    }

    auto* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<StackHeader*>(aligned - sizeof(StackHeader));
    const size_t alignedOffset = static_cast<size_t>(aligned - static_cast<byte*>(m_buffer));
    if (alignedOffset + header->size != m_offset) {
        m_stats.RecordFailedAllocation();
        return;
    }
    const size_t oldOffset = m_offset;
    m_offset = header->previousOffset;
    header->magic = 0;
    m_stats.RecordFree(oldOffset - m_offset);
}

size_t StackAllocator::AllocatedSize() const
{
    return m_offset;
}

const char* StackAllocator::Name() const
{
    return "StackAllocator";
}

bool StackAllocator::Owns(void* ptr) const
{
    return IsLiveAllocation(ptr);
}

AllocatorStats StackAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    stats.liveBytes = m_offset;
    if (stats.peakBytes < stats.liveBytes) {
        stats.peakBytes = stats.liveBytes;
    }
    return stats;
}

AllocatorStats StackAllocator::GetDetailedStats() const
{
    AllocatorStats stats = GetStats();
    stats.reservedBytes = m_capacity;
    stats.committedBytes = m_capacity;
    stats.freeBytes = m_capacity >= m_offset ? m_capacity - m_offset : 0;
    stats.largestFreeBlockBytes = stats.freeBytes;
    return stats;
}

bool StackAllocator::IsLiveAllocation(void* ptr) const noexcept
{
    if (!m_buffer || !ptr) {
        return false;
    }
    const auto* start = static_cast<const byte*>(m_buffer);
    const auto* end = start + m_offset;
    const auto* p = static_cast<const byte*>(ptr);
    if (p < start + sizeof(StackHeader) || p >= end) {
        return false;
    }

    const auto* header = reinterpret_cast<const StackHeader*>(p - sizeof(StackHeader));
    if (header->magic != kStackMagic || header->adjustment < sizeof(StackHeader)) {
        return false;
    }

    const auto* raw = p - header->adjustment;
    if (raw < start || raw >= end || p + header->size > end) {
        return false;
    }

    const size_t rawOffset = static_cast<size_t>(raw - start);
    const size_t currentOffset = static_cast<size_t>(p - start);
    return currentOffset >= rawOffset && header->previousOffset <= rawOffset;
}

void StackAllocator::Reset()
{
    m_offset = 0;
    m_stats.ResetLive();
}

size_t StackAllocator::GetCapacity() const noexcept
{
    return m_capacity;
}

size_t StackAllocator::GetMarker() const noexcept
{
    return m_offset;
}

void StackAllocator::RewindToMarker(size_t marker)
{
    if (marker <= m_offset) {
        m_offset = marker;
        if (marker == 0) {
            m_stats.ResetLive();
        }
    }
}

} // namespace Engine::Memory
