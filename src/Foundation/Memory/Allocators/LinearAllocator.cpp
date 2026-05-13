#include <Foundation/Memory/Allocators/LinearAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Utility/Move.hpp>

namespace Engine::Memory {

namespace {
constexpr size_t kMinAlignment = alignof(MaxAlignT);
}

LinearAllocator::LinearAllocator(size_t size, IAllocator* backingAllocator)
    : m_capacity(size)
    , m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
    , m_ownsBuffer(true)
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

LinearAllocator::LinearAllocator(void* buffer, size_t size)
    : m_buffer(buffer)
    , m_capacity(size)
    , m_ownsBuffer(false)
{
}

LinearAllocator::~LinearAllocator()
{
    if (m_ownsBuffer && m_backingAllocator && m_buffer) {
        m_backingAllocator->Free(m_buffer);
    }

    m_buffer = nullptr;
    m_capacity = 0;
    m_offset = 0;
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_capacity(other.m_capacity)
    , m_offset(other.m_offset)
    , m_backingAllocator(other.m_backingAllocator)
    , m_ownsBuffer(other.m_ownsBuffer)
{
    other.m_buffer = nullptr;
    other.m_capacity = 0;
    other.m_offset = 0;
    other.m_ownsBuffer = false;
}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
{
    if (this != &other) {
        this->~LinearAllocator();

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

void* LinearAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_buffer || size == 0) {
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
    const size_t adjustment = AlignmentAdjustment(current, alignment);
    if (m_offset + adjustment + size > m_capacity) {
        return nullptr;
    }

    const size_t alignedOffset = m_offset + adjustment;
    m_offset = alignedOffset + size;
    return base + alignedOffset;
}

void* LinearAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }
    return nullptr;
}

void LinearAllocator::Free(void*)
{
}

size_t LinearAllocator::AllocatedSize() const
{
    return m_offset;
}

const char* LinearAllocator::Name() const
{
    return "LinearAllocator";
}

bool LinearAllocator::Owns(void* ptr) const
{
    if (!m_buffer || !ptr) {
        return false;
    }
    const auto* start = static_cast<const byte*>(m_buffer);
    const auto* end = start + m_offset;
    const auto* p = static_cast<const byte*>(ptr);
    return p >= start && p < end;
}

void LinearAllocator::Reset()
{
    m_offset = 0;
}

size_t LinearAllocator::GetMarker() const
{
    return m_offset;
}

void LinearAllocator::RewindToMarker(size_t marker)
{
    if (marker <= m_offset) {
        m_offset = marker;
    }
}

size_t LinearAllocator::GetCapacity() const
{
    return m_capacity;
}

size_t LinearAllocator::GetFreeSpace() const
{
    return m_capacity > m_offset ? m_capacity - m_offset : 0;
}

LinearAllocator* LinearAllocator::CreateChildArena(size_t size)
{
    LinearAllocator* child = New<LinearAllocator>(GetDefaultAllocator(), size, this);
    if (!child) {
        return nullptr;
    }
    AttachChild(UniquePtr<IAllocator>(child));
    return child;
}

void LinearAllocator::AttachChild(UniquePtr<IAllocator> child)
{
    if (child) {
        m_children.push_back(Move(child));
    }
}

} // namespace Engine::Memory
