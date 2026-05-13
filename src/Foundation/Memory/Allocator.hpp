#ifndef FOUNDATION_MEMORY_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_ALLOCATOR_HDR

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryTag.hpp>
#include <Foundation/Platform.hpp>
#include <Foundation/Threading/Atomic.hpp>
#include <new>

#ifdef GetFreeSpace
#undef GetFreeSpace
#endif

namespace Engine::Memory {

#if !defined(NDEBUG) || defined(ENGINE_MEMORY_DIAGNOSTICS)
    inline constexpr bool kAllocatorDetailedStatsEnabled = true;
#else
    inline constexpr bool kAllocatorDetailedStatsEnabled = false;
#endif

#if !defined(ENGINE_MEMORY_TRACK_OWNERSHIP)
    #if !defined(NDEBUG) || defined(ENGINE_MEMORY_DIAGNOSTICS)
        #define ENGINE_MEMORY_TRACK_OWNERSHIP 1
    #else
        #define ENGINE_MEMORY_TRACK_OWNERSHIP 0
    #endif
#endif
    inline constexpr bool kAllocatorOwnershipTrackingEnabled = ENGINE_MEMORY_TRACK_OWNERSHIP != 0;

    struct AllocatorStats {
        const char* name = nullptr;
        size_t liveBytes = 0;
        size_t peakBytes = 0;
        size_t allocationCount = 0;
        size_t freeCount = 0;
        size_t failedAllocationCount = 0;
        size_t liveAllocationCount = 0;
        size_t reservedBytes = 0;
        size_t committedBytes = 0;
        size_t freeBytes = 0;
        size_t largestFreeBlockBytes = 0;
        size_t fragmentationBytes = 0;
    };

    enum class AllocatorStatsDetail : uint8 {
        Fast,
        Detailed
    };

    class AllocatorStatsTracker final {
    public:
        void RecordAllocation(size_t size) noexcept {
            if constexpr (!kAllocatorDetailedStatsEnabled) {
                (void)size;
                return;
            }
            const size_t live = m_liveBytes.FetchAdd(size, MemoryOrder::Relaxed) + size;
            m_allocationCount.FetchAdd(1, MemoryOrder::Relaxed);
            m_liveAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
            UpdatePeak(live);
        }

        void RecordFree(size_t size) noexcept {
            if constexpr (!kAllocatorDetailedStatsEnabled) {
                (void)size;
                return;
            }
            SaturatingSubtract(m_liveBytes, size);
            SaturatingSubtract(m_liveAllocationCount, 1);
            m_freeCount.FetchAdd(1, MemoryOrder::Relaxed);
        }

        void RecordResize(size_t oldSize, size_t newSize) noexcept {
            if constexpr (!kAllocatorDetailedStatsEnabled) {
                (void)oldSize;
                (void)newSize;
                return;
            }
            if (oldSize == newSize) {
                return;
            }
            if (newSize > oldSize) {
                const size_t delta = newSize - oldSize;
                const size_t live = m_liveBytes.FetchAdd(delta, MemoryOrder::Relaxed) + delta;
                UpdatePeak(live);
                return;
            }
            const size_t delta = oldSize - newSize;
            SaturatingSubtract(m_liveBytes, delta);
        }

        void RecordFailedAllocation() noexcept {
            if constexpr (!kAllocatorDetailedStatsEnabled) {
                return;
            }
            m_failedAllocationCount.FetchAdd(1, MemoryOrder::Relaxed);
        }

        void ResetLive() noexcept {
            if constexpr (!kAllocatorDetailedStatsEnabled) {
                return;
            }
            m_liveBytes.Store(0, MemoryOrder::Relaxed);
            m_liveAllocationCount.Store(0, MemoryOrder::Relaxed);
        }

        [[nodiscard]] AllocatorStats Snapshot(const char* name) const noexcept {
            AllocatorStats stats{};
            stats.name = name;
            stats.liveBytes = m_liveBytes.Load(MemoryOrder::Relaxed);
            stats.peakBytes = m_peakBytes.Load(MemoryOrder::Relaxed);
            stats.allocationCount = m_allocationCount.Load(MemoryOrder::Relaxed);
            stats.freeCount = m_freeCount.Load(MemoryOrder::Relaxed);
            stats.failedAllocationCount = m_failedAllocationCount.Load(MemoryOrder::Relaxed);
            stats.liveAllocationCount = m_liveAllocationCount.Load(MemoryOrder::Relaxed);
            return stats;
        }

    private:
        static void SaturatingSubtract(Atomic<size_t>& value, size_t amount) noexcept {
            size_t current = value.Load(MemoryOrder::Relaxed);
            while (true) {
                const size_t desired = amount <= current ? current - amount : 0;
                if (value.CompareExchangeWeak(current, desired, MemoryOrder::Relaxed, MemoryOrder::Relaxed)) {
                    return;
                }
            }
        }

        void UpdatePeak(size_t live) noexcept {
            size_t peak = m_peakBytes.Load(MemoryOrder::Relaxed);
            while (live > peak &&
                   !m_peakBytes.CompareExchangeWeak(peak, live, MemoryOrder::Relaxed, MemoryOrder::Relaxed)) {
            }
        }

        Atomic<size_t> m_liveBytes{0};
        Atomic<size_t> m_peakBytes{0};
        Atomic<size_t> m_allocationCount{0};
        Atomic<size_t> m_freeCount{0};
        Atomic<size_t> m_failedAllocationCount{0};
        Atomic<size_t> m_liveAllocationCount{0};
    };

    /// @brief Base allocator interface
    class IAllocator {
    public:
        virtual ~IAllocator() = default;

        /// @brief Allocate memory block
        /// @param size Size in bytes to allocate
        /// @param alignment Alignment requirement (must be power of 2)
        /// @return Pointer to allocated memory, or nullptr on failure
        virtual void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) = 0;

        /// @brief Free a previously allocated block
        /// @param ptr Pointer returned by Allocate
        virtual void Free(void* ptr) = 0;

        /// @brief Reallocate a memory block
        /// @param ptr Pointer to previously allocated memory (can be nullptr)
        /// @param newSize New size in bytes
        /// @param alignment Alignment requirement (must be power of 2)
        /// @return Pointer to new memory block, or nullptr on failure.
        ///         If ptr is null, acts like Allocate.
        ///         If newSize is 0, acts like Free (returns nullptr).
        virtual void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) {
            // Default implementation returns nullptr. Subclasses should override.
            if (newSize == 0) {
                Free(ptr);
                return nullptr;
            }
            if (ptr == nullptr) {
                return Allocate(newSize, alignment);
            }
            return nullptr;
        }

        /// @brief Get total allocated bytes
        virtual size_t AllocatedSize() const = 0;

        /// @brief Get allocator name for debugging
        virtual const char* Name() const = 0;

        /// @brief Check if allocator owns this pointer.
        /// @details Must be non-invasive: implementations may inspect allocator-owned
        ///          ranges, registries, or metadata after a range proof, but must not
        ///          dereference caller-adjacent memory to discover ownership. If
        ///          ownership cannot be proven cheaply, return false.
        virtual bool Owns(void* ptr) const = 0;

        /// @brief Snapshot cheap allocator counters only. Must avoid structural free-list/page walks.
        virtual AllocatorStats GetStats() const {
            AllocatorStats stats{};
            stats.name = Name();
            stats.liveBytes = AllocatedSize();
            stats.peakBytes = stats.liveBytes;
            return stats;
        }

        /// @brief Snapshot full allocator telemetry. May take structural locks and scan free lists.
        virtual AllocatorStats GetDetailedStats() const {
            return GetStats();
        }
    };

    /// @brief Allocation header stored before each allocation (for tracking)
    struct AllocationHeader {
        size_t size;
        size_t adjustment;
        MemoryTag tag;
        size_t padding;
    };

    struct ArrayAllocationHeader {
        size_t count;
        void* allocation;
    };

    /// @brief Get the system default allocator (uses malloc/free)
    IAllocator& GetDefaultAllocator();

    /// @brief Set the global default allocator
    void SetDefaultAllocator(IAllocator* allocator);

    // ============================================================================
    // Typed allocation helpers
    // ============================================================================

    /// @brief Allocate and construct a single object
    template<typename T, typename... Args>
    T* New(IAllocator& allocator, Args&&... args) {
        void* memory = allocator.Allocate(sizeof(T), alignof(T));
        if (!memory) return nullptr;
        return new (memory) T(std::forward<Args>(args)...);
    }

    /// @brief Destruct and free a single object
    template<typename T>
    void Delete(IAllocator& allocator, T* ptr) {
        if (ptr) {
            ptr->~T();
            allocator.Free(ptr);
        }
    }

    /// @brief Allocate array of objects (default constructed)
    template<typename T>
    T* NewArray(IAllocator& allocator, size_t count) {
        constexpr size_t headerSize = sizeof(ArrayAllocationHeader);
        constexpr size_t arrayAlignment = alignof(T) > alignof(ArrayAllocationHeader)
            ? alignof(T)
            : alignof(ArrayAllocationHeader);
        constexpr size_t maxAlignmentPadding = arrayAlignment - 1;

        if (count > ((std::numeric_limits<size_t>::max)() - headerSize - maxAlignmentPadding) / sizeof(T)) {
            return nullptr;
        }

        const size_t totalSize = headerSize + maxAlignmentPadding + sizeof(T) * count;

        void* memory = allocator.Allocate(totalSize, arrayAlignment);
        if (!memory) return nullptr;

        T* array = static_cast<T*>(AlignPointer(static_cast<uint8_t*>(memory) + headerSize, arrayAlignment));
        auto* header = reinterpret_cast<ArrayAllocationHeader*>(reinterpret_cast<uint8_t*>(array) - headerSize);
        header->count = count;
        header->allocation = memory;

        for (size_t i = 0; i < count; ++i) {
            new (array + i) T();
        }
        return array;
    }

    /// @brief Destruct and free array
    template<typename T>
    void DeleteArray(IAllocator& allocator, T* array) {
        if (array) {
            auto* header = reinterpret_cast<ArrayAllocationHeader*>(reinterpret_cast<uint8_t*>(array) - sizeof(ArrayAllocationHeader));
            size_t count = header->count;

            for (size_t i = 0; i < count; ++i) {
                array[i].~T();
            }
            allocator.Free(header->allocation);
        }
    }
} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_ALLOCATOR_HDR
