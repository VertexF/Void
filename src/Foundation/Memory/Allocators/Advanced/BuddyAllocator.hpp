#ifndef FOUNDATION_MEMORY_BUDDY_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_BUDDY_ALLOCATOR_HDR

// Buddy Allocator (power-of-two block allocator)
// ============================================================================

#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>


namespace Engine::Memory {

/// @brief Buddy allocator using power-of-two blocks.
class BuddyAllocator : public IAllocator {
public:
    BuddyAllocator(size_t size, size_t minBlockSize = 256, IAllocator* backingAllocator = nullptr);
    ~BuddyAllocator() override;

    BuddyAllocator(const BuddyAllocator&) = delete;
    BuddyAllocator& operator=(const BuddyAllocator&) = delete;

    BuddyAllocator(BuddyAllocator&& other) noexcept;
    BuddyAllocator& operator=(BuddyAllocator&& other) noexcept;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;
    [[nodiscard]] AllocatorStats GetDetailedStats() const override;

    [[nodiscard]] size_t GetTotalSize() const noexcept { return m_totalSize; }
    [[nodiscard]] size_t GetMinBlockSize() const noexcept { return m_minBlockSize; }

private:
    struct FreeBlock {
        FreeBlock* next = nullptr;
    };

    struct Header {
        uint32 order = 0;
        uint32 adjustment = 0;
        size_t size = 0;
        MemoryTag tag = MemoryTag::Default;
        uint16 padding = 0;
    };

    size_t SizeForOrder(size_t order) const noexcept;
    size_t OrderForSize(size_t blockSize) const noexcept;
    FreeBlock* PopFree(size_t order);
    void PushFree(size_t order, FreeBlock* block);
    bool RemoveFree(size_t order, FreeBlock* block);
    [[nodiscard]] bool IsLiveAllocation(void* ptr) const noexcept;

    byte* m_buffer = nullptr;
    size_t m_totalSize = 0;
    size_t m_minBlockSize = 0;
    size_t m_levels = 0;
    IAllocator* m_backingAllocator = nullptr;
    Vector<FreeBlock*> m_freeLists{};
    size_t m_allocatedBytes = 0;
    AllocatorStatsTracker m_stats;
    mutable Threading::SpinLock m_lock;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_BUDDY_ALLOCATOR_HDR
