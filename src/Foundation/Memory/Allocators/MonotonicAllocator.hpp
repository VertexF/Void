#ifndef FOUNDATION_MEMORY_MONOTONIC_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_MONOTONIC_ALLOCATOR_HDR

// O(1) bump allocation while the current block has capacity; bulk Release(). Ideal for:
//   - Per-frame scratch allocations
//   - Command buffer building
//   - Particle system temporaries
//   - Any scope where you allocate many, free all at once

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Platform.hpp>
#include <Foundation/Memory/Operations.hpp>
#include <Utility/Assert.hpp>

#include <limits>

namespace Engine::Memory {

/// @brief Bump allocator with O(1) fast path and upstream block growth on exhaustion.
///        Call Release() to free everything at once.
///        Grows by allocating new blocks from an upstream allocator.
class MonotonicAllocator final : public IAllocator {
public:
    /// @brief Construct with upstream allocator and optional initial capacity.
    explicit MonotonicAllocator(
        IAllocator& upstream,
        usize initialCapacity = kDefaultInitialCapacity,
        const char* name = "MonotonicAllocator") noexcept
        : m_upstream(upstream)
        , m_name(name)
    {
        if (initialCapacity > 0) {
            AllocateBlock(initialCapacity);
        }
    }

    /// @brief Construct over a user-provided buffer (no upstream alloc until exhausted).
    MonotonicAllocator(
        void* buffer, usize bufferSize,
        IAllocator& upstream,
        const char* name = "MonotonicAllocator") noexcept
        : m_upstream(upstream)
        , m_initialBegin(static_cast<uint8*>(buffer))
        , m_initialEnd(static_cast<uint8*>(buffer) + bufferSize)
        , m_current(static_cast<uint8*>(buffer))
        , m_end(static_cast<uint8*>(buffer) + bufferSize)
        , m_name(name)
    {}

    ~MonotonicAllocator() override { Release(); }

    // Non-copyable, non-movable
    MonotonicAllocator(const MonotonicAllocator&) = delete;
    MonotonicAllocator& operator=(const MonotonicAllocator&) = delete;

    // -- IAllocator interface ------------------------------------------------

    [[nodiscard]] void* Allocate(usize size, usize alignment = 8) override {
        if (size == 0) {
            size = 1;
        }
        if (!IsPowerOfTwo(alignment)) {
            alignment = alignof(MaxAlignT);
        }
        if (alignment < alignof(MaxAlignT)) {
            alignment = alignof(MaxAlignT);
        }

        if (void* ptr = TryAllocateFromCurrent(size, alignment)) {
            return ptr;
        }

        const usize required = RequiredBytes(size, alignment);
        if (required == 0) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "MonotonicAllocator: allocation size overflow");
            return nullptr;
        }

        const usize blockSize = Max(m_nextBlockSize, required);
        AllocateBlock(blockSize);

        void* ptr = TryAllocateFromCurrent(size, alignment);
        if (!ptr) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "MonotonicAllocator: allocation failed after block growth");
        }
        return ptr;
    }

    void Free(void* /*ptr*/) override {
        // Monotonic: individual free is a no-op
    }

    [[nodiscard]] void* Reallocate(void* ptr, usize newSize, usize alignment = 8) override {
        if (!ptr) {
            return Allocate(newSize, alignment);
        }
        if (newSize == 0) {
            if (Owns(ptr)) {
                auto* header = reinterpret_cast<AllocationRecord*>(static_cast<uint8*>(ptr) - sizeof(AllocationRecord));
                if (header->magic == kAllocationMagic && header->generation == m_generation) {
                    m_totalAllocated -= header->size;
                    m_stats.RecordFree(header->size);
                    header->magic = 0;
                }
            }
            return nullptr;
        }
        if (!Owns(ptr)) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "MonotonicAllocator: reallocate pointer is not live");
            return nullptr;
        }

        auto* header = reinterpret_cast<AllocationRecord*>(static_cast<uint8*>(ptr) - sizeof(AllocationRecord));
        if (header->magic != kAllocationMagic || header->generation != m_generation) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "MonotonicAllocator: allocation record is not live");
            return nullptr;
        }

        const usize oldSize = header->size;
        if (newSize <= oldSize) {
            header->size = newSize;
            m_totalAllocated -= oldSize - newSize;
            m_stats.RecordResize(oldSize, newSize);
            return ptr;
        }

        void* newPtr = Allocate(newSize, alignment);
        if (!newPtr) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "MonotonicAllocator: reallocate allocation failed");
            return nullptr;
        }

        MemCopy(newPtr, ptr, oldSize);
        m_totalAllocated -= oldSize;
        m_stats.RecordFree(oldSize);
        header->magic = 0;
        return newPtr;
    }

    [[nodiscard]] usize AllocatedSize() const override { return m_totalAllocated; }
    [[nodiscard]] const char* Name() const override { return m_name; }
    [[nodiscard]] AllocatorStats GetStats() const override {
        AllocatorStats stats = m_stats.Snapshot(Name());
        stats.liveBytes = m_totalAllocated;
        return stats;
    }

    [[nodiscard]] AllocatorStats GetDetailedStats() const override {
        AllocatorStats stats = GetStats();
        if (m_initialBegin && m_initialEnd > m_initialBegin) {
            stats.reservedBytes += static_cast<usize>(m_initialEnd - m_initialBegin);
        }
        for (Block* block = m_blockList; block; block = block->next) {
            stats.reservedBytes += block->size;
        }
        stats.committedBytes = stats.reservedBytes;
        stats.freeBytes = GetRemainingCapacity();
        stats.largestFreeBlockBytes = stats.freeBytes;
        return stats;
    }
    [[nodiscard]] bool Owns(void* ptr) const override {
        if (!ptr) {
            return false;
        }

        const auto* bytes = static_cast<const uint8*>(ptr);
        if (m_initialBegin && bytes >= m_initialBegin + sizeof(AllocationRecord) && bytes < m_initialEnd) {
            return IsLiveAllocationRecord(bytes);
        }

        for (Block* block = m_blockList; block; block = block->next) {
            const auto* begin = reinterpret_cast<const uint8*>(block) + sizeof(Block);
            const auto* end = begin + block->size;
            if (bytes >= begin + sizeof(AllocationRecord) && bytes < end) {
                return IsLiveAllocationRecord(bytes);
            }
        }
        return false;
    }

    // -- Monotonic-specific --------------------------------------------------

    /// @brief Release ALL memory back to the upstream allocator.
    void Release() noexcept {
        Block* block = m_blockList;
        while (block) {
            Block* next = block->next;
            m_upstream.Free(block);
            block = next;
        }
        m_blockList = nullptr;
        m_current = m_initialBegin;
        m_end = m_initialEnd;
        m_totalAllocated = 0;
        m_stats.ResetLive();
        m_nextBlockSize = kDefaultInitialCapacity;
        AdvanceGeneration();
    }

    /// @brief Reset the cursor and return overflow blocks to the upstream allocator.
    void Reset() noexcept {
        Release();
    }

    [[nodiscard]] usize GetRemainingCapacity() const noexcept {
        return m_end > m_current ? static_cast<usize>(m_end - m_current) : 0;
    }

private:
    static constexpr usize kDefaultInitialCapacity = 4096;
    static constexpr uint32 kAllocationMagic = 0x4D4F4E4Fu; // MONO

    struct Block {
        Block* next;
        usize size;
    };

    struct AllocationRecord {
        usize size;
        uint32 magic;
        uint32 generation;
    };

    [[nodiscard]] static usize Max(usize a, usize b) noexcept { return a > b ? a : b; }

    [[nodiscard]] static usize RequiredBytes(usize size, usize alignment) noexcept {
        if (size > (std::numeric_limits<usize>::max)() - sizeof(AllocationRecord) - alignment) {
            return 0;
        }
        return sizeof(AllocationRecord) + size + alignment;
    }

    void AdvanceGeneration() noexcept {
        ++m_generation;
        if (m_generation == 0) {
            ++m_generation;
        }
    }

    [[nodiscard]] bool IsLiveAllocationRecord(const uint8* user) const noexcept {
        const auto* header = reinterpret_cast<const AllocationRecord*>(user - sizeof(AllocationRecord));
        return header->magic == kAllocationMagic && header->generation == m_generation;
    }

    [[nodiscard]] void* TryAllocateFromCurrent(usize size, usize alignment) noexcept {
        if (!m_current || !m_end || m_current >= m_end) {
            return nullptr;
        }

        const usize remaining = static_cast<usize>(m_end - m_current);
        const usize required = RequiredBytes(size, alignment);
        if (required == 0 || required > remaining) {
            return nullptr;
        }

        const usize adjustment = AlignmentAdjustmentWithHeader(m_current, alignment, sizeof(AllocationRecord));
        if (adjustment > remaining || size > remaining - adjustment) {
            return nullptr;
        }

        auto* user = m_current + adjustment;
        auto* header = reinterpret_cast<AllocationRecord*>(user - sizeof(AllocationRecord));
        header->size = size;
        header->magic = kAllocationMagic;
        header->generation = m_generation;

        m_current = user + size;
        m_totalAllocated += size;
        m_stats.RecordAllocation(size);
        return user;
    }

    void AllocateBlock(usize capacity) noexcept {
        usize totalSize = sizeof(Block) + capacity;
        void* raw = m_upstream.Allocate(totalSize, alignof(Block));
        ENGINE_ASSERT_MSG(raw != nullptr, "MonotonicAllocator: upstream allocation failed");
        if (!raw) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "MonotonicAllocator: upstream block allocation failed");
            return;
        }

        auto* block = static_cast<Block*>(raw);
        block->size = capacity;
        block->next = m_blockList;
        m_blockList = block;

        m_current = reinterpret_cast<uint8*>(raw) + sizeof(Block);
        m_end = m_current + capacity;

        // Geometric growth: double for next time
        m_nextBlockSize = capacity * 2;
    }

    IAllocator& m_upstream;
    Block*      m_blockList      = nullptr;
    uint8*      m_initialBegin    = nullptr;
    uint8*      m_initialEnd      = nullptr;
    uint8*      m_current        = nullptr;
    uint8*      m_end            = nullptr;
    usize       m_totalAllocated = 0;
    usize       m_nextBlockSize  = kDefaultInitialCapacity;
    uint32      m_generation     = 1;
    const char* m_name;
    AllocatorStatsTracker m_stats;
};

/// @brief Always-fail allocator. Useful for testing that a code path doesn't allocate.
class NullAllocator final : public IAllocator {
public:
    [[nodiscard]] void* Allocate(usize /*size*/, usize /*alignment*/ = 8) override {
        ENGINE_ASSERT_MSG(false, "NullAllocator: allocation attempted");
        return nullptr;
    }
    void Free(void* /*ptr*/) override {}
    [[nodiscard]] usize AllocatedSize() const override { return 0; }
    [[nodiscard]] const char* Name() const override { return "NullAllocator"; }
    [[nodiscard]] bool Owns(void* /*ptr*/) const override { return false; }
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_MONOTONIC_ALLOCATOR_HDR
