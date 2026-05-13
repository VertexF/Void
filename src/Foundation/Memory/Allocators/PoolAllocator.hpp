#ifndef FOUNDATION_MEMORY_POOL_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_POOL_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>
#include <Utility/Move.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

/// @brief Fixed-size block pool allocator
/// @details All allocations are the same size. O(1) allocate and free.
///          Uses a free list for efficient reuse.
class PoolAllocator : public IAllocator {
public:
    /// @brief Create pool allocator
    /// @param blockSize Size of each block in bytes
    /// @param blockCount Number of blocks to allocate
    /// @param backingAllocator Allocator for the buffer (default: system)
    /// @param blockAlignment Alignment for each block (default: max_align_t)
    PoolAllocator(size_t blockSize, size_t blockCount, IAllocator* backingAllocator = nullptr,
                  size_t blockAlignment = alignof(MaxAlignT));
    
    ~PoolAllocator() override;
    
    // Non-copyable
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    
    // Movable
    PoolAllocator(PoolAllocator&& other) noexcept;
    PoolAllocator& operator=(PoolAllocator&& other) noexcept;
    
    // ========================================================================
    // IAllocator Interface
    // ========================================================================
    
    /// @brief Allocate a block
    /// @param size Must be <= block size
    /// @param alignment Must be <= block alignment
    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;

    /// @brief Reallocate a block
    /// @details For PoolAllocator, this is only successful if newSize <= blockSize
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    
    /// @brief Free a block back to the pool
    void Free(void* ptr) override;
    
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;
    
    // ========================================================================
    // Pool Allocator Specific
    // ========================================================================
    
    /// @brief Get block size
    [[nodiscard]] size_t GetBlockSize() const;
    
    /// @brief Get total number of blocks
    [[nodiscard]] size_t GetBlockCount() const;
    
    /// @brief Get number of free blocks
    [[nodiscard]] size_t GetFreeBlockCount() const;
    
    /// @brief Get number of allocated blocks
    [[nodiscard]] size_t GetAllocatedBlockCount() const;
    
    /// @brief Check if pool is full
    [[nodiscard]] bool IsFull() const;
    
    /// @brief Reset pool, freeing all allocations
    void Reset();
    
private:
    /// Free list node (stored in free blocks)
    struct FreeBlock {
        FreeBlock* next;
    };
    
    void InitializeFreeList();
    [[nodiscard]] bool IsAllocatedIndex(size_t index) const noexcept;
    void SetAllocatedIndex(size_t index, bool value) noexcept;
    
    void* m_buffer = nullptr;
    size_t m_blockSize = 0;
    size_t m_blockCount = 0;
    size_t m_blockAlignment = alignof(MaxAlignT);
    Atomic<size_t> m_allocatedCount{0};
    Atomic<FreeBlock*> m_freeList{nullptr};
    IAllocator* m_backingAllocator = nullptr;
    uint8* m_allocatedBitmap = nullptr;
    size_t m_allocatedBitmapBytes = 0;
    MemoryTag* m_blockTags = nullptr;
    AllocatorStatsTracker m_stats;
    mutable Threading::SpinLock m_lock;
};

/// @brief Typed pool allocator for specific type
template<typename T>
class TypedPoolAllocator {
public:
    explicit TypedPoolAllocator(size_t count, IAllocator* backing = nullptr)
        : m_pool(sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T), count, backing, alignof(T))
    {}
    
    template<typename... Args>
    [[nodiscard]] T* Allocate(Args&&... args) {
        void* ptr = m_pool.Allocate(sizeof(T), alignof(T));
        if (!ptr) return nullptr;
        return new (ptr) T(Forward<Args>(args)...);
    }
    
    void Free(T* ptr) {
        if (ptr) {
            ptr->~T();
            m_pool.Free(ptr);
        }
    }
    
    [[nodiscard]] size_t GetFreeCount() const { return m_pool.GetFreeBlockCount(); }
    [[nodiscard]] bool IsFull() const { return m_pool.IsFull(); }
    void Reset() { m_pool.Reset(); }
    
private:
    PoolAllocator m_pool;
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_POOL_ALLOCATOR_HDR
