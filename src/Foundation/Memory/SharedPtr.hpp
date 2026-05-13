#ifndef FOUNDATION_MEMORY_SHARED_PTR_HDR
#define FOUNDATION_MEMORY_SHARED_PTR_HDR

#include <Foundation/Platform.hpp>
#include <Utility/Assert.hpp>
#include <Utility/Macros.hpp>
#include <Threading/Atomic.hpp>
#include <Foundation/Memory/Allocator.hpp>

// Forward declare IAllocator to avoid circular dependency
namespace Engine::Memory { class IAllocator; }

namespace Engine {

// Forward declarations
template<typename T> class SharedPtr;
template<typename T> class WeakPtr;
template<typename T, typename Deleter> class UniquePtr;

// ============================================================================
// Control Block (Detail)
// ============================================================================

namespace Detail {

/// @brief Base control block with atomic reference counts
struct ControlBlockBase {
    Threading::Atomic<int32> StrongCount{1};
    Threading::Atomic<int32> WeakCount{1};  // +1 for strong refs existing
    Memory::IAllocator* Alloc_ = nullptr;   // nullptr = global heap

    virtual ~ControlBlockBase() = default;
    virtual void DestroyObject() noexcept = 0;
    virtual void DeleteThis() noexcept = 0;

    void AddStrong() noexcept {
        (void)StrongCount.FetchAdd(1, Threading::MemoryOrder::Relaxed);
    }

    void AddWeak() noexcept {
        (void)WeakCount.FetchAdd(1, Threading::MemoryOrder::Relaxed);
    }

    void ReleaseStrong() noexcept {
        if (StrongCount.FetchSub(1, Threading::MemoryOrder::AcqRel) == 1) {
            DestroyObject();
            ReleaseWeak();
        }
    }

    void ReleaseWeak() noexcept {
        if (WeakCount.FetchSub(1, Threading::MemoryOrder::AcqRel) == 1) {
            DeleteThis();
        }
    }

    int32 UseCount() const noexcept {
        return StrongCount.Load(Threading::MemoryOrder::Relaxed);
    }

    bool Expired() const noexcept {
        return StrongCount.Load(Threading::MemoryOrder::Acquire) == 0;
    }

    /// @brief Try to increment strong count if not zero (for WeakPtr::Lock)
    bool TryAddStrong() noexcept {
        int32 Count = StrongCount.Load(Threading::MemoryOrder::Relaxed);
        while (Count > 0) {
            if (StrongCount.CompareExchangeWeak(Count, Count + 1,
                    Threading::MemoryOrder::AcqRel)) {
                return true;
            }
        }
        return false;
    }
};

/// @brief Control block with separate pointer + deleter
template<typename T, typename Deleter>
struct ControlBlockPtr final : ControlBlockBase {
    T* Ptr;
    [[no_unique_address]] Deleter Del;

    ControlBlockPtr(T* P, Deleter D, Memory::IAllocator* A = nullptr) noexcept
        : Ptr(P), Del(static_cast<Deleter&&>(D)) { Alloc_ = A; }

    void DestroyObject() noexcept override {
        if (Ptr) Del(Ptr);
        Ptr = nullptr;
    }

    void DeleteThis() noexcept override {
        if (Alloc_) { this->~ControlBlockPtr(); Alloc_->Free(this); }
        else delete this;
    }
};

/// @brief Control block with embedded object (MakeShared - single allocation)
template<typename T>
struct ControlBlockInplace final : ControlBlockBase {
    alignas(T) unsigned char Storage[sizeof(T)];

    template<typename... Args>
    explicit ControlBlockInplace(Memory::IAllocator* A, Args&&... Arguments) {
        Alloc_ = A;
        ::new (static_cast<void*>(Storage)) T(static_cast<Args&&>(Arguments)...);
    }

    T* GetPtr() noexcept { return reinterpret_cast<T*>(Storage); }

    void DestroyObject() noexcept override {
        GetPtr()->~T();
    }

    void DeleteThis() noexcept override {
        if (Alloc_) { this->~ControlBlockInplace(); Alloc_->Free(this); }
        else delete this;
    }
};

} // namespace Detail

// ============================================================================
// SharedPtr
// ============================================================================

/// @brief Thread-safe reference-counted smart pointer
template<typename T>
class SharedPtr {
public:
    using ElementType = T;

private:
    template<typename U> friend class SharedPtr;
    template<typename U> friend class WeakPtr;

    T* Ptr_ = nullptr;
    Detail::ControlBlockBase* Ctrl_ = nullptr;

    void AddRef() noexcept {
        if (Ctrl_) Ctrl_->AddStrong();
    }

    void ReleaseRef() noexcept {
        if (Ctrl_) Ctrl_->ReleaseStrong();
    }

    // Private constructor for MakeShared
    template<typename U, typename... Args>
    friend SharedPtr<U> MakeShared(Args&&...);
    template<typename U, typename... Args>
    friend SharedPtr<U> AllocateShared(Memory::IAllocator& Alloc, Args&&...);
    template<typename U>
    friend SharedPtr<U> MakeSharedForOverwrite();

public:
    // --- Constructors ---

    constexpr SharedPtr() noexcept = default;
    constexpr SharedPtr(decltype(nullptr)) noexcept {}

    template<typename U>
        requires IsConvertible<U*, T*>
    explicit SharedPtr(U* P, Memory::IAllocator* Alloc = nullptr) {
        struct DefaultDel {
            Memory::IAllocator* A_ = nullptr;
            void operator()(U* Ptr) const noexcept {
                if (!Ptr) {
                    return;
                }
                if (A_) {
                    Ptr->~U();
                    A_->Free(Ptr);
                    return;
                }
                delete Ptr;
            }
        };
        if (Alloc) {
            void* Mem = Alloc->Allocate(sizeof(Detail::ControlBlockPtr<U, DefaultDel>),
                                        alignof(Detail::ControlBlockPtr<U, DefaultDel>));
            Ctrl_ = new (Mem) Detail::ControlBlockPtr<U, DefaultDel>(P, DefaultDel{Alloc}, Alloc);
        } else {
            Ctrl_ = new Detail::ControlBlockPtr<U, DefaultDel>(P, DefaultDel{});
        }
        Ptr_ = P;
    }

    template<typename U, typename D>
        requires IsConvertible<U*, T*>
    explicit SharedPtr(UniquePtr<U, D>&& Other) {
        U* P = Other.Release();
        if (!P) {
            return;
        }
        Ctrl_ = new Detail::ControlBlockPtr<U, D>(
            P, static_cast<D&&>(Other.GetDeleter()));
        Ptr_ = P;
    }

    template<typename U, typename D>
        requires IsArray<U> && IsConvertible<RemoveExtent<U>*, T*>
    explicit SharedPtr(UniquePtr<U, D>&& Other) {
        using Elem = RemoveExtent<U>;
        Elem* P = Other.Release();
        if (!P) {
            return;
        }
        Ctrl_ = new Detail::ControlBlockPtr<Elem, D>(
            P, static_cast<D&&>(Other.GetDeleter()));
        Ptr_ = P;
    }

    template<typename U, typename D>
        requires IsConvertible<U*, T*> && (!IsConvertible<D, Memory::IAllocator*>)
    SharedPtr(U* P, D Del, Memory::IAllocator* Alloc = nullptr) {
        if (Alloc) {
            void* Mem = Alloc->Allocate(sizeof(Detail::ControlBlockPtr<U, D>),
                                        alignof(Detail::ControlBlockPtr<U, D>));
            Ctrl_ = new (Mem) Detail::ControlBlockPtr<U, D>(P, static_cast<D&&>(Del), Alloc);
        } else {
            Ctrl_ = new Detail::ControlBlockPtr<U, D>(P, static_cast<D&&>(Del));
        }
        Ptr_ = P;
    }

    // Aliasing constructor - shares ownership with Other, points to P
    template<typename U>
    SharedPtr(const SharedPtr<U>& Other, T* P) noexcept
        : Ptr_(P), Ctrl_(Other.Ctrl_) {
        AddRef();
    }

    // Copy
    SharedPtr(const SharedPtr& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        AddRef();
    }

    template<typename U>
        requires IsConvertible<U*, T*>
    SharedPtr(const SharedPtr<U>& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        AddRef();
    }

    // Move
    SharedPtr(SharedPtr&& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        Other.Ptr_ = nullptr;
        Other.Ctrl_ = nullptr;
    }

    template<typename U>
        requires IsConvertible<U*, T*>
    SharedPtr(SharedPtr<U>&& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        Other.Ptr_ = nullptr;
        Other.Ctrl_ = nullptr;
    }

    // --- Destructor ---

    ~SharedPtr() { ReleaseRef(); }

    // --- Assignment ---

    SharedPtr& operator=(const SharedPtr& Other) noexcept {
        SharedPtr(Other).Swap(*this);
        return *this;
    }

    template<typename U>
        requires IsConvertible<U*, T*>
    SharedPtr& operator=(const SharedPtr<U>& Other) noexcept {
        SharedPtr(Other).Swap(*this);
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& Other) noexcept {
        SharedPtr(static_cast<SharedPtr&&>(Other)).Swap(*this);
        return *this;
    }

    template<typename U>
        requires IsConvertible<U*, T*>
    SharedPtr& operator=(SharedPtr<U>&& Other) noexcept {
        SharedPtr(static_cast<SharedPtr<U>&&>(Other)).Swap(*this);
        return *this;
    }

    SharedPtr& operator=(decltype(nullptr)) noexcept {
        Reset();
        return *this;
    }

    // --- Observers ---

    [[nodiscard]] Memory::IAllocator* Allocator() const noexcept {
        return Ctrl_ ? Ctrl_->Alloc_ : nullptr;
    }

    ENGINE_FORCE_INLINE T* Get() const noexcept { return Ptr_; }

    ENGINE_FORCE_INLINE T& operator*() const noexcept {
        ENGINE_ASSERT(Ptr_ != nullptr);
        return *Ptr_;
    }

    ENGINE_FORCE_INLINE T* operator->() const noexcept {
        ENGINE_ASSERT(Ptr_ != nullptr);
        return Ptr_;
    }

    explicit operator bool() const noexcept { return Ptr_ != nullptr; }

    int32 UseCount() const noexcept {
        return Ctrl_ ? Ctrl_->UseCount() : 0;
    }

    bool IsUnique() const noexcept { return UseCount() == 1; }

    // --- Modifiers ---

    void Reset() noexcept { SharedPtr().Swap(*this); }

    template<typename U>
        requires IsConvertible<U*, T*>
    void Reset(U* P) { SharedPtr(P, Allocator()).Swap(*this); }

    void Swap(SharedPtr& Other) noexcept {
        auto TempPtr = Ptr_;
        auto TempCtrl = Ctrl_;
        Ptr_ = Other.Ptr_;
        Ctrl_ = Other.Ctrl_;
        Other.Ptr_ = TempPtr;
        Other.Ctrl_ = TempCtrl;
    }

    // --- Comparison ---

    bool operator==(const SharedPtr& Other) const noexcept { return Ptr_ == Other.Ptr_; }
    bool operator!=(const SharedPtr& Other) const noexcept { return Ptr_ != Other.Ptr_; }
    bool operator==(decltype(nullptr)) const noexcept { return Ptr_ == nullptr; }
    bool operator!=(decltype(nullptr)) const noexcept { return Ptr_ != nullptr; }
    bool operator<(const SharedPtr& Other) const noexcept { return Ptr_ < Other.Ptr_; }
};

// ============================================================================
// WeakPtr
// ============================================================================

/// @brief Non-owning observer that can be promoted to SharedPtr
template<typename T>
class WeakPtr {
public:
    using ElementType = T;

private:
    template<typename U> friend class WeakPtr;
    template<typename U> friend class SharedPtr;

    T* Ptr_ = nullptr;
    Detail::ControlBlockBase* Ctrl_ = nullptr;

    void AddRef() noexcept { if (Ctrl_) Ctrl_->AddWeak(); }
    void ReleaseRef() noexcept { if (Ctrl_) Ctrl_->ReleaseWeak(); }

public:
    constexpr WeakPtr() noexcept = default;

    WeakPtr(const SharedPtr<T>& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        AddRef();
    }

    template<typename U>
        requires IsConvertible<U*, T*>
    WeakPtr(const SharedPtr<U>& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        AddRef();
    }

    WeakPtr(const WeakPtr& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        AddRef();
    }

    WeakPtr(WeakPtr&& Other) noexcept
        : Ptr_(Other.Ptr_), Ctrl_(Other.Ctrl_) {
        Other.Ptr_ = nullptr;
        Other.Ctrl_ = nullptr;
    }

    ~WeakPtr() { ReleaseRef(); }

    WeakPtr& operator=(const WeakPtr& Other) noexcept {
        WeakPtr(Other).Swap(*this);
        return *this;
    }

    WeakPtr& operator=(const SharedPtr<T>& Other) noexcept {
        WeakPtr(Other).Swap(*this);
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& Other) noexcept {
        WeakPtr(static_cast<WeakPtr&&>(Other)).Swap(*this);
        return *this;
    }

    void Reset() noexcept { WeakPtr().Swap(*this); }

    void Swap(WeakPtr& Other) noexcept {
        auto TempPtr = Ptr_;
        auto TempCtrl = Ctrl_;
        Ptr_ = Other.Ptr_;
        Ctrl_ = Other.Ctrl_;
        Other.Ptr_ = TempPtr;
        Other.Ctrl_ = TempCtrl;
    }

    int32 UseCount() const noexcept { return Ctrl_ ? Ctrl_->UseCount() : 0; }
    bool Expired() const noexcept { return !Ctrl_ || Ctrl_->Expired(); }

    /// @brief Promote to SharedPtr (returns empty if expired)
    SharedPtr<T> Lock() const noexcept {
        SharedPtr<T> Result;
        if (Ctrl_ && Ctrl_->TryAddStrong()) {
            Result.Ptr_ = Ptr_;
            Result.Ctrl_ = Ctrl_;
        }
        return Result;
    }
};

// ============================================================================
// MakeShared - Single-Allocation Construction
// ============================================================================

/// @brief Construct T with single allocation (object + control block together)
template<typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... Arguments) {
    auto* Ctrl = new Detail::ControlBlockInplace<T>(nullptr, static_cast<Args&&>(Arguments)...);
    SharedPtr<T> Result;
    Result.Ptr_ = Ctrl->GetPtr();
    Result.Ctrl_ = Ctrl;
    return Result;
}

/// @brief Construct T with single allocation using a custom allocator
template<typename T, typename... Args>
SharedPtr<T> AllocateShared(Memory::IAllocator& Alloc, Args&&... Arguments) {
    using CB = Detail::ControlBlockInplace<T>;
    void* Mem = Alloc.Allocate(sizeof(CB), alignof(CB));
    auto* Ctrl = new (Mem) CB(&Alloc, static_cast<Args&&>(Arguments)...);
    SharedPtr<T> Result;
    Result.Ptr_ = Ctrl->GetPtr();
    Result.Ctrl_ = Ctrl;
    return Result;
}

/// @brief Construct T with default-initialization (no zero-fill), single allocation
template<typename T>
SharedPtr<T> MakeSharedForOverwrite() {
    auto* Ctrl = new Detail::ControlBlockInplace<T>(nullptr);
    SharedPtr<T> Result;
    Result.Ptr_ = Ctrl->GetPtr();
    Result.Ctrl_ = Ctrl;
    return Result;
}

// ============================================================================
// Pointer Casts
// ============================================================================

template<typename T, typename U>
SharedPtr<T> StaticPointerCast(const SharedPtr<U>& Ptr) noexcept {
    auto* p = static_cast<T*>(Ptr.Get());
    return SharedPtr<T>(Ptr, p);
}

template<typename T, typename U>
SharedPtr<T> DynamicPointerCast(const SharedPtr<U>& Ptr) noexcept {
    auto* p = dynamic_cast<T*>(Ptr.Get());
    if (p) return SharedPtr<T>(Ptr, p);
    return SharedPtr<T>{};
}

template<typename T, typename U>
SharedPtr<T> ConstPointerCast(const SharedPtr<U>& Ptr) noexcept {
    auto* p = const_cast<T*>(Ptr.Get());
    return SharedPtr<T>(Ptr, p);
}

} // namespace Engine

#endif // FOUNDATION_MEMORY_SHARED_PTR_HDR