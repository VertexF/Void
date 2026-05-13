#ifndef FOUNDATION_MEMORY_ALIGNMENT_HDR
#define FOUNDATION_MEMORY_ALIGNMENT_HDR

#include <cstdint>

namespace Engine::Memory {
    /// @brief Fundamental max-alignment surrogate for allocator defaults.
    /// @details Freestanding replacement for std::max_align_t.
    union MaxAlignT {
        long double AsLongDouble;
        void* AsPointer;
    };

    /// @brief Align a Pointer up to the specified alignment
    /// @param ptr Pointer to align
    /// @param alignment Alignment requirement (must be power of 2)
    /// @return Aligned Pointer (>= ptr)
    inline void* AlignPointer(void* ptr, size_t alignment) noexcept {
        uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t aligned = (address + alignment - 1) & ~(alignment - 1);
        return reinterpret_cast<void*>(aligned);
    }

    /// @brief Align a const Pointer up to the specified alignment
    /// @param ptr Pointer to align
    /// @param alignment Alignment requirement (must be power of 2)
    /// @return Aligned Pointer (>= ptr)
    inline const void* AlignPointer(const void* ptr, size_t alignment) noexcept {
        return AlignPointer(const_cast<void*>(ptr), alignment);
    }

    /// @brief Calculate adjustment needed to align a Pointer
    /// @param ptr Pointer to check
    /// @param alignment Alignment requirement (must be power of 2)
    /// @return Number of bytes to add to ptr to achieve alignment
    inline size_t AlignmentAdjustment(const void* ptr, size_t alignment) noexcept {
        uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t aligned = (address + alignment - 1) & ~(alignment - 1);
        return static_cast<size_t>(aligned - address);
    }

    /// @brief Calculate alignment adjustment with space for a header
    /// @param ptr Pointer to check
    /// @param alignment Alignment requirement (must be power of 2)
    /// @param headerSize Size of header to reserve before aligned Pointer
    /// @return Number of bytes to add to ptr to fit header and achieve alignment
    inline size_t AlignmentAdjustmentWithHeader(const void* ptr, size_t alignment, size_t headerSize) noexcept {
        size_t adjustment = AlignmentAdjustment(ptr, alignment);
        if (adjustment < headerSize) {
            headerSize -= adjustment;
            adjustment += alignment * ((headerSize + alignment - 1) / alignment);
        }
        return adjustment;
    }

    /// @brief Check if a value is a power of two
    /// @param value Value to check
    /// @return true if value is a power of two (including 1)
    constexpr bool IsPowerOfTwo(size_t value) noexcept {
        return value && !(value & (value - 1));
    }

    /// @brief Align a size up to the specified alignment
    /// @param size Size to align
    /// @param alignment Alignment requirement (must be power of 2)
    /// @return Aligned size (>= size)
    constexpr size_t AlignSize(size_t size, size_t alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_ALIGNMENT_HDR
