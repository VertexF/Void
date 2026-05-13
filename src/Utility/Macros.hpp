#ifndef ENGINE_MACROS_HPP
#define ENGINE_MACROS_HPP

#include <Foundation/CompilerTraits.hpp>
#include <Foundation/Platform.hpp>

#include <type_traits>

namespace Engine {

template<typename From, typename To>
inline constexpr bool IsConvertible = std::is_convertible_v<From, To>;

template<typename T>
inline constexpr bool IsArray = std::is_array_v<T>;

template<typename T>
using RemoveExtent = std::remove_extent_t<T>;

template<typename T>
using UnderlyingType = std::underlying_type_t<T>;

} // namespace Engine

// ============================================================================
// Exception Policy
// ============================================================================

#ifndef ENGINE_ENABLE_EXCEPTIONS
    #define ENGINE_ENABLE_EXCEPTIONS 0
#endif

// ============================================================================
// DLL Export/Import Macros
// ============================================================================
#ifndef ENGINE_API
    #if defined(ENGINE_PLATFORM_WINDOWS)
        #if defined(ENGINE_BUILD_SHARED)
            #if defined(ENGINE_EXPORT)
                #define ENGINE_API __declspec(dllexport)
            #else
                #define ENGINE_API __declspec(dllimport)
            #endif
        #else
            #define ENGINE_API
        #endif
    #else
        #if defined(ENGINE_BUILD_SHARED) && defined(ENGINE_EXPORT)
            #define ENGINE_API __attribute__((visibility("default")))
        #else
            #define ENGINE_API
        #endif
    #endif
#endif

// ============================================================================
// Alignment
// ============================================================================
#define ENGINE_ALIGNAS(x) alignas(x)
#define ENGINE_ALIGNOF(x) alignof(x)

// Cache line size (typical)
#define ENGINE_CACHE_LINE_SIZE 64
// Align to cache line
#define ENGINE_CACHE_ALIGNED ENGINE_ALIGNAS(ENGINE_CACHE_LINE_SIZE)

// ============================================================================
// Debug Break (build-config guard wrapping CompilerTraits ENGINE_DEBUG_BREAK_IMPL)
// ============================================================================
#ifndef ENGINE_DEBUG_BREAK
    #if defined(ENGINE_BUILD_DEBUG)
        #define ENGINE_DEBUG_BREAK() __debugbreak()
    #else
        #define ENGINE_DEBUG_BREAK() do { } while(0)
    #endif
#endif

// ============================================================================
// Stringify
// ============================================================================
#define ENGINE_STRINGIFY_IMPL(x) #x
#define ENGINE_STRINGIFY(x) ENGINE_STRINGIFY_IMPL(x)

#define ENGINE_CONCAT_IMPL(a, b) a##b
#define ENGINE_CONCAT(a, b) ENGINE_CONCAT_IMPL(a, b)
// ============================================================================
// Unique Identifier Generation
// ============================================================================

#define ENGINE_UNIQUE_NAME(base) ENGINE_CONCAT(base, __LINE__)

// ============================================================================
// Unused Parameter
// ============================================================================

#define ENGINE_UNUSED(x) (void)(x)

// ============================================================================
// Array Size
// ============================================================================

template<typename T, Engine::usize N>
constexpr Engine::usize EngineArraySizeHelper(T (&)[N]) noexcept {
    return N;
}

#define ENGINE_ARRAY_SIZE(arr) EngineArraySizeHelper(arr)

// ============================================================================
// Offset Of
// ============================================================================

#define ENGINE_OFFSET_OF(type, member) offsetof(type, member)

// ============================================================================
// Delete Copy/Move
// ============================================================================

#define ENGINE_NON_COPYABLE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete

#define ENGINE_NON_MOVABLE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

#define ENGINE_NON_COPYABLE_NON_MOVABLE(ClassName) \
    ENGINE_NON_COPYABLE(ClassName); \
    ENGINE_NON_MOVABLE(ClassName)

// ============================================================================
// Default Copy/Move
// ============================================================================

#define ENGINE_DEFAULT_COPYABLE(ClassName) \
    ClassName(const ClassName&) = default; \
    ClassName& operator=(const ClassName&) = default

#define ENGINE_DEFAULT_MOVABLE(ClassName) \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator=(ClassName&&) noexcept = default

#define ENGINE_DEFAULT_COPYABLE_MOVABLE(ClassName) \
    ENGINE_DEFAULT_COPYABLE(ClassName); \
    ENGINE_DEFAULT_MOVABLE(ClassName)

// ============================================================================
// Bit Operations
// ============================================================================

#define ENGINE_BIT(n) (1ULL << (n))
#define ENGINE_BIT_SET(value, bit) ((value) |= ENGINE_BIT(bit))
#define ENGINE_BIT_CLEAR(value, bit) ((value) &= ~ENGINE_BIT(bit))
#define ENGINE_BIT_TOGGLE(value, bit) ((value) ^= ENGINE_BIT(bit))
#define ENGINE_BIT_CHECK(value, bit) (((value) & ENGINE_BIT(bit)) != 0)
// ============================================================================
// Enum Flags
// ============================================================================

#define ENGINE_ENUM_FLAGS(EnumType) \
    inline constexpr EnumType operator|(EnumType a, EnumType b) noexcept { \
        using T = UnderlyingType<EnumType>; \
        return static_cast<EnumType>(static_cast<T>(a) | static_cast<T>(b)); \
    } \
    inline constexpr EnumType operator&(EnumType a, EnumType b) noexcept { \
        using T = UnderlyingType<EnumType>; \
        return static_cast<EnumType>(static_cast<T>(a) & static_cast<T>(b)); \
    } \
    inline constexpr EnumType operator^(EnumType a, EnumType b) noexcept { \
        using T = UnderlyingType<EnumType>; \
        return static_cast<EnumType>(static_cast<T>(a) ^ static_cast<T>(b)); \
    } \
    inline constexpr EnumType operator~(EnumType a) noexcept { \
        using T = UnderlyingType<EnumType>; \
        return static_cast<EnumType>(~static_cast<T>(a)); \
    } \
    inline constexpr EnumType& operator|=(EnumType& a, EnumType b) noexcept { \
        return a = a | b; \
    } \
    inline constexpr EnumType& operator&=(EnumType& a, EnumType b) noexcept { \
        return a = a & b; \
    } \
    inline constexpr EnumType& operator^=(EnumType& a, EnumType b) noexcept { \
        return a = a ^ b; \
    } \
    inline constexpr bool HasFlag(EnumType value, EnumType flag) noexcept { \
        using T = UnderlyingType<EnumType>; \
        return (static_cast<T>(value) & static_cast<T>(flag)) == static_cast<T>(flag); \
    }

// ============================================================================
// Source Location
// ============================================================================

#define ENGINE_FILE __FILE__
#define ENGINE_LINE __LINE__

#endif // !ENGINE_MACROS_HPP
