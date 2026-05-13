#ifndef MOVE_HDR
#define MOVE_HDR

#include <Utility/Macros.hpp>

namespace Engine {

// ============================================================================
// Type Trait Intrinsics (compiler built-in, no <type_traits> needed)
// ============================================================================

/// @brief Remove reference from type (uses compiler intrinsic).
/// @details __remove_reference is available on Clang, GCC 14+, MSVC 19.35+.
#if defined(ENGINE_COMPILER_CLANG) || (defined(ENGINE_COMPILER_GCC) && ENGINE_COMPILER_VERSION >= 140000)
    template<typename T>
    using RemoveReference = __remove_reference(T);
#elif defined(ENGINE_COMPILER_MSVC)
    // MSVC doesn't expose __remove_reference as a type trait intrinsic
    // in the same way; use manual specialization.
    template<typename T> struct RemoveRefImpl        { using Type = T; };
    template<typename T> struct RemoveRefImpl<T&>    { using Type = T; };
    template<typename T> struct RemoveRefImpl<T&&>   { using Type = T; };
    template<typename T>
    using RemoveReference = typename RemoveRefImpl<T>::Type;
#else
    template<typename T> struct RemoveRefImpl        { using Type = T; };
    template<typename T> struct RemoveRefImpl<T&>    { using Type = T; };
    template<typename T> struct RemoveRefImpl<T&&>   { using Type = T; };
    template<typename T>
    using RemoveReference = typename RemoveRefImpl<T>::Type;
#endif

// ============================================================================
// Core Utilities
// ============================================================================

/// @brief Cast to rvalue reference (equivalent to Move, no <utility>).
template<typename T>
[[nodiscard]] inline constexpr RemoveReference<T>&&
Move(T&& t) noexcept
{
    return static_cast<RemoveReference<T>&&>(t);
}

/// @brief Perfect forwarding (equivalent to Forward, no <utility>).
template<typename T>
[[nodiscard]] inline constexpr T&&
Forward(RemoveReference<T>& t) noexcept
{
    return static_cast<T&&>(t);
}

template<typename T>
[[nodiscard]] inline constexpr T&&
Forward(RemoveReference<T>&& t) noexcept
{
    return static_cast<T&&>(t);
}

/// @brief Swap two values (no <utility>).
template<typename T>
inline constexpr void Swap(T& a, T& b) noexcept
{
    T tmp = Move(a);
    a = Move(b);
    b = Move(tmp);
}

/// @brief Replace obj with new_value and return old value (no <utility>).
template<typename T, typename U = T>
[[nodiscard]] inline constexpr T Exchange(T& obj, U&& newValue) noexcept
{
    T old = Move(obj);
    obj = Forward<U>(newValue);
    return old;
}

/// @brief Get address of object, bypassing overloaded operator& (no <memory>).
template<typename T>
[[nodiscard]] inline constexpr T* AddressOf(T& v) noexcept
{
    return __builtin_addressof(v);
}

} // namespace Engine

#endif // !MOVE_HDR