#ifndef FOUNDATION_MEMORY_REF_HDR
#define FOUNDATION_MEMORY_REF_HDR

#include <Foundation/Platform.hpp>
#include <Utility/Move.hpp>
#include <Memory/SharedPtr.hpp>
#include <Foundation/Memory/UniquePtr.hpp>

namespace Engine::Memory {

template<typename T>
using Ref = T*;

template<typename T>
using RefPtr = Ref<T>;

template<typename T>
using WeakRef = T*;

template<typename To, typename From>
[[nodiscard]] constexpr Ref<To> StaticRefCast(Ref<From> from) noexcept {
    return static_cast<To*>(from);
}

template<typename To, typename From>
[[nodiscard]] constexpr Ref<To> DynamicRefCast(Ref<From> from) noexcept {
    return dynamic_cast<To*>(from);
}

template<typename T>
[[nodiscard]] constexpr Ref<T> ConstRefCast(Ref<const T> from) noexcept {
    return const_cast<T*>(from);
}

template<typename To, typename From>
[[nodiscard]] constexpr Ref<To> ReinterpretRefCast(Ref<From> from) noexcept {
    return reinterpret_cast<To*>(from);
}

template<typename T, typename... Args>
[[nodiscard]] Ref<T> MakeRef(Args&&... args) {
    return MakeUnique<T>(Forward<Args>(args)...).Release();
}

template<typename T>
[[nodiscard]] constexpr Ref<T> LockWeakRef(WeakRef<T> weak) noexcept {
    return weak;
}

template<typename T>
[[nodiscard]] constexpr bool IsExpired(WeakRef<T> weak) noexcept {
    return weak == nullptr;
}

template<typename T>
struct IsSharedPtr {
    static constexpr bool Value = false;
    static constexpr bool value = false;
};

template<typename T>
struct IsSharedPtr<SharedPtr<T>> {
    static constexpr bool Value = true;
    static constexpr bool value = true;
};

template<typename T>
inline constexpr bool IsSharedPtrV = IsSharedPtr<T>::Value;

template<typename T>
struct IsWeakPtr {
    static constexpr bool Value = false;
    static constexpr bool value = false;
};

template<typename T>
struct IsWeakPtr<WeakPtr<T>> {
    static constexpr bool Value = true;
    static constexpr bool value = true;
};

template<typename T>
inline constexpr bool IsWeakPtrV = IsWeakPtr<T>::Value;

template<typename T>
struct IsUniquePtr {
    static constexpr bool Value = false;
    static constexpr bool value = false;
};

template<typename T, typename Deleter>
struct IsUniquePtr<UniquePtr<T, Deleter>> {
    static constexpr bool Value = true;
    static constexpr bool value = true;
};

template<typename T>
inline constexpr bool IsUniquePtrV = IsUniquePtr<T>::Value;

template<typename T>
struct IsRef {
    static constexpr bool Value = false;
    static constexpr bool value = false;
};

template<typename T>
struct IsRef<T*> {
    static constexpr bool Value = true;
    static constexpr bool value = true;
};

template<typename T>
inline constexpr bool IsRefV = IsRef<T>::Value;

template<typename T>
inline constexpr bool IsSmartPtrV = IsSharedPtrV<T> || IsWeakPtrV<T> || IsUniquePtrV<T>;

template<typename T>
struct PointerElementType {
    using Type = void;
};

template<typename T>
struct PointerElementType<SharedPtr<T>> {
    using Type = T;
};

template<typename T>
struct PointerElementType<WeakPtr<T>> {
    using Type = T;
};

template<typename T, typename Deleter>
struct PointerElementType<UniquePtr<T, Deleter>> {
    using Type = T;
};

template<typename T>
struct PointerElementType<T*> {
    using Type = T;
};

template<typename T>
using PointerElementTypeT = typename PointerElementType<T>::Type;

template<typename T>
void swap(SharedPtr<T>& lhs, SharedPtr<T>& rhs) noexcept {
    lhs.Swap(rhs);
}

template<typename T>
void swap(WeakPtr<T>& lhs, WeakPtr<T>& rhs) noexcept {
    lhs.Swap(rhs);
}

template<typename T, typename Deleter>
void swap(UniquePtr<T, Deleter>& lhs, UniquePtr<T, Deleter>& rhs) noexcept {
    lhs.Swap(rhs);
}

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_REF_HDR