#ifndef FOUNDATION_MEMORY_ALIGNED_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_ALIGNED_ALLOCATOR_HDR

// AlignedAllocator — SIMD-aligned memory allocation
// ============================================================================
// Guarantees alignment >= requested for SIMD operations (particle systems,
// vertex buffers, audio processing, matrix pools).
// Originated from ELT memory/allocator.h aligned_allocator.
// ============================================================================

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Platform.hpp>

#include <cstdlib>
#include <limits>

#if !defined(NDEBUG)
    #include <Foundation/Threading/Lock/SpinLock.hpp>
    #include <unordered_set>
#endif

#if defined(ENGINE_PLATFORM_WINDOWS)
    #include <malloc.h>
#endif

namespace Engine::Memory {

namespace Detail {
struct AlignedAllocationHeader {
    size_t size = 0;
    size_t adjustment = 0;
    size_t magic = 0;
};

inline constexpr size_t kAlignedAllocationMagic = 0x414C474Eu; // ALGN
}

/// @brief Allocator that guarantees SIMD-friendly alignment (default 64 = cache line)
/// @details Wraps another allocator (or system malloc) to enforce minimum alignment.
///          Use for buffers fed to SIMD intrinsics, GPU uploads, or DMA.
class AlignedAllocator final : public IAllocator {
public:
    /// @param minAlignment Minimum alignment in bytes (must be power of 2, default: 64)
    /// @param backing Optional backing allocator (nullptr = system malloc)
    explicit AlignedAllocator(usize minAlignment = 64, IAllocator* backing = nullptr) noexcept
        : m_minAlignment(minAlignment), m_backing(backing), m_allocated(0) {}

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(max_align_t)) override {
        if (size == 0) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "AlignedAllocator: zero-size allocation");
            return nullptr;
        }
        if (!IsPowerOfTwo(m_minAlignment)) {
            m_minAlignment = alignof(MaxAlignT);
        }
        if (!IsPowerOfTwo(alignment)) {
            alignment = alignof(MaxAlignT);
        }
        alignment = alignment > m_minAlignment ? alignment : m_minAlignment;
        if (alignment < alignof(MaxAlignT)) {
            alignment = alignof(MaxAlignT);
        }

        constexpr size_t headerSize = sizeof(Detail::AlignedAllocationHeader);
        if (size > (std::numeric_limits<size_t>::max)() - headerSize - alignment) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "AlignedAllocator: allocation size overflow");
            return nullptr;
        }

        const size_t totalSize = size + headerSize + alignment;
        void* raw = nullptr;

        if (m_backing) {
            raw = m_backing->Allocate(totalSize, alignment);
        } else {
#if defined(ENGINE_PLATFORM_WINDOWS)
            raw = _aligned_malloc(totalSize, alignment);
#else
            // posix_memalign or aligned_alloc
            if (alignment < sizeof(void*)) alignment = sizeof(void*);
            raw = ::aligned_alloc(alignment, AlignSize(totalSize, alignment));
#endif
        }

        if (!raw) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "AlignedAllocator: allocation failed");
            return nullptr;
        }

        auto* rawBytes = static_cast<uint8*>(raw);
        auto* aligned = static_cast<uint8*>(AlignPointer(rawBytes + headerSize, alignment));
        auto* header = reinterpret_cast<Detail::AlignedAllocationHeader*>(aligned - headerSize);
        header->size = size;
        header->adjustment = static_cast<size_t>(aligned - rawBytes);
        header->magic = Detail::kAlignedAllocationMagic;

        m_allocated += size;
        m_stats.RecordAllocation(size);
#if !defined(NDEBUG)
        Threading::SpinLockGuard guard(m_lock);
        m_liveAllocations.insert(aligned);
#endif
        return aligned;
    }

    void Free(void* ptr) override {
        if (!ptr) return;

#if !defined(NDEBUG)
        {
            Threading::SpinLockGuard guard(m_lock);
            const auto it = m_liveAllocations.find(ptr);
            if (it == m_liveAllocations.end()) {
                ReportAllocatorFailure(m_stats, AllocatorFailureKind::DoubleFree, "AlignedAllocator: invalid or double free");
                return;
            }
            m_liveAllocations.erase(it);
        }
#endif

        auto* aligned = static_cast<uint8*>(ptr);
        auto* header = reinterpret_cast<Detail::AlignedAllocationHeader*>(aligned - sizeof(Detail::AlignedAllocationHeader));
        if (header->magic != Detail::kAlignedAllocationMagic) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::Corruption, "AlignedAllocator: allocation header corrupted");
            return;
        }
        const size_t size = header->size;
        void* raw = aligned - header->adjustment;
        if (m_allocated >= size) {
            m_allocated -= size;
        } else {
            m_allocated = 0;
        }
        m_stats.RecordFree(size);
        header->magic = 0;

        if (m_backing) {
            m_backing->Free(raw);
        } else {
#if defined(ENGINE_PLATFORM_WINDOWS)
            _aligned_free(raw);
#else
            ::free(raw);
#endif
        }
    }

    [[nodiscard]] size_t AllocatedSize() const override { return m_allocated; }
    [[nodiscard]] const char* Name() const override { return "AlignedAllocator"; }
    [[nodiscard]] bool Owns(void* ptr) const override {
        if (!ptr) {
            return false;
        }
#if !defined(NDEBUG)
        Threading::SpinLockGuard guard(m_lock);
        return m_liveAllocations.find(ptr) != m_liveAllocations.end();
#else
        const auto* aligned = static_cast<const uint8*>(ptr);
        const auto* header = reinterpret_cast<const Detail::AlignedAllocationHeader*>(
            aligned - sizeof(Detail::AlignedAllocationHeader));
        return header->magic == Detail::kAlignedAllocationMagic;
#endif
    }

    [[nodiscard]] AllocatorStats GetStats() const override {
        AllocatorStats stats = m_stats.Snapshot(Name());
        stats.liveBytes = m_allocated;
        return stats;
    }

private:
    usize m_minAlignment;
    IAllocator* m_backing;
    size_t m_allocated;
    AllocatorStatsTracker m_stats;
#if !defined(NDEBUG)
    std::unordered_set<void*> m_liveAllocations;
    mutable Threading::SpinLock m_lock;
#endif
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_ALIGNED_ALLOCATOR_HDR
