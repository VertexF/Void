#ifndef FOUNDATION_MEMORY_UNIQUE_PTR_HDR
#define FOUNDATION_MEMORY_UNIQUE_PTR_HDR

// Design decisions:
// - [[no_unique_address]] for zero-overhead empty deleters
// - Compressed pair avoids wasting space
// - Array specialization with bounds-aware access
// - Forward-declared for use in headers without full definition
// - Deleter is a template parameter for custom allocator integration

#include <Foundation/Platform.hpp>
#include <Utility/Assert.hpp>
#include <Utility/Macros.hpp>
#include <Foundation/Memory/Allocator.hpp>

namespace Engine::Memory {

// ============================================================================
// Default Deleter
// ============================================================================

/// @brief Default deleter - calls destructor and deallocates
/// @details In freestanding builds, uses MAL::Free. In hosted builds,
///          falls back to operator delete. Override with custom deleters
///          for pool/arena allocations.
template<typename T>
struct DefaultDeleter {
    constexpr DefaultDeleter() noexcept = default;

    template<typename U>
        requires IsConvertible<U*, T*>
    constexpr DefaultDeleter(const DefaultDeleter<U>&) noexcept {}

    void operator()(T* Ptr) const noexcept {
        static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
        Ptr->~T();
        Memory::GetDefaultAllocator().Free(static_cast<void*>(Ptr));
    }
};

/// @brief Array deleter specialization
template<typename T>
struct DefaultDeleter<T[]> {
    constexpr DefaultDeleter() noexcept = default;

    void operator()(T* Ptr) const noexcept {
        static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
        Memory::GetDefaultAllocator().Free(static_cast<void*>(Ptr));
    }
};

/// @brief Deleter that routes through IAllocator
template<typename T>
struct AllocatorDeleter {
    Memory::IAllocator* Alloc_ = nullptr;
    constexpr AllocatorDeleter() noexcept = default;
    explicit constexpr AllocatorDeleter(Memory::IAllocator* A) noexcept : Alloc_(A) {}

    void operator()(T* Ptr) const noexcept {
        static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
        Ptr->~T();
        if (Alloc_) Alloc_->Free(static_cast<void*>(Ptr));
    }
};

// ============================================================================
// UniquePtr - Single Object
// ============================================================================

/// @brief Single-ownership smart pointer with zero overhead
/// @details Same size as raw pointer when using empty deleter (DefaultDeleter).
///          Move-only - no copies allowed. Deterministic destruction.
template<typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr {
public:
    using PointerType  = T*;
    using ElementType  = T;
    using DeleterType  = Deleter;

private:
    struct CompressedPair {
        PointerType Ptr;
        [[no_unique_address]] Deleter Del;

        constexpr CompressedPair() noexcept : Ptr(nullptr), Del() {}
        constexpr CompressedPair(PointerType P) noexcept : Ptr(P), Del() {}
        constexpr CompressedPair(PointerType P, Deleter D) noexcept
            : Ptr(P), Del(static_cast<Deleter&&>(D)) {}
    };

    CompressedPair Data_;

public:
    // --- Constructors ---

    constexpr UniquePtr() noexcept = default;
    constexpr UniquePtr(decltype(nullptr)) noexcept : Data_() {}
    constexpr explicit UniquePtr(PointerType P) noexcept : Data_(P) {}
    constexpr UniquePtr(PointerType P, Deleter D) noexcept
        : Data_(P, static_cast<Deleter&&>(D)) {}

    // Move
    UniquePtr(UniquePtr&& Other) noexcept
        : Data_(Other.Release(), static_cast<Deleter&&>(Other.GetDeleter())) {}

    // Converting move
    template<typename U, typename D>
        requires IsConvertible<U*, T*>
    UniquePtr(UniquePtr<U, D>&& Other) noexcept
        : Data_(Other.Release(), static_cast<D&&>(Other.GetDeleter())) {}

    // No copies
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // --- Destructor ---

    ~UniquePtr() {
        if (Data_.Ptr) Data_.Del(Data_.Ptr);
    }

    // --- Assignment ---

    UniquePtr& operator=(UniquePtr&& Other) noexcept {
        Reset(Other.Release());
        Data_.Del = static_cast<Deleter&&>(Other.Data_.Del);
        return *this;
    }

    template<typename U, typename D>
        requires IsConvertible<U*, T*>
    UniquePtr& operator=(UniquePtr<U, D>&& Other) noexcept {
        Reset(Other.Release());
        Data_.Del = static_cast<D&&>(Other.GetDeleter());
        return *this;
    }

    UniquePtr& operator=(decltype(nullptr)) noexcept {
        Reset();
        return *this;
    }

    // --- Observers ---

    ENGINE_FORCE_INLINE PointerType Get() const noexcept { return Data_.Ptr; }
    ENGINE_FORCE_INLINE Deleter& GetDeleter() noexcept { return Data_.Del; }
    ENGINE_FORCE_INLINE const Deleter& GetDeleter() const noexcept { return Data_.Del; }

    explicit operator bool() const noexcept { return Data_.Ptr != nullptr; }

    ENGINE_FORCE_INLINE T& operator*() const noexcept {
        ENGINE_ASSERT(Data_.Ptr != nullptr);
        return *Data_.Ptr;
    }

    ENGINE_FORCE_INLINE PointerType operator->() const noexcept {
        ENGINE_ASSERT(Data_.Ptr != nullptr);
        return Data_.Ptr;
    }

    // --- Modifiers ---

    PointerType Release() noexcept {
        PointerType P = Data_.Ptr;
        Data_.Ptr = nullptr;
        return P;
    }

    void Reset(PointerType P = nullptr) noexcept {
        PointerType Old = Data_.Ptr;
        Data_.Ptr = P;
        if (Old) Data_.Del(Old);
    }

    void Swap(UniquePtr& Other) noexcept {
        auto TempPtr = Data_.Ptr;
        Data_.Ptr = Other.Data_.Ptr;
        Other.Data_.Ptr = TempPtr;
    }

    // --- Comparison ---

    bool operator==(const UniquePtr& Other) const noexcept { return Data_.Ptr == Other.Data_.Ptr; }
    bool operator!=(const UniquePtr& Other) const noexcept { return Data_.Ptr != Other.Data_.Ptr; }
    bool operator==(decltype(nullptr)) const noexcept { return Data_.Ptr == nullptr; }
    bool operator!=(decltype(nullptr)) const noexcept { return Data_.Ptr != nullptr; }
    bool operator<(const UniquePtr& Other) const noexcept { return Data_.Ptr < Other.Data_.Ptr; }
};

// ============================================================================
// UniquePtr - Array Specialization
// ============================================================================

template<typename T, typename Deleter>
class UniquePtr<T[], Deleter> {
public:
    using PointerType = T*;
    using ElementType = T;
    using DeleterType = Deleter;

private:
    struct CompressedPair {
        PointerType Ptr;
        [[no_unique_address]] Deleter Del;
        constexpr CompressedPair() noexcept : Ptr(nullptr), Del() {}
        constexpr CompressedPair(PointerType P) noexcept : Ptr(P), Del() {}
        constexpr CompressedPair(PointerType P, Deleter D) noexcept
            : Ptr(P), Del(static_cast<Deleter&&>(D)) {}
    };

    CompressedPair Data_;

public:
    constexpr UniquePtr() noexcept = default;
    constexpr UniquePtr(decltype(nullptr)) noexcept : Data_() {}
    constexpr explicit UniquePtr(PointerType P) noexcept : Data_(P) {}

    UniquePtr(UniquePtr&& Other) noexcept
        : Data_(Other.Release(), static_cast<Deleter&&>(Other.GetDeleter())) {}

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    ~UniquePtr() { if (Data_.Ptr) Data_.Del(Data_.Ptr); }

    UniquePtr& operator=(UniquePtr&& Other) noexcept {
        Reset(Other.Release());
        Data_.Del = static_cast<Deleter&&>(Other.Data_.Del);
        return *this;
    }

    UniquePtr& operator=(decltype(nullptr)) noexcept { Reset(); return *this; }

    PointerType Get() const noexcept { return Data_.Ptr; }
    Deleter& GetDeleter() noexcept { return Data_.Del; }
    const Deleter& GetDeleter() const noexcept { return Data_.Del; }
    explicit operator bool() const noexcept { return Data_.Ptr != nullptr; }

    T& operator[](usize I) const noexcept {
        ENGINE_ASSERT(Data_.Ptr != nullptr);
        return Data_.Ptr[I];
    }

    PointerType Release() noexcept {
        PointerType P = Data_.Ptr;
        Data_.Ptr = nullptr;
        return P;
    }

    void Reset(PointerType P = nullptr) noexcept {
        PointerType Old = Data_.Ptr;
        Data_.Ptr = P;
        if (Old) Data_.Del(Old);
    }

    void Swap(UniquePtr& Other) noexcept {
        auto TempPtr = Data_.Ptr;
        Data_.Ptr = Other.Data_.Ptr;
        Other.Data_.Ptr = TempPtr;
    }
};

// ============================================================================
// MakeUnique
// ============================================================================

/// @brief Construct a T and wrap in UniquePtr
template<typename T, typename... Args>
    requires (!IsArray<T>)
UniquePtr<T> MakeUnique(Args&&... Arguments) {
    void* Mem = Memory::GetDefaultAllocator().Allocate(sizeof(T), alignof(T));
    return UniquePtr<T>(new (Mem) T(static_cast<Args&&>(Arguments)...));
}

/// @brief Construct a T using IAllocator and wrap in UniquePtr with AllocatorDeleter
template<typename T, typename... Args>
    requires (!IsArray<T>)
UniquePtr<T, AllocatorDeleter<T>> AllocateUnique(Memory::IAllocator& Alloc, Args&&... Arguments) {
    void* Mem = Alloc.Allocate(sizeof(T), alignof(T));
    T* Ptr = new (Mem) T(static_cast<Args&&>(Arguments)...);
    return UniquePtr<T, AllocatorDeleter<T>>(Ptr, AllocatorDeleter<T>(&Alloc));
}

/// @brief Construct an array of T and wrap in UniquePtr
template<typename T>
    requires IsArray<T>
UniquePtr<T> MakeUnique(usize Count) {
    using Elem = RemoveExtent<T>;
    void* Mem = Memory::GetDefaultAllocator().Allocate(sizeof(Elem) * Count, alignof(Elem));
    Elem* Arr = static_cast<Elem*>(Mem);
    for (usize I = 0; I < Count; ++I) new (Arr + I) Elem();
    return UniquePtr<T>(Arr);
}

/// @brief Construct a T with default-initialization (no zero-fill) and wrap in UniquePtr
template<typename T>
    requires (!IsArray<T>)
UniquePtr<T> MakeUniqueForOverwrite() {
    void* Mem = Memory::GetDefaultAllocator().Allocate(sizeof(T), alignof(T));
    return UniquePtr<T>(new (Mem) T);
}

/// @brief Construct an array of T with default-initialization and wrap in UniquePtr
template<typename T>
    requires IsArray<T>
UniquePtr<T> MakeUniqueForOverwrite(usize Count) {
    using Elem = RemoveExtent<T>;
    void* Mem = Memory::GetDefaultAllocator().Allocate(sizeof(Elem) * Count, alignof(Elem));
    Elem* Arr = static_cast<Elem*>(Mem);
    for (usize I = 0; I < Count; ++I) new (Arr + I) Elem;
    return UniquePtr<T>(Arr);
}

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_UNIQUE_PTR_HDR