#ifndef FOUNDATION_MEMORY_OPERATIONS_HDR
#define FOUNDATION_MEMORY_OPERATIONS_HDR

#include <Foundation/Platform.hpp>

#include <cstring>

// Originated from ELT fw_mem.h

namespace Engine {

    /// @brief Copy n bytes from src to dst. Regions must not overlap.
    inline void* MemCopy(void* dst, const void* src, size_t n) noexcept
    {
        return std::memcpy(dst, src, n);
    }

    /// @brief Set n bytes at dst to value.
    inline void* MemSet(void* dst, int32_t value, size_t n) noexcept
    {
        return std::memset(dst, value, n);
    }

    /// @brief Copy n bytes from src to dst. Regions may overlap.
    inline void* MemMove(void* dst, const void* src, size_t n) noexcept
    {
        return std::memmove(dst, src, n);
    }

    /// @brief Compare n bytes. Returns 0 if equal, <0 or >0 otherwise.
    inline int32_t MemCompare(const void* a, const void* b, size_t n) noexcept
    {
        return std::memcmp(a, b, n);
    }

    /// @brief Zero n bytes at dst.
    inline void MemZero(void* dst, size_t n) noexcept
    {
        MemSet(dst, 0, n);
    }

    // ============================================================================
    // Template-Specialized Small Memory Operations
    // ============================================================================
    // For compile-time-known sizes, the compiler can fully unroll these into
    // register-width moves. Superior to runtime MemCopy/MemSet for small,
    // known-size transfers (struct copies, SIMD lane shuffles, etc.).

    /// @brief Copy exactly N bytes — unrolled at compile time for small sizes.
    template<size_t N>
    inline void FastCopy(void* dst, const void* src) noexcept
    {
        if constexpr (N == 0) {
            // Nothing
        } else if constexpr (N == 1) {
            *static_cast<uint8_t*>(dst) = *static_cast<const uint8_t*>(src);
        } else if constexpr (N == 2) {
            *static_cast<uint16_t*>(dst) = *static_cast<const uint16_t*>(src);
        } else if constexpr (N == 4) {
            *static_cast<uint32_t*>(dst) = *static_cast<const uint32_t*>(src);
        } else if constexpr (N == 8) {
            *static_cast<uint64_t*>(dst) = *static_cast<const uint64_t*>(src);
        } else if constexpr (N == 16) {
            auto* d = static_cast<uint64_t*>(dst);
            const auto* s = static_cast<const uint64_t*>(src);
            d[0] = s[0]; d[1] = s[1];
        } else if constexpr (N == 32) {
            auto* d = static_cast<uint64_t*>(dst);
            const auto* s = static_cast<const uint64_t*>(src);
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        } else if constexpr (N <= 64) {
            // Recurse: split in half
            constexpr size_t Half = N / 2;
            constexpr size_t Rest = N - Half;
            FastCopy<Half>(dst, src);
            FastCopy<Rest>(static_cast<uint8_t*>(dst) + Half, static_cast<const uint8_t*>(src) + Half);
        } else {
            // Fall back to loop for large known sizes (compiler will still optimize)
            MemCopy(dst, src, N);
        }
    }

    /// @brief Set exactly N bytes to a value — unrolled at compile time.
    template<size_t N>
    inline void FastSet(void* dst, uint8_t value) noexcept
    {
        if constexpr (N == 0) {
            // Nothing
        } else if constexpr (N == 1) {
            *static_cast<uint8_t*>(dst) = value;
        } else if constexpr (N == 2) {
            uint16_t v = static_cast<uint16_t>(value) | (static_cast<uint16_t>(value) << 8);
            *static_cast<uint16_t*>(dst) = v;
        } else if constexpr (N == 4) {
            uint32_t v = static_cast<uint32_t>(value) * 0x01010101u;
            *static_cast<uint32_t*>(dst) = v;
        } else if constexpr (N == 8) {
            uint64_t v = static_cast<uint64_t>(value) * 0x0101010101010101ULL;
            *static_cast<uint64_t*>(dst) = v;
        } else if constexpr (N <= 64) {
            constexpr size_t Half = N / 2;
            constexpr size_t Rest = N - Half;
            FastSet<Half>(dst, value);
            FastSet<Rest>(static_cast<uint8_t*>(dst) + Half, value);
        } else {
            MemSet(dst, value, N);
        }
    }

    /// @brief Zero-initialize a trivially copyable struct.
    /// Safer than MemZero with raw pointers — preserves type information.
    template<typename T>
    inline void ZeroStruct(T& obj) noexcept
    {
        static_assert(__is_trivially_copyable(T), "ZeroStruct only works on trivially copyable types");
        FastSet<sizeof(T)>(&obj, 0);
    }

    /// @brief Zero-initialize a trivially copyable struct, returning it.
    /// Useful for initializing C API structs: `auto desc = ZeroInit<VkBufferCreateInfo>();`
    template<typename T>
    inline T ZeroInit() noexcept
    {
        static_assert(__is_trivially_copyable(T), "ZeroInit only works on trivially copyable types");
        T obj;
        FastSet<sizeof(T)>(&obj, 0);
        return obj;
    }

} // namespace Engine

#endif // !FOUNDATION_MEMORY_OPERATIONS_HDR
