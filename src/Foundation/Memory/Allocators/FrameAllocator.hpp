#ifndef FOUNDATION_MEMORY_FRAME_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_FRAME_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>

// Win32 defines GetFreeSpace as a macro - undef to avoid name collision
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif

namespace Engine::Memory {

/// @brief Per-frame allocator that frees everything on BeginFrame().
class FrameAllocator : public IAllocator {
public:
    explicit FrameAllocator(size_t size, IAllocator* backingAllocator = nullptr);
    FrameAllocator(void* buffer, size_t size);
    ~FrameAllocator() override;

    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;

    FrameAllocator(FrameAllocator&& other) noexcept;
    FrameAllocator& operator=(FrameAllocator&& other) noexcept;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;
    [[nodiscard]] AllocatorStats GetDetailedStats() const override;

    /// @brief Starts a new frame (frees all allocations from previous frame).
    void BeginFrame();

    /// @brief Optional end-of-frame hook (currently a no-op).
    void EndFrame();

    [[nodiscard]] size_t GetCapacity() const noexcept;
    [[nodiscard]] size_t GetFreeSpace() const noexcept;

    [[nodiscard]] size_t GetMarker() const noexcept;
    void RewindToMarker(size_t marker);

private:
    void* m_buffer = nullptr;
    size_t m_capacity = 0;
    size_t m_offset = 0;
    IAllocator* m_backingAllocator = nullptr;
    bool m_ownsBuffer = false;
    AllocatorStatsTracker m_stats;
};

/// @brief RAII scope that rewinds the frame allocator to the marker on destruction.
class FrameAllocatorScope final {
public:
    explicit FrameAllocatorScope(FrameAllocator& allocator) noexcept
        : m_allocator(allocator), m_marker(allocator.GetMarker()) {}

    ~FrameAllocatorScope();

    FrameAllocatorScope(const FrameAllocatorScope&) = delete;
    FrameAllocatorScope& operator=(const FrameAllocatorScope&) = delete;

private:
    FrameAllocator& m_allocator;
    size_t m_marker = 0;
};

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_FRAME_ALLOCATOR_HDR
