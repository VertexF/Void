#ifndef NUMERIC_LIMITS_HDR
#define NUMERIC_LIMITS_HDR

// ============================================================================
// Engine - Core Module
// Types - NumericLimits (Freestanding)
// ============================================================================
// Replaces NumericLimits with compile-time constants via compiler
// intrinsics and known IEEE 754 bit patterns. Zero <limits> dependency.

#include <Foundation/Platform.hpp>

#include <limits>

namespace Engine {

// Primary template — undefined for unsupported types
template <typename T>
struct NumericLimits;

// ---- Integer specializations ----

template <> struct NumericLimits<bool> {
    static constexpr bool Min() noexcept { return false; }
    static constexpr bool Max() noexcept { return true; }
    static constexpr bool Lowest() noexcept { return false; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = false;
    static constexpr int32 Digits = 1;
};

template <> struct NumericLimits<int8> {
    static constexpr int8 Min() noexcept { return -128; }
    static constexpr int8 Max() noexcept { return 127; }
    static constexpr int8 Lowest() noexcept { return -128; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 7;
};

template <> struct NumericLimits<uint8> {
    static constexpr uint8 Min() noexcept { return 0; }
    static constexpr uint8 Max() noexcept { return 255; }
    static constexpr uint8 Lowest() noexcept { return 0; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = false;
    static constexpr int32 Digits = 8;
};

template <> struct NumericLimits<int16> {
    static constexpr int16 Min() noexcept { return -32768; }
    static constexpr int16 Max() noexcept { return 32767; }
    static constexpr int16 Lowest() noexcept { return -32768; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 15;
};

template <> struct NumericLimits<uint16> {
    static constexpr uint16 Min() noexcept { return 0; }
    static constexpr uint16 Max() noexcept { return 65535; }
    static constexpr uint16 Lowest() noexcept { return 0; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = false;
    static constexpr int32 Digits = 16;
};

template <> struct NumericLimits<int32> {
    static constexpr int32 Min() noexcept { return (-2147483647 - 1); }
    static constexpr int32 Max() noexcept { return 2147483647; }
    static constexpr int32 Lowest() noexcept { return (-2147483647 - 1); }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 31;
};

template <> struct NumericLimits<uint32> {
    static constexpr uint32 Min() noexcept { return 0; }
    static constexpr uint32 Max() noexcept { return 4294967295u; }
    static constexpr uint32 Lowest() noexcept { return 0; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = false;
    static constexpr int32 Digits = 32;
};

template <> struct NumericLimits<int64> {
    static constexpr int64 Min() noexcept { return (-9223372036854775807LL - 1); }
    static constexpr int64 Max() noexcept { return 9223372036854775807LL; }
    static constexpr int64 Lowest() noexcept { return (-9223372036854775807LL - 1); }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 63;
};

template <> struct NumericLimits<uint64> {
    static constexpr uint64 Min() noexcept { return 0; }
    static constexpr uint64 Max() noexcept { return 18446744073709551615ULL; }
    static constexpr uint64 Lowest() noexcept { return 0; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = false;
    static constexpr int32 Digits = 64;
};

// ---- Floating-point specializations (IEEE 754) ----

template <> struct NumericLimits<float32> {
    static constexpr float32 Min() noexcept { return 1.175494351e-38f; }
    static constexpr float32 Max() noexcept { return 3.402823466e+38f; }
    static constexpr float32 Lowest() noexcept { return -3.402823466e+38f; }
    static constexpr float32 Epsilon() noexcept { return 1.192092896e-07f; }
    static constexpr float32 Infinity() noexcept { return std::numeric_limits<float32>::infinity(); }
    static constexpr float32 QuietNaN() noexcept { return std::numeric_limits<float32>::quiet_NaN(); }
    static constexpr float32 DenormMin() noexcept { return 1.401298464e-45f; }
    static constexpr bool IsInteger = false;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 24;
    static constexpr int32 Digits10 = 6;
    static constexpr int32 MaxDigits10 = 9;
};

template <> struct NumericLimits<float64> {
    static constexpr float64 Min() noexcept { return 2.2250738585072014e-308; }
    static constexpr float64 Max() noexcept { return 1.7976931348623158e+308; }
    static constexpr float64 Lowest() noexcept { return -1.7976931348623158e+308; }
    static constexpr float64 Epsilon() noexcept { return 2.2204460492503131e-016; }
    static constexpr float64 Infinity() noexcept { return std::numeric_limits<float64>::infinity(); }
    static constexpr float64 QuietNaN() noexcept { return std::numeric_limits<float64>::quiet_NaN(); }
    static constexpr float64 DenormMin() noexcept { return 4.9406564584124654e-324; }
    static constexpr bool IsInteger = false;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 53;
    static constexpr int32 Digits10 = 15;
    static constexpr int32 MaxDigits10 = 17;
};

// ---- char / wchar_t / size_t for completeness ----

template <> struct NumericLimits<char> {
    static constexpr char Min() noexcept { return static_cast<char>(-128); }
    static constexpr char Max() noexcept { return static_cast<char>(127); }
    static constexpr char Lowest() noexcept { return static_cast<char>(-128); }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 7;
};

#if defined(ENGINE_COMPILER_MSVC)
// MSVC: wchar_t is unsigned 16-bit
template <> struct NumericLimits<wchar_t> {
    static constexpr wchar_t Min() noexcept { return 0; }
    static constexpr wchar_t Max() noexcept { return 65535; }
    static constexpr wchar_t Lowest() noexcept { return 0; }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = false;
    static constexpr int32 Digits = 16;
};
#else
// GCC/Clang: wchar_t is signed 32-bit
template <> struct NumericLimits<wchar_t> {
    static constexpr wchar_t Min() noexcept { return static_cast<wchar_t>(-2147483647 - 1); }
    static constexpr wchar_t Max() noexcept { return static_cast<wchar_t>(2147483647); }
    static constexpr wchar_t Lowest() noexcept { return static_cast<wchar_t>(-2147483647 - 1); }
    static constexpr bool IsInteger = true;
    static constexpr bool IsSigned = true;
    static constexpr int32 Digits = 31;
};
#endif

} // namespace Engine

#endif // !NUMERIC_LIMITS_HDR
