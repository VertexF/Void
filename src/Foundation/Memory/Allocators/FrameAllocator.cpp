#include <Foundation/Memory/Allocators/FrameAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>

#include <limits>

namespace Engine::Memory {

namespace {
constexpr size_t kMinAlignment = alignof(MaxAlignT);
}

FrameAllocator::FrameAllocator(size_t size, IAllocator* backingAllocator)
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

FrameAllocator::FrameAllocator(void* buffer, size_t size)
    : m_buffer(buffer),
      m_capacity(size),
      m_ownsBuffer(false)
{
}

FrameAllocator::~FrameAllocator()
{
    if (m_ownsBuffer && m_backingAllocator && m_buffer) {
        m_backingAllocator->Free(m_buffer);
    }

    m_buffer = nullptr;
    m_capacity = 0;
    m_offset = 0;
}

FrameAllocator::FrameAllocator(FrameAllocator&& other) noexcept
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

FrameAllocator& FrameAllocator::operator=(FrameAllocator&& other) noexcept
{
    if (this != &other) {
        this->~FrameAllocator();

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

void* FrameAllocator::Allocate(size_t size, size_t alignment)
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
    size_t adjustment = AlignmentAdjustment(current, alignment);
    if (m_offset > (std::numeric_limits<size_t>::max)() - adjustment ||
        size > (std::numeric_limits<size_t>::max)() - m_offset - adjustment ||
        m_offset + adjustment + size > m_capacity) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    const size_t previousOffset = m_offset;
    size_t alignedOffset = m_offset + adjustment;
    m_offset = alignedOffset + size;
    m_stats.RecordAllocation(m_offset - previousOffset);
    return base + alignedOffset;
}

void* FrameAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!Owns(ptr)) {
        m_stats.RecordFailedAllocation();
        return nullptr;
    }

    m_stats.RecordFailedAllocation();
    return nullptr;
}

void FrameAllocator::Free(void* /*ptr*/)
{
    // No-op; use BeginFrame() or RewindToMarker() instead.
}

size_t FrameAllocator::AllocatedSize() const
{
    return m_offset;
}

const char* FrameAllocator::Name() const
{
    return "FrameAllocator";
}

bool FrameAllocator::Owns(void* ptr) const
{
    if (!m_buffer || !ptr) {
        return false;
    }
    const auto* start = static_cast<const byte*>(m_buffer);
    const auto* end = start + m_offset;
    const auto* p = static_cast<const byte*>(ptr);
    return p >= start && p < end;
}

AllocatorStats FrameAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    stats.liveBytes = m_offset;
    if (stats.peakBytes < stats.liveBytes) {
        stats.peakBytes = stats.liveBytes;
    }
    return stats;
}

AllocatorStats FrameAllocator::GetDetailedStats() const
{
    AllocatorStats stats = GetStats();
    stats.reservedBytes = m_capacity;
    stats.committedBytes = m_capacity;
    stats.freeBytes = GetFreeSpace();
    stats.largestFreeBlockBytes = stats.freeBytes;
    return stats;
}

void FrameAllocator::BeginFrame()
{
    m_offset = 0;
    m_stats.ResetLive();
}

void FrameAllocator::EndFrame()
{
}

size_t FrameAllocator::GetCapacity() const noexcept
{
    return m_capacity;
}

size_t FrameAllocator::GetFreeSpace() const noexcept
{
    return m_capacity > m_offset ? (m_capacity - m_offset) : 0;
}

size_t FrameAllocator::GetMarker() const noexcept
{
    return m_offset;
}

void FrameAllocator::RewindToMarker(size_t marker)
{
    if (marker <= m_offset) {
        m_offset = marker;
        if (marker == 0) {
            m_stats.ResetLive();
        }
    }
}

FrameAllocatorScope::~FrameAllocatorScope()
{
    m_allocator.RewindToMarker(m_marker);
}

} // namespace Engine::Memory
