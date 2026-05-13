#ifndef FOUNDATION_MEMORY_SIMD_HDR
#define FOUNDATION_MEMORY_SIMD_HDR

// ============================================================================
// SIMD-Optimized Memory Fill & Copy (ADR-321)
// ============================================================================
// Size-based dispatch for Fill operations:
//   < 64 bytes:   scalar loop (SIMD overhead not worth it)
//   64B - 256KB:  AVX2 aligned stores (through cache)
//   >= 256KB:     AVX2 non-temporal stores (bypass cache, avoid pollution)
// ============================================================================
#include <Foundation/CompilerTraits.hpp>

#if defined(ENGINE_SIMD_AVX2)
#define ENGINE_HAS_AVX2 1
#if defined(ENGINE_COMPILER_MSVC)
#include <Foundation/Memory/Operations.hpp>
#include <algorithm>
#endif
#else
#define ENGINE_HAS_AVX2 0
#endif

namespace Engine::Memory::Detail {

// Size-based dispatch thresholds (empirically tuned, MSVC 19.50 / Intel):
//   < kSimdMinBytes:     scalar (SIMD setup overhead not worth it)
//   kSimdMinBytes-kErms: AVX2 aligned stores (3-5x faster than rep stosd at these sizes)
//   kErms-kNT:           Fill / rep stosd ERMS fast-path (L3-resident, hardware-optimized)
//   >= kNT:              AVX2 non-temporal stores (bypass cache hierarchy entirely)
//
// The ERMS tier exists because Intel's rep stosd microcode uses a store-buffer fast-path
// for L3-resident data that AVX2 cached stores can't match (measured 1.37x slower at 1MB).
inline constexpr usize kSimdMinBytes = 64;
inline constexpr usize kErmsThreshold = 16 * 1024;         // 16KB: ERMS wins above here
inline constexpr usize kNonTemporalThreshold = 4 * 1024 * 1024; // 4MB: NT stores win above here

#if ENGINE_HAS_AVX2

inline bool HasAvx2RuntimeSupport() noexcept {
#if defined(ENGINE_COMPILER_MSVC) && (defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86))
    int cpuInfo[4] = {0, 0, 0, 0};
    __cpuidex(cpuInfo, 0, 0);
    if (cpuInfo[0] < 7) {
        return false;
    }

    __cpuidex(cpuInfo, 1, 0);
    const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    const bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) {
        return false;
    }

    const unsigned long long xcr0 = _xgetbv(0);
    if ((xcr0 & 0x6) != 0x6) {
        return false;
    }

    __cpuidex(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 5)) != 0;
#elif (defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)) && (defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86))
    return __builtin_cpu_supports("avx2");
#else
    return true;
#endif
}

// ============================================================================
// 4-byte (int/float) fill - AVX2
// ============================================================================
inline void SimdFill4(void* dest, uint32 pattern, usize count) noexcept {
    const usize totalBytes = count * 4;

    // Small: scalar
    if (totalBytes < kSimdMinBytes) {
        auto* p = static_cast<uint32*>(dest);
        for (usize i = 0; i < count; ++i) p[i] = pattern;
        return;
    }

    // Medium (16KB-4MB): rep stosd via Fill - ERMS hardware fast-path
    if (totalBytes >= kErmsThreshold && totalBytes < kNonTemporalThreshold) {
        auto* p = static_cast<uint32*>(dest);
        int val;
        MemCopy(&val, &pattern, 4);
        Fill(p, p + count, val);
        return;
    }

    auto* dst = static_cast<uint8*>(dest);
    const __m256i val = _mm256_set1_epi32(static_cast<int>(pattern));

    // Align to 32-byte boundary
    const uintptr addr = reinterpret_cast<uintptr>(dst);
    const usize misalign = addr & 31;
    usize offset = 0;

    if (misalign != 0) {
        const usize headBytes = 32 - misalign;
        const usize headCount = headBytes / 4;
        auto* p = static_cast<uint32*>(dest);
        for (usize i = 0; i < headCount && i < count; ++i) p[i] = pattern;
        offset = headCount * 4;
    }

    const usize endOffset = totalBytes - (totalBytes & 31);

    if (totalBytes >= kNonTemporalThreshold) {
        // Non-temporal stores - bypass cache for huge fills
        for (; offset + 128 <= endOffset; offset += 128) {
            auto* p = reinterpret_cast<__m256i*>(dst + offset);
            _mm256_stream_si256(p, val);
            _mm256_stream_si256(p + 1, val);
            _mm256_stream_si256(p + 2, val);
            _mm256_stream_si256(p + 3, val);
        }
        for (; offset + 32 <= endOffset; offset += 32) {
            _mm256_stream_si256(reinterpret_cast<__m256i*>(dst + offset), val);
        }
        _mm_sfence();
    } else {
        // Small-medium (64B-16KB): AVX2 cached stores
        for (; offset + 128 <= endOffset; offset += 128) {
            auto* p = reinterpret_cast<__m256i*>(dst + offset);
            _mm256_store_si256(p, val);
            _mm256_store_si256(p + 1, val);
            _mm256_store_si256(p + 2, val);
            _mm256_store_si256(p + 3, val);
        }
        for (; offset + 32 <= endOffset; offset += 32) {
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst + offset), val);
        }
    }

    // Tail
    auto* tail = reinterpret_cast<uint32*>(dst + offset);
    const usize remaining = (totalBytes - offset) / 4;
    for (usize i = 0; i < remaining; ++i) tail[i] = pattern;
}

// ============================================================================
// 8-byte (int64/double/pointer) fill - AVX2
// ============================================================================
inline void SimdFill8(void* dest, uint64 pattern, usize count) noexcept {
    const usize totalBytes = count * 8;

    if (totalBytes < kSimdMinBytes) {
        auto* p = static_cast<uint64*>(dest);
        for (usize i = 0; i < count; ++i) p[i] = pattern;
        return;
    }

    // Medium (16KB-4MB): rep stosq via Fill - ERMS hardware fast-path
    if (totalBytes >= kErmsThreshold && totalBytes < kNonTemporalThreshold) {
        auto* p = static_cast<uint64*>(dest);
        uint64 val = pattern;
        Fill(p, p + count, val);
        return;
    }

    auto* dst = static_cast<uint8*>(dest);
    const __m256i val = _mm256_set1_epi64x(static_cast<long long>(pattern));

    const uintptr addr = reinterpret_cast<uintptr>(dst);
    const usize misalign = addr & 31;
    usize offset = 0;

    if (misalign != 0) {
        const usize headBytes = 32 - misalign;
        const usize headCount = headBytes / 8;
        auto* p = static_cast<uint64*>(dest);
        for (usize i = 0; i < headCount && i < count; ++i) p[i] = pattern;
        offset = headCount * 8;
    }

    const usize endOffset = totalBytes - (totalBytes & 31);

    if (totalBytes >= kNonTemporalThreshold) {
        for (; offset + 128 <= endOffset; offset += 128) {
            auto* p = reinterpret_cast<__m256i*>(dst + offset);
            _mm256_stream_si256(p, val);
            _mm256_stream_si256(p + 1, val);
            _mm256_stream_si256(p + 2, val);
            _mm256_stream_si256(p + 3, val);
        }
        for (; offset + 32 <= endOffset; offset += 32) {
            _mm256_stream_si256(reinterpret_cast<__m256i*>(dst + offset), val);
        }
        _mm_sfence();
    } else {
        for (; offset + 128 <= endOffset; offset += 128) {
            auto* p = reinterpret_cast<__m256i*>(dst + offset);
            _mm256_store_si256(p, val);
            _mm256_store_si256(p + 1, val);
            _mm256_store_si256(p + 2, val);
            _mm256_store_si256(p + 3, val);
        }
        for (; offset + 32 <= endOffset; offset += 32) {
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst + offset), val);
        }
    }

    auto* tail = reinterpret_cast<uint64*>(dst + offset);
    const usize remaining = (totalBytes - offset) / 8;
    for (usize i = 0; i < remaining; ++i) tail[i] = pattern;
}

// ============================================================================
// 1-byte fill - just MemSet (already optimal)
// ============================================================================
inline void SimdFill1(void* dest, uint8 pattern, usize count) noexcept {
    MemSet(dest, pattern, count);
}

// ============================================================================
// 2-byte (short/char16_t) fill - AVX2
// ============================================================================
inline void SimdFill2(void* dest, uint16 pattern, usize count) noexcept {
    const usize totalBytes = count * 2;

    if (totalBytes < kSimdMinBytes) {
        auto* p = static_cast<uint16*>(dest);
        for (usize i = 0; i < count; ++i) p[i] = pattern;
        return;
    }

    // Medium (16KB-4MB): Fill - ERMS hardware fast-path
    if (totalBytes >= kErmsThreshold && totalBytes < kNonTemporalThreshold) {
        auto* p = static_cast<uint16*>(dest);
        Fill(p, p + count, pattern);
        return;
    }

    auto* dst = static_cast<uint8*>(dest);
    const __m256i val = _mm256_set1_epi16(static_cast<short>(pattern));

    const uintptr addr = reinterpret_cast<uintptr>(dst);
    const usize misalign = addr & 31;
    usize offset = 0;

    if (misalign != 0) {
        const usize headBytes = 32 - misalign;
        const usize headCount = headBytes / 2;
        auto* p = static_cast<uint16*>(dest);
        for (usize i = 0; i < headCount && i < count; ++i) p[i] = pattern;
        offset = headCount * 2;
    }

    const usize endOffset = totalBytes - (totalBytes & 31);

    if (totalBytes >= kNonTemporalThreshold) {
        for (; offset + 128 <= endOffset; offset += 128) {
            auto* p = reinterpret_cast<__m256i*>(dst + offset);
            _mm256_stream_si256(p, val);
            _mm256_stream_si256(p + 1, val);
            _mm256_stream_si256(p + 2, val);
            _mm256_stream_si256(p + 3, val);
        }
        for (; offset + 32 <= endOffset; offset += 32) {
            _mm256_stream_si256(reinterpret_cast<__m256i*>(dst + offset), val);
        }
        _mm_sfence();
    } else {
        for (; offset + 128 <= endOffset; offset += 128) {
            auto* p = reinterpret_cast<__m256i*>(dst + offset);
            _mm256_store_si256(p, val);
            _mm256_store_si256(p + 1, val);
            _mm256_store_si256(p + 2, val);
            _mm256_store_si256(p + 3, val);
        }
        for (; offset + 32 <= endOffset; offset += 32) {
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst + offset), val);
        }
    }

    auto* tail = reinterpret_cast<uint16*>(dst + offset);
    const usize remaining = (totalBytes - offset) / 2;
    for (usize i = 0; i < remaining; ++i) tail[i] = pattern;
}

#endif // ENGINE_HAS_AVX2

// ============================================================================
// Dispatch: SimdFill<T> - picks the right lane width
// ============================================================================
template<typename T>
inline void SimdFill(T* dest, usize count, const T& value) noexcept {
#if ENGINE_HAS_AVX2
    static const bool kHasAvx2 = HasAvx2RuntimeSupport();
    if (!kHasAvx2) {
        for (usize i = 0; i < count; ++i) {
            dest[i] = value;
        }
        return;
    }

    if constexpr (sizeof(T) == 1) {
        uint8 pattern;
        MemCopy(&pattern, &value, 1);
        SimdFill1(dest, pattern, count);
    } else if constexpr (sizeof(T) == 2) {
        uint16 pattern;
        MemCopy(&pattern, &value, 2);
        SimdFill2(dest, pattern, count);
    } else if constexpr (sizeof(T) == 4) {
        uint32 pattern;
        MemCopy(&pattern, &value, 4);
        SimdFill4(dest, pattern, count);
    } else if constexpr (sizeof(T) == 8) {
        uint64 pattern;
        MemCopy(&pattern, &value, 8);
        SimdFill8(dest, pattern, count);
    } else {
        // Odd-sized trivial type: scalar fallback
        for (usize i = 0; i < count; ++i) dest[i] = value;
    }
#else
    // No AVX2: scalar fallback
    for (usize i = 0; i < count; ++i) dest[i] = value;
#endif
}

// ============================================================================
// SIMD-Optimized Copy (ADR-321)
// ============================================================================
// Non-overlapping copy optimized by size:
//   < 64 bytes:   scalar memcpy
//   64B - 256KB:  AVX2 with software prefetch (through cache)
//   >= 256KB:     Non-temporal streaming (bypass cache)
// ============================================================================
#if ENGINE_HAS_AVX2

inline void SimdCopyBytes(void* dest, const void* src, usize bytes) noexcept {
    auto* d = static_cast<uint8*>(dest);
    auto* s = static_cast<const uint8*>(src);

    // For general-purpose Copy: memcpy/ERMS is optimal at all sizes.
    // NT streaming only helps when destination won't be read again (GPU uploads).
    // Use StreamingCopy() explicitly for write-and-forget paths.
    if (bytes < 8 * 1024 * 1024) {
        MemCopy(d, s, bytes);
        return;
    }

    // Large (>= 8MB): Non-temporal streaming - bypasses destination cache allocation.
    // Regular loads + NT stores avoid RFO. NTA prefetch minimizes source pollution.
    const uintptr dAddr = reinterpret_cast<uintptr>(d);
    const usize misalign = dAddr & 31;
    usize offset = 0;

    if (misalign != 0) {
        const usize head = 32 - misalign;
        MemCopy(d, s, head < bytes ? head : bytes);
        offset = head < bytes ? head : bytes;
    }

    for (; offset + 128 <= bytes; offset += 128) {
        _mm_prefetch(reinterpret_cast<const char*>(s + offset + 512), _MM_HINT_NTA);
        _mm_prefetch(reinterpret_cast<const char*>(s + offset + 576), _MM_HINT_NTA);

        __m256i v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + offset));
        __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + offset + 32));
        __m256i v2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + offset + 64));
        __m256i v3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + offset + 96));
        _mm256_stream_si256(reinterpret_cast<__m256i*>(d + offset), v0);
        _mm256_stream_si256(reinterpret_cast<__m256i*>(d + offset + 32), v1);
        _mm256_stream_si256(reinterpret_cast<__m256i*>(d + offset + 64), v2);
        _mm256_stream_si256(reinterpret_cast<__m256i*>(d + offset + 96), v3);
    }
    _mm_sfence();

    if (offset < bytes) {
        MemCopy(d + offset, s + offset, bytes - offset);
    }
}

/// @brief Rep movsb copy via __movsb intrinsic (baseline comparison).
inline void RepMovsbCopy(void* dest, const void* src, usize bytes) noexcept {
#if defined(ENGINE_COMPILER_MSVC) && defined(ENGINE_ARCH_X64)
    __movsb(static_cast<unsigned char*>(dest),
            static_cast<const unsigned char*>(src), bytes);
#else
    MemCopy(dest, src, bytes);
#endif
}

#endif // ENGINE_HAS_AVX2

/// @brief Optimized byte copy - dispatches to SIMD for large buffers.
/// @pre src and dest must NOT overlap. For overlapping regions, use memmove.
inline void SimdCopy(void* dest, const void* src, usize bytes) noexcept {
#if ENGINE_HAS_AVX2
    static const bool kHasAvx2 = HasAvx2RuntimeSupport();
    if (kHasAvx2) {
        SimdCopyBytes(dest, src, bytes);
        return;
    }

    MemCopy(dest, src, bytes);
#else
    MemCopy(dest, src, bytes);
#endif
}

} // namespace Engine::Memory::Detail

#endif // !FOUNDATION_MEMORY_SIMD_HDR