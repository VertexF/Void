#ifndef FOUNDATION_MEMORY_SCOPE_HDR
#define FOUNDATION_MEMORY_SCOPE_HDR

#include <Foundation/Memory/UniquePtr.hpp>

#include <Foundation/Platform.hpp>
#include <Utility/Macros.hpp>

#include <Utility/Move.hpp>

namespace Engine::Memory {

// ============================================================================
// Scope<T> - Unique Ownership Smart Pointer
// ============================================================================

/// @brief Unique ownership smart Pointer (alias for UniquePtr)
/// @tparam T The managed type
/// @details Scope represents exclusive ownership of a dynamically allocated object.
///          When the Scope goes out of scope, the managed object is destroyed.
template<typename T>
using Scope = UniquePtr<T>;

// ============================================================================
// Factory Functions
// ============================================================================

/// @brief Create a Scope<T> with a new instance constructed with the given arguments
/// @tparam T The type to create
/// @tparam Args Constructor argument types
/// @param args Constructor arguments
/// @return A Scope<T> owning the new instance
template<typename T, typename... Args>
[[nodiscard]] constexpr Scope<T> MakeScope(Args&&... args) {
    return MakeUnique<T>(Forward<Args>(args)...);
}

/// @brief Create a Scope<T> from a raw Pointer (takes ownership)
/// @tparam T The type to manage
/// @param ptr The raw Pointer to take ownership of
/// @return A Scope<T> owning the Pointer
/// @warning The Scope takes ownership - do not delete the Pointer manually
template<typename T>
[[nodiscard]] constexpr Scope<T> AdoptScope(T* ptr) noexcept {
    return Scope<T>(ptr);
}

// ============================================================================
// Scope<T[]> - Unique Ownership Array Smart Pointer
// ============================================================================

/// @brief Create a Scope<T[]> for an array
/// @tparam T The element type
/// @param count The number of elements
/// @return A Scope<T[]> owning the array
template<typename T>
[[nodiscard]] Scope<T[]> MakeScopeArray(size_t count) {
    return MakeUnique<T[]>(count);
}

// ============================================================================
// Type Traits
// ============================================================================

/// @brief Check if a type is a Scope
template<typename T>
struct IsScope : FalseType {};

template<typename T>
struct IsScope<Scope<T>> : TrueType {};

template<typename T>
inline constexpr bool IsScopeV = IsScope<T>::value;

// ============================================================================
// Casting Utilities
// ============================================================================

/// @brief Static cast a Scope<From> to Scope<To>
/// @tparam To The target type
/// @tparam From The source type
/// @param from The source Scope
/// @return A new Scope<To> (source is released)
/// @note Performs static_cast - use for known safe conversions
template<typename To, typename From>
[[nodiscard]] Scope<To> StaticScopeCast(Scope<From>&& from) noexcept {
    return Scope<To>(static_cast<To*>(from.Release()));
}

/// @brief Dynamic cast a Scope<From> to Scope<To>
/// @tparam To The target type
/// @tparam From The source type
/// @param from The source Scope
/// @return A new Scope<To> if cast succeeds, nullptr otherwise
/// @note If cast fails, the original Scope retains ownership
template<typename To, typename From>
[[nodiscard]] Scope<To> DynamicScopeCast(Scope<From>&& from) noexcept {
    if (To* casted = dynamic_cast<To*>(from.Get())) {
        from.Release();
        return Scope<To>(casted);
    }
    return nullptr;
}

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_SCOPE_HDR