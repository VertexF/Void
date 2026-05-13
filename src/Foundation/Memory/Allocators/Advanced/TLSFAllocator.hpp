#ifndef FOUNDATION_MEMORY_TLSFALLOCATOR_HDR
#define FOUNDATION_MEMORY_TLSFALLOCATOR_HDR

#include <cstddef>
#include <cstdint>

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>


namespace Engine::Memory {
    
/// @brief Two-Level Segregated Fit (TLSF) Allocator
/// @details Constant time O(1) complexity for both allocate and free.
///          Low fragmentation, good for general purpose heap allocations.
class TLSFAllocator : public IAllocator {
public:
    /// @brief Create TLSF allocator
    /// @param size Total size of the memory pool
    /// @param backingAllocator Allocator for the initial pool (default: system)
    explicit TLSFAllocator(size_t size, IAllocator* backingAllocator = nullptr);
    
    ~TLSFAllocator() override;

    // Non-copyable
    TLSFAllocator(const TLSFAllocator&) = delete;
    TLSFAllocator& operator=(const TLSFAllocator&) = delete;

    TLSFAllocator(TLSFAllocator&& other) noexcept = delete;
    TLSFAllocator& operator=(TLSFAllocator&& other) noexcept = delete;

    // ========================================================================
    // IAllocator Interface
    // ========================================================================
    
    void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    
    size_t AllocatedSize() const override;
    const char* Name() const override;
    bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;

    // ========================================================================
    // TLSF Specific
    // ========================================================================
    
    [[nodiscard]] size_t GetTotalSize() const noexcept { return m_poolSize; }

private:
    struct Block;

    /// Block header stored in memory
    struct Block {
        size_t size;           // Bit 0: free flag, Bit 1: prev free flag
        Block* prevPhysical;   // Link to physically adjacent previous block
        
        // Only valid if block is free
        Block* nextFree;
        Block* prevFree;

        static constexpr size_t kFreeBit = 1;
        static constexpr size_t kPrevFreeBit = 2;
        static constexpr size_t kSizeMask = ~(kFreeBit | kPrevFreeBit);

        size_t GetSize() const noexcept { return size & kSizeMask; }
        void SetSize(size_t s) noexcept { size = (size & ~kSizeMask) | (s & kSizeMask); }
        
        bool IsFree() const noexcept { return (size & kFreeBit) != 0; }
        void SetFree(bool f) noexcept { if (f) size |= kFreeBit; else size &= ~kFreeBit; }
        
        bool IsPrevFree() const noexcept { return (size & kPrevFreeBit) != 0; }
        void SetPrevFree(bool f) noexcept { if (f) size |= kPrevFreeBit; else size &= ~kPrevFreeBit; }
    };

    static constexpr size_t kSliLog2 = 5; // 32 divisions
    static constexpr size_t kSli = 1 << kSliLog2;
    static constexpr size_t kFliMax = 32; // up to 4GB pools
    
    void InsertBlock(Block* block);
    void RemoveBlock(Block* block);
    Block* FindSuitableBlock(size_t size, size_t& fl, size_t& sl);
    void Mapping(size_t size, size_t& fl, size_t& sl);
    [[nodiscard]] bool IsLiveBlock(void* ptr) const noexcept;

    void* m_poolBuffer = nullptr;
    size_t m_poolSize = 0;
    IAllocator* m_backingAllocator = nullptr;

    uint32_t m_flBitmap = 0;
    uint32_t m_slBitmaps[kFliMax] = {0};
    Block* m_blocks[kFliMax][kSli] = {nullptr};
    
    size_t m_allocatedBytes = 0;
    AllocatorStatsTracker m_stats;
    mutable Threading::SpinLock m_lock;
};

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_TLSFALLOCATOR_HDR
