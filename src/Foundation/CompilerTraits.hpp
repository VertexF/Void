// ============================================================================
// source/Shared/Primitives/View/Private/CompilerTraits.h
// ALL the macros live here - traditional header, NOT a module
// ============================================================================
#pragma once

// ============================================================================
// Compiler Detection
// ============================================================================

// MSVC
#if defined(_MSC_VER)
    #define ENGINE_COMPILER_MSVC 1
    #define ENGINE_COMPILER_VERSION _MSC_VER
    #define ENGINE_COMPILER_NAME "MSVC"
    
    #if _MSC_VER >= 1930
        #define ENGINE_COMPILER_MSVC_2022 1
    #elif _MSC_VER >= 1920
        #define ENGINE_COMPILER_MSVC_2019 1
    #endif

// Clang (check before GCC since Clang defines __GNUC__)
#elif defined(__clang__)
    #define ENGINE_COMPILER_CLANG 1
    #define ENGINE_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
    #define ENGINE_COMPILER_NAME "Clang"
    
    #if defined(__apple_build_version__)
        #define ENGINE_COMPILER_APPLE_CLANG 1
    #endif

// GCC
#elif defined(__GNUC__)
    #define ENGINE_COMPILER_GCC 1
    #define ENGINE_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #define ENGINE_COMPILER_NAME "GCC"

// Intel
#elif defined(__INTEL_COMPILER) || defined(__ICL)
    #define ENGINE_COMPILER_INTEL 1
    #define ENGINE_COMPILER_VERSION __INTEL_COMPILER
    #define ENGINE_COMPILER_NAME "Intel"

#else
    #define ENGINE_COMPILER_UNKNOWN 1
    #define ENGINE_COMPILER_VERSION 0
    #define ENGINE_COMPILER_NAME "Unknown"
#endif

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define ENGINE_PLATFORM_WINDOWS 1
    #define ENGINE_PLATFORM_NAME "Windows"
    
    #if defined(_WIN64)
        #define ENGINE_PLATFORM_64BIT 1
    #else
        #define ENGINE_PLATFORM_32BIT 1
    #endif

#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    
    #if TARGET_OS_IPHONE || TARGET_OS_IOS
        #define ENGINE_PLATFORM_IOS 1
        #define ENGINE_PLATFORM_NAME "iOS"
    #elif TARGET_OS_MAC
        #define ENGINE_PLATFORM_MACOS 1
        #define ENGINE_PLATFORM_NAME "macOS"
    #endif
    
    #define ENGINE_PLATFORM_APPLE 1
    #define ENGINE_PLATFORM_POSIX 1
    #define ENGINE_PLATFORM_64BIT 1

#elif defined(__linux__)
    #define ENGINE_PLATFORM_LINUX 1
    #define ENGINE_PLATFORM_POSIX 1
    #define ENGINE_PLATFORM_NAME "Linux"
    
    #if defined(__LP64__) || defined(_LP64)
        #define ENGINE_PLATFORM_64BIT 1
    #else
        #define ENGINE_PLATFORM_32BIT 1
    #endif

#elif defined(__ANDROID__)
    #define ENGINE_PLATFORM_ANDROID 1
    #define ENGINE_PLATFORM_POSIX 1
    #define ENGINE_PLATFORM_NAME "Android"

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #define ENGINE_PLATFORM_BSD 1
    #define ENGINE_PLATFORM_POSIX 1
    #define ENGINE_PLATFORM_NAME "BSD"

#else
    #define ENGINE_PLATFORM_UNKNOWN 1
    #define ENGINE_PLATFORM_NAME "Unknown"
#endif

// ============================================================================
// Architecture Detection
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define ENGINE_ARCH_X64 1
    #define ENGINE_ARCH_NAME "x86_64"
    #define ENGINE_ARCH_64BIT 1
    
#elif defined(__i386__) || defined(_M_IX86) || defined(__i386)
    #define ENGINE_ARCH_X86 1
    #define ENGINE_ARCH_NAME "x86"
    #define ENGINE_ARCH_32BIT 1
    
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ENGINE_ARCH_ARM64 1
    #define ENGINE_ARCH_NAME "ARM64"
    #define ENGINE_ARCH_64BIT 1
    
#elif defined(__arm__) || defined(_M_ARM)
    #define ENGINE_ARCH_ARM 1
    #define ENGINE_ARCH_NAME "ARM"
    #define ENGINE_ARCH_32BIT 1
    
#elif defined(__riscv)
    #define ENGINE_ARCH_RISCV 1
    #define ENGINE_ARCH_NAME "RISC-V"
    #if __riscv_xlen == 64
        #define ENGINE_ARCH_64BIT 1
    #else
        #define ENGINE_ARCH_32BIT 1
    #endif
    
#else
    #define ENGINE_ARCH_UNKNOWN 1
    #define ENGINE_ARCH_NAME "Unknown"
#endif

// ============================================================================
// Endianness Detection
// ============================================================================

#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define ENGINE_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define ENGINE_BIG_ENDIAN 1
    #endif
#elif defined(_WIN32)
    // Windows is always little-endian on supported architectures
    #define ENGINE_LITTLE_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__THUMBEL__) || \
      defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    #define ENGINE_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || \
      defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
    #define ENGINE_BIG_ENDIAN 1
#else
    // Default assumption
    #define ENGINE_LITTLE_ENDIAN 1
#endif

// ============================================================================
// C++ Standard Detection
// ============================================================================

#if defined(_MSVC_LANG)
    #define ENGINE_CPP_VERSION _MSVC_LANG
#elif defined(__cplusplus)
    #define ENGINE_CPP_VERSION __cplusplus
#else
    #define ENGINE_CPP_VERSION 0
#endif

#if ENGINE_CPP_VERSION >= 202302L
    #define ENGINE_CPP23 1
#endif

#if ENGINE_CPP_VERSION >= 202002L
    #define ENGINE_CPP20 1
#endif

#if ENGINE_CPP_VERSION >= 201703L
    #define ENGINE_CPP17 1
#endif

// Verify minimum C++20 requirement
#if !defined(ENGINE_CPP20)
    #error "Engine requires C++20 or later"
#endif

// ============================================================================
// Compiler-Specific Attributes and Intrinsics
// ============================================================================

// Force inline
#if defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_FORCE_INLINE __forceinline
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define ENGINE_FORCE_INLINE inline
#endif

// No inline
#if defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_NO_INLINE __declspec(noinline)
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_NO_INLINE __attribute__((noinline))
#else
    #define ENGINE_NO_INLINE
#endif

// Restrict qualifier
#if defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_RESTRICT __restrict
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_RESTRICT __restrict__
#else
    #define ENGINE_RESTRICT
#endif

// Alignment
#if defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_ALIGN(n) __declspec(align(n))
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_ALIGN(n) __attribute__((aligned(n)))
#else
    #define ENGINE_ALIGN(n) alignas(n)
#endif

// Assume aligned pointer
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_ASSUME_ALIGNED(ptr, alignment) \
        static_cast<decltype(ptr)>(__builtin_assume_aligned(ptr, alignment))
#elif defined(ENGINE_COMPILER_MSVC) && defined(ENGINE_ARCH_X64)
    // MSVC doesn't have a direct equivalent, but we can use __assume
    #define ENGINE_ASSUME_ALIGNED(ptr, alignment) \
        (__assume((reinterpret_cast<uintptr_t>(ptr) & ((alignment) - 1)) == 0), ptr)
#else
    #define ENGINE_ASSUME_ALIGNED(ptr, alignment) (ptr)
#endif

// Unreachable code marker
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_UNREACHABLE() __builtin_unreachable()
#elif defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_UNREACHABLE() __assume(0)
#else
    #define ENGINE_UNREACHABLE() ((void)0)
#endif

// Assume condition (optimizer hint)
#if defined(ENGINE_COMPILER_CLANG) && __clang_major__ >= 12
    #define ENGINE_ASSUME(expr) __builtin_assume(expr)
#elif defined(ENGINE_COMPILER_GCC) && __GNUC__ >= 13
    // GCC 13+ has __attribute__((assume))
    #define ENGINE_ASSUME(expr) __attribute__((assume(expr)))
#elif defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_ASSUME(expr) __assume(expr)
#else
    #define ENGINE_ASSUME(expr) ((void)0)
#endif

// Branch prediction hints
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_LIKELY(x) __builtin_expect(!!(x), 1)
    #define ENGINE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define ENGINE_LIKELY(x) (x)
    #define ENGINE_UNLIKELY(x) (x)
#endif

// Prefetch
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    // locality: 0=non-temporal, 1=low, 2=moderate, 3=high
    #define ENGINE_PREFETCH_READ(ptr, locality) __builtin_prefetch(ptr, 0, locality)
    #define ENGINE_PREFETCH_WRITE(ptr, locality) __builtin_prefetch(ptr, 1, locality)
#elif defined(ENGINE_COMPILER_MSVC) && (defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86))
    #include <xmmintrin.h>
    #define ENGINE_PREFETCH_READ(ptr, locality) \
        _mm_prefetch(static_cast<const char*>(ptr), \
                     (locality) == 0 ? _MM_HINT_NTA : \
                     (locality) == 1 ? _MM_HINT_T2 : \
                     (locality) == 2 ? _MM_HINT_T1 : _MM_HINT_T0)
    #define ENGINE_PREFETCH_WRITE(ptr, locality) ENGINE_PREFETCH_READ(ptr, locality)
#else
    #define ENGINE_PREFETCH_READ(ptr, locality) ((void)0)
    #define ENGINE_PREFETCH_WRITE(ptr, locality) ((void)0)
#endif

// Function signature for debug
#if defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_FUNCTION_SIG __FUNCSIG__
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_FUNCTION_SIG __PRETTY_FUNCTION__
#else
    #define ENGINE_FUNCTION_SIG __func__
#endif

// Debug break
#if defined(ENGINE_COMPILER_MSVC)
    #define ENGINE_DEBUG_BREAK() __debugbreak()
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #if defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86)
        #define ENGINE_DEBUG_BREAK() __asm__ volatile("int $0x03")
    #elif defined(ENGINE_ARCH_ARM64)
        #define ENGINE_DEBUG_BREAK() __asm__ volatile(".inst 0xd4200000")
    #elif defined(ENGINE_ARCH_ARM)
        #define ENGINE_DEBUG_BREAK() __asm__ volatile("bkpt #0")
    #else
        #include <signal.h>
        #define ENGINE_DEBUG_BREAK() raise(SIGTRAP)
    #endif
#else
    #define ENGINE_DEBUG_BREAK() ((void)0)
#endif

// Count trailing zeros
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_CTZ32(x) __builtin_ctz(x)
    #define ENGINE_CTZ64(x) __builtin_ctzll(x)
#elif defined(ENGINE_COMPILER_MSVC)
    #include <intrin.h>
    inline unsigned long engine_ctz32(unsigned long x) {
        unsigned long index;
        _BitScanForward(&index, x);
        return index;
    }
    inline unsigned long engine_ctz64(unsigned long long x) {
        unsigned long index;
        _BitScanForward64(&index, x);
        return index;
    }
    #define ENGINE_CTZ32(x) engine_ctz32(x)
    #define ENGINE_CTZ64(x) engine_ctz64(x)
#else
    // Fallback using standard library
    #include <bit>
    #define ENGINE_CTZ32(x) std::countr_zero(static_cast<unsigned int>(x))
    #define ENGINE_CTZ64(x) std::countr_zero(static_cast<unsigned long long>(x))
#endif

// Count leading zeros
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_CLZ32(x) __builtin_clz(x)
    #define ENGINE_CLZ64(x) __builtin_clzll(x)
#elif defined(ENGINE_COMPILER_MSVC)
    #include <intrin.h>
    inline unsigned long engine_clz32(unsigned long x) {
        unsigned long index;
        _BitScanReverse(&index, x);
        return 31 - index;
    }
    inline unsigned long engine_clz64(unsigned long long x) {
        unsigned long index;
        _BitScanReverse64(&index, x);
        return 63 - index;
    }
    #define ENGINE_CLZ32(x) engine_clz32(x)
    #define ENGINE_CLZ64(x) engine_clz64(x)
#else
    #include <bit>
    #define ENGINE_CLZ32(x) std::countl_zero(static_cast<unsigned int>(x))
    #define ENGINE_CLZ64(x) std::countl_zero(static_cast<unsigned long long>(x))
#endif

// Population count (count set bits)
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_POPCNT32(x) __builtin_popcount(x)
    #define ENGINE_POPCNT64(x) __builtin_popcountll(x)
#elif defined(ENGINE_COMPILER_MSVC)
    #include <intrin.h>
    #define ENGINE_POPCNT32(x) __popcnt(x)
    #define ENGINE_POPCNT64(x) __popcnt64(x)
#else
    #include <bit>
    #define ENGINE_POPCNT32(x) std::popcount(static_cast<unsigned int>(x))
    #define ENGINE_POPCNT64(x) std::popcount(static_cast<unsigned long long>(x))
#endif

// Byte swap
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_BSWAP16(x) __builtin_bswap16(x)
    #define ENGINE_BSWAP32(x) __builtin_bswap32(x)
    #define ENGINE_BSWAP64(x) __builtin_bswap64(x)
#elif defined(ENGINE_COMPILER_MSVC)
    #include <stdlib.h>
    #define ENGINE_BSWAP16(x) _byteswap_ushort(x)
    #define ENGINE_BSWAP32(x) _byteswap_ulong(x)
    #define ENGINE_BSWAP64(x) _byteswap_uint64(x)
#else
    // Fallback
    inline uint16_t engine_bswap16(uint16_t x) {
        return (x >> 8) | (x << 8);
    }
    inline uint32_t engine_bswap32(uint32_t x) {
        return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) | 
               ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
    }
    inline uint64_t engine_bswap64(uint64_t x) {
        return ((x >> 56) & 0xFF) | ((x >> 40) & 0xFF00) |
               ((x >> 24) & 0xFF0000) | ((x >> 8) & 0xFF000000) |
               ((x << 8) & 0xFF00000000) | ((x << 24) & 0xFF0000000000) |
               ((x << 40) & 0xFF000000000000) | ((x << 56) & 0xFF00000000000000);
    }
    #define ENGINE_BSWAP16(x) engine_bswap16(x)
    #define ENGINE_BSWAP32(x) engine_bswap32(x)
    #define ENGINE_BSWAP64(x) engine_bswap64(x)
#endif

// ============================================================================
// Cache Line Size
// ============================================================================

#if defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86)
    inline constexpr size_t ENGINE_CACHE_LINE_SIZE = 64;
#elif defined(ENGINE_ARCH_ARM64)
    // ARM64 can vary, but 64 is common; Apple Silicon uses 128
    #if defined(ENGINE_PLATFORM_APPLE)
        inline constexpr size_t ENGINE_CACHE_LINE_SIZE = 128;
    #else
        inline constexpr size_t ENGINE_CACHE_LINE_SIZE = 64;
    #endif
#elif defined(ENGINE_ARCH_ARM)
    inline constexpr size_t ENGINE_CACHE_LINE_SIZE = 32;
#else
    inline constexpr size_t ENGINE_CACHE_LINE_SIZE = 64;  // Safe default
#endif

// ============================================================================
// Export/Import Macros (for DLL builds)
// ============================================================================

#if defined(ENGINE_PLATFORM_WINDOWS)
    #if defined(ENGINE_BUILD_SHARED)
        #define ENGINE_EXPORT __declspec(dllexport)
        #define ENGINE_IMPORT __declspec(dllimport)
    #else
        #define ENGINE_EXPORT
        #define ENGINE_IMPORT
    #endif
#elif defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
    #define ENGINE_EXPORT __attribute__((visibility("default")))
    #define ENGINE_IMPORT
#else
    #define ENGINE_EXPORT
    #define ENGINE_IMPORT
#endif

#if defined(ENGINE_BUILDING_VIEW_MODULE)
    #define ENGINE_VIEW_API ENGINE_EXPORT
#else
    #define ENGINE_VIEW_API ENGINE_IMPORT
#endif