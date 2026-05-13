#ifndef FOUNDATION_MEMORY_LINEAR_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_LINEAR_ALLOCATOR_HDR

#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/UniquePtr.hpp>
#include <Foundation/Memory/Allocator.hpp>

namespace Engine::Memory {

/// @brief Fast linear allocator (bump Pointer)
/// @details Allocations are O(1). Individual frees are not supported.
///          Call Reset() to free all allocations at once.
class LinearAllocator : public IAllocator {
public:
    /// @brief Create allocator with specified buffer size
    /// @param size Size in bytes
    /// @param backingAllocator Allocator for the buffer (default: system)
    explicit LinearAllocator(size_t size, IAllocator* backingAllocator = nullptr);

    /// @brief Create allocator using external buffer
    /// @param buffer Pre-allocated buffer
    /// @param size Buffer size in bytes
    LinearAllocator(void* buffer, size_t size);

    ~LinearAllocator() override;

    // Non-copyable
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    // Movable
    LinearAllocator(LinearAllocator&& other) noexcept;
    LinearAllocator& operator=(LinearAllocator&& other) noexcept;

    // ========================================================================
    // IAllocator Interface
    // ========================================================================

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override; // No-op for linear allocator
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;

    // ========================================================================
    // Linear Allocator Specific
    // ========================================================================

    /// @brief Reset allocator, freeing all allocations
    void Reset();

    /// @brief Get marker for current position
    [[nodiscard]] size_t GetMarker() const;

    /// @brief Rewind to a previous marker
    /// @param marker Marker from GetMarker()
    void RewindToMarker(size_t marker);

    /// @brief Get total capacity
    [[nodiscard]] size_t GetCapacity() const;

    /// @brief Get remaining free space
    [[nodiscard]] size_t GetFreeSpace() const;

    // ========================================================================
    // Hierarchical Arena Support
    // ========================================================================

    /// @brief Create a child arena that uses this allocator for its backing storage
    /// @param size Size of the child arena in bytes
    /// @return A new linear allocator managed by this one
    [[nodiscard]] LinearAllocator* CreateChildArena(size_t size);

    /// @brief Attach a child allocator to this one's lifetime
    void AttachChild(UniquePtr<IAllocator> child);

private:
    void* m_buffer = nullptr;
    size_t m_capacity = 0;
    size_t m_offset = 0;
    IAllocator* m_backingAllocator = nullptr;
    bool m_ownsBuffer = false;
    
    Vector<UniquePtr<IAllocator>> m_children;
};

/// @brief RAII scope for linear allocator
/// @details Automatically rewinds to marker on destruction
class LinearAllocatorScope {
public:
    explicit LinearAllocatorScope(LinearAllocator& allocator)
        : m_allocator(allocator), m_marker(allocator.GetMarker()) {}

    ~LinearAllocatorScope() {
        m_allocator.RewindToMarker(m_marker);
    }

    // Non-copyable, non-movable
    LinearAllocatorScope(const LinearAllocatorScope&) = delete;
    LinearAllocatorScope& operator=(const LinearAllocatorScope&) = delete;

private:
    LinearAllocator& m_allocator;
    size_t m_marker;
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_LINEAR_ALLOCATOR_HDR
