#include <Foundation/Memory/TLSFAllocator.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Operations.hpp>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace Engine::Memory {
    namespace {
        /// @brief Find the index of the least significant bit set
        inline uint32_t FindFirstSet(uint32_t value) noexcept {
            if (value == 0) return 0;
            #ifdef _MSC_VER
                unsigned long index;
                _BitScanForward(&index, value);
                return static_cast<uint32_t>(index);
            #else
                return static_cast<uint32_t>(__builtin_ctz(value));
            #endif
        }

        /// @brief Find the index of the most significant bit set
        inline uint32_t FindLastSet(uint32_t value) noexcept {
            if (value == 0) return 0;
            #ifdef _MSC_VER
                unsigned long index;
                _BitScanReverse(&index, value);
                return static_cast<uint32_t>(index);
            #else
                return 31u - static_cast<uint32_t>(__builtin_clz(value));
            #endif
        }

        constexpr size_t kMinBlockSize = 16;
        constexpr size_t kMaxBlockSize = 1ULL << 32;
    } // namespace

    TLSFAllocator::TLSFAllocator(size_t size, IAllocator* backingAllocator) : m_poolSize(size), 
        m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator()) {
    
        if (m_poolSize < sizeof(Block) + kMinBlockSize || !m_backingAllocator) return;

        // Align pool size to min block size
        m_poolSize = AlignSize(m_poolSize, kMinBlockSize);

        // Allocate pool
        m_poolBuffer = m_backingAllocator->Allocate(m_poolSize, alignof(MaxAlignT));
        if (!m_poolBuffer) return;

        // Initialize bitmaps and blocks
        m_flBitmap = 0;
        MemSet(m_slBitmaps, 0, sizeof(m_slBitmaps));
        MemSet(m_blocks, 0, sizeof(m_blocks));

        // Create initial large block
        Block* initialBlock = static_cast<Block*>(m_poolBuffer);
        initialBlock->SetSize(m_poolSize - sizeof(Block));
        initialBlock->SetFree(true);
        initialBlock->SetPrevFree(false);
        initialBlock->prevPhysical = nullptr;
        initialBlock->nextFree = nullptr;
        initialBlock->prevFree = nullptr;

        InsertBlock(initialBlock);
    }

    TLSFAllocator::~TLSFAllocator()
    {
        if (m_backingAllocator && m_poolBuffer) {
            m_backingAllocator->Free(m_poolBuffer);
        }
    }

    void* TLSFAllocator::Allocate(size_t size, size_t alignment)
    {
        if (size == 0) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "TLSFAllocator: zero-size allocation");
            return nullptr;
        }
        if (!IsPowerOfTwo(alignment)) {
            alignment = alignof(MaxAlignT);
        }
        if (alignment < alignof(MaxAlignT)) {
            alignment = alignof(MaxAlignT);
        }
        if (alignment > kMinBlockSize) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::UnsupportedOperation, "TLSFAllocator: requested alignment exceeds block granularity");
            return nullptr;
        }

        if (size < kMinBlockSize) {
            size = kMinBlockSize;
        }
        size = AlignSize(size, kMinBlockSize);

        Threading::SpinLockGuard guard(m_lock);

        size_t fl, sl;
        Mapping(size, fl, sl);

        Block* block = FindSuitableBlock(size, fl, sl);
        if (!block) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "TLSFAllocator: no suitable free block");
            return nullptr;
        }

        RemoveBlock(block);

        // Split block if possible
        const size_t remaining = block->GetSize() - size;
        if (remaining >= sizeof(Block) + kMinBlockSize) {
            block->SetSize(size);
            
            Block* nextBlock = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(block) + sizeof(Block) + size);
            nextBlock->SetSize(remaining - sizeof(Block));
            nextBlock->SetFree(true);
            nextBlock->SetPrevFree(false);
            nextBlock->prevPhysical = block;
            
            InsertBlock(nextBlock);

            // Keep physical metadata coherent for the block following the split tail.
            Block* afterNext = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(nextBlock) + sizeof(Block) + nextBlock->GetSize());
            if (reinterpret_cast<uint8_t*>(afterNext) < reinterpret_cast<uint8_t*>(m_poolBuffer) + m_poolSize) {
                afterNext->prevPhysical = nextBlock;
                afterNext->SetPrevFree(true);
            }
        }

        block->SetFree(false);
        
        // Update next physical block's prev_free bit
        Block* nextPhysical = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(block) + sizeof(Block) + block->GetSize());
        if (reinterpret_cast<uint8_t*>(nextPhysical) < reinterpret_cast<uint8_t*>(m_poolBuffer) + m_poolSize) {
            nextPhysical->SetPrevFree(false);
            nextPhysical->prevPhysical = block;
        }

        m_allocatedBytes += block->GetSize();
        m_stats.RecordAllocation(block->GetSize());
        void* result = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(block) + sizeof(Block));

        return result;
    }

    void* TLSFAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
    {
        if (ptr == nullptr) {
            return Allocate(newSize, alignment);
        }
        if (newSize == 0) {
            Free(ptr);
            return nullptr;
        }

        if (!Owns(ptr)) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "TLSFAllocator: reallocate pointer is not live");
            return nullptr;
        }

        Block* block = reinterpret_cast<Block*>(static_cast<uint8_t*>(ptr) - sizeof(Block));
        size_t oldSize = block->GetSize();

        if (newSize <= oldSize) {
            // Shrinking or same size. 
            // Optimization: We could potentially split the block here,
            // but for now we just return the same block.
            return ptr;
        }

        // Growing. 
        // Optimization: Check if next physical block is free and can be coalesced?
        // For now, use simple fallback.
        void* newPtr = Allocate(newSize, alignment);
        if (newPtr) {
            MemCopy(newPtr, ptr, oldSize);
            Free(ptr);
        } else {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "TLSFAllocator: reallocate allocation failed");
        }
        return newPtr;
    }

    void TLSFAllocator::Free(void* ptr)
    {
        if (!ptr) return;

        Threading::SpinLockGuard guard(m_lock);
        if (!IsLiveBlock(ptr)) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::DoubleFree, "TLSFAllocator: invalid or double free");
            return;
        }

        Block* block = reinterpret_cast<Block*>(static_cast<uint8_t*>(ptr) - sizeof(Block));
        if (block->IsFree()) return;

        size_t blockSize = block->GetSize();
        block->SetFree(true);
        m_allocatedBytes -= blockSize;
        m_stats.RecordFree(blockSize);

        // Coalesce with next physical block
        Block* nextPhysical = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(block) + sizeof(Block) + block->GetSize());
        if (reinterpret_cast<uint8_t*>(nextPhysical) < reinterpret_cast<uint8_t*>(m_poolBuffer) + m_poolSize && nextPhysical->IsFree()) {
            RemoveBlock(nextPhysical);
            block->SetSize(block->GetSize() + nextPhysical->GetSize() + sizeof(Block));
        }

        // Coalesce with previous physical block
        if (block->IsPrevFree()) {
            Block* prevPhysical = block->prevPhysical;
            if (prevPhysical && prevPhysical->IsFree()) {
                RemoveBlock(prevPhysical);
                prevPhysical->SetSize(prevPhysical->GetSize() + block->GetSize() + sizeof(Block));
                block = prevPhysical;
            }
        }

        InsertBlock(block);
        
        // Update next physical block's prev_free bit
        nextPhysical = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(block) + sizeof(Block) + block->GetSize());
        if (reinterpret_cast<uint8_t*>(nextPhysical) < reinterpret_cast<uint8_t*>(m_poolBuffer) + m_poolSize) {
            nextPhysical->SetPrevFree(true);
            nextPhysical->prevPhysical = block;
        }
    }

    size_t TLSFAllocator::AllocatedSize() const
    {
        Threading::SpinLockGuard guard(m_lock);
        return m_allocatedBytes;
    }

    const char* TLSFAllocator::Name() const
    {
        return "TLSFAllocator";
    }

    bool TLSFAllocator::Owns(void* ptr) const
    {
        if (!m_poolBuffer || !ptr) return false;

        Threading::SpinLockGuard guard(m_lock);
        return IsLiveBlock(ptr);
    }

    AllocatorStats TLSFAllocator::GetStats() const
    {
        AllocatorStats stats = m_stats.Snapshot(Name());
        return stats;
    }

    AllocatorStats TLSFAllocator::GetDetailedStats() const
    {
        AllocatorStats stats = GetStats();
        Threading::SpinLockGuard guard(m_lock);
        stats.liveBytes = m_allocatedBytes;
        stats.reservedBytes = m_poolSize;
        stats.committedBytes = m_poolSize;
        for (size_t fl = 0; fl < kFliMax; ++fl) {
            for (size_t sl = 0; sl < kSli; ++sl) {
                for (Block* block = m_blocks[fl][sl]; block; block = block->nextFree) {
                    const size_t blockSize = block->GetSize();
                    stats.freeBytes += blockSize;
                    if (blockSize > stats.largestFreeBlockBytes) {
                        stats.largestFreeBlockBytes = blockSize;
                    }
                }
            }
        }
        stats.fragmentationBytes = stats.freeBytes > stats.largestFreeBlockBytes
            ? stats.freeBytes - stats.largestFreeBlockBytes
            : 0;
        return stats;
    }

    void TLSFAllocator::Mapping(size_t size, size_t& fl, size_t& sl)
    {
        // TLSF bitmaps are 32-bit/32-level; clamp unsupported large requests.
        const uint32_t size32 = (size > static_cast<size_t>(0xFFFFFFFFu))
            ? 0xFFFFFFFFu
            : static_cast<uint32_t>(size);

        fl = FindLastSet(size32);

        // Small-size linear class: keep sub-32-byte blocks out of the 32-byte
        // class bucket so FindSuitableBlock never returns a too-small block.
        if (size32 < static_cast<uint32_t>(kMinBlockSize)) {
            fl = 0;
            sl = 0;
            return;
        }

        if (size32 < (1u << kSliLog2)) {
            fl = 0;
            sl = static_cast<size_t>((size32 - static_cast<uint32_t>(kMinBlockSize)) / static_cast<uint32_t>(kMinBlockSize));
            if (sl >= kSli) {
                sl = kSli - 1;
            }
            return;
        }

        sl = (size32 >> (fl - kSliLog2)) - kSli;

        // Defensive guard for any edge case on class boundaries.
        if (sl >= kSli) {
            sl = kSli - 1;
        }

        // Defensive guard for clamped input values.
        if (fl >= kFliMax) {
            fl = kFliMax - 1;
        }
    }

    void TLSFAllocator::InsertBlock(Block* block)
    {
        size_t fl, sl;
        Mapping(block->GetSize(), fl, sl);

        block->nextFree = m_blocks[fl][sl];
        block->prevFree = nullptr;
        if (m_blocks[fl][sl]) {
            m_blocks[fl][sl]->prevFree = block;
        }
        m_blocks[fl][sl] = block;

        m_flBitmap |= (uint32_t{1} << fl);
        m_slBitmaps[fl] |= (uint32_t{1} << sl);
    }

    void TLSFAllocator::RemoveBlock(Block* block)
    {
        size_t fl, sl;
        Mapping(block->GetSize(), fl, sl);

        if (block->nextFree) block->nextFree->prevFree = block->prevFree;
        if (block->prevFree) block->prevFree->nextFree = block->nextFree;
        
        if (m_blocks[fl][sl] == block) {
            m_blocks[fl][sl] = block->nextFree;
            if (m_blocks[fl][sl] == nullptr) {
                m_slBitmaps[fl] &= ~(uint32_t{1} << sl);
                if (m_slBitmaps[fl] == 0) {
                    m_flBitmap &= ~(uint32_t{1} << fl);
                }
            }
        }
    }

    TLSFAllocator::Block* TLSFAllocator::FindSuitableBlock(size_t, size_t& fl, size_t& sl)
    {
        uint32_t slMap = m_slBitmaps[fl] & (~uint32_t{0} << sl);
        if (slMap == 0) {
            if (fl + 1 >= kFliMax) return nullptr;

            uint32_t flMap = m_flBitmap & (~uint32_t{0} << (fl + 1));
            if (flMap == 0) return nullptr;
            
            fl = FindFirstSet(flMap);
            slMap = m_slBitmaps[fl];
        }
        
        sl = FindFirstSet(slMap);
        return m_blocks[fl][sl];
    }

    bool TLSFAllocator::IsLiveBlock(void* ptr) const noexcept
    {
        if (!m_poolBuffer || !ptr) {
            return false;
        }

        const auto* poolBegin = static_cast<const uint8_t*>(m_poolBuffer);
        const auto* poolEnd = poolBegin + m_poolSize;
        const auto* payload = static_cast<const uint8_t*>(ptr);
        if (payload < poolBegin + sizeof(Block) || payload >= poolEnd) {
            return false;
        }

        const auto* block = reinterpret_cast<const Block*>(payload - sizeof(Block));
        if (reinterpret_cast<const uint8_t*>(block) < poolBegin || reinterpret_cast<const uint8_t*>(block) + sizeof(Block) > poolEnd) {
            return false;
        }

        const size_t blockSize = block->GetSize();
        if (blockSize < kMinBlockSize || (blockSize % kMinBlockSize) != 0) {
            return false;
        }

        const auto* blockEnd = reinterpret_cast<const uint8_t*>(block) + sizeof(Block) + blockSize;
        return blockEnd <= poolEnd && !block->IsFree();
    }

} // namespace Engine::Memory
