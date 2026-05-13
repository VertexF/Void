#ifndef ANY_HDR
#define ANY_HDR

// ============================================================================
// Engine - Core Module
// Any — Type-Erased Value Container
// ============================================================================
// Small buffer optimization (32 bytes inline). No Any dependency.
// Move-only by default. Copy supported if stored type is copyable.
// Originated from ELT error_types.h — rebuilt for Engine conventions.

#include <Platform.hpp>
#include <Utility/Move.hpp>
#include <Utility/Assert.hpp>
#include <Memory/Allocator.hpp>

namespace Engine {

class Any {
    static constexpr usize SmallSize = 32;
    static constexpr usize SmallAlign = alignof(void*);

    struct VTable {
        void (*Destroy)(void*) noexcept;
        void (*MoveConstruct)(void* Dst, void* Src) noexcept;
        void (*CopyConstruct)(void* Dst, const void* Src);
        bool IsSmall;
        usize TypeSize;
        usize TypeAlign;
    };

    template<typename T>
    static VTable MakeVTable() noexcept {
        return {
            [](void* P) noexcept { static_cast<T*>(P)->~T(); },
            [](void* Dst, void* Src) noexcept {
                ::new (Dst) T(Move(*static_cast<T*>(Src)));
                static_cast<T*>(Src)->~T();
            },
            [](void* Dst, const void* Src) {
                ::new (Dst) T(*static_cast<const T*>(Src));
            },
            sizeof(T) <= SmallSize && alignof(T) <= SmallAlign,
            sizeof(T),
            alignof(T)
        };
    }

    alignas(SmallAlign) uint8 SmallStorage_[SmallSize]{};
    void* HeapPtr_ = nullptr;
    const VTable* VT_ = nullptr;
    bool Engaged_ = false;
    Memory::IAllocator* Allocator_ = nullptr;

    void* AllocHeap(usize Size, usize Align) noexcept {
        if (Allocator_) return Allocator_->Allocate(Size, Align);
        return Memory::GetDefaultAllocator().Allocate(Size, Align);
    }

    void FreeHeap(void* Ptr) noexcept {
        if (!Ptr) return;
        if (Allocator_) Allocator_->Free(Ptr);
        else Memory::GetDefaultAllocator().Free(Ptr);
    }

    void* Storage() noexcept {
        return VT_ && VT_->IsSmall ? static_cast<void*>(SmallStorage_)
                                    : HeapPtr_;
    }
    const void* Storage() const noexcept {
        return VT_ && VT_->IsSmall ? static_cast<const void*>(SmallStorage_)
                                    : HeapPtr_;
    }

public:
    Any() noexcept = default;
    explicit Any(Memory::IAllocator* alloc) noexcept : Allocator_(alloc) {}

    template<typename T>
        requires (!IsSame<RemoveCVRef<T>, Any>)
    Any(T&& Value, Memory::IAllocator* alloc = nullptr) noexcept  // NOLINT
        : Allocator_(alloc) {
        using Decayed = RemoveCVRef<T>;
        static constinit VTable VT = MakeVTable<Decayed>();
        VT_ = &VT;
        if constexpr (sizeof(Decayed) <= SmallSize && alignof(Decayed) <= SmallAlign) {
            ::new (static_cast<void*>(SmallStorage_)) Decayed(Forward<T>(Value));
        } else {
            HeapPtr_ = AllocHeap(sizeof(Decayed), alignof(Decayed));
            ::new (HeapPtr_) Decayed(Forward<T>(Value));
        }
        Engaged_ = true;
    }

    Any(Any&& Other) noexcept
        : VT_(Other.VT_), Engaged_(Other.Engaged_) {
        if (Engaged_) {
            if (VT_->IsSmall) {
                VT_->MoveConstruct(SmallStorage_, Other.SmallStorage_);
            } else {
                HeapPtr_ = Other.HeapPtr_;
                Other.HeapPtr_ = nullptr;
            }
            Other.Engaged_ = false;
            Other.VT_ = nullptr;
        }
    }

    Any(const Any& Other)
        : VT_(Other.VT_), Engaged_(Other.Engaged_), Allocator_(Other.Allocator_) {
        if (Engaged_) {
            if (VT_->IsSmall) {
                VT_->CopyConstruct(SmallStorage_, Other.SmallStorage_);
            } else {
                HeapPtr_ = AllocHeap(VT_->TypeSize, VT_->TypeAlign);
                VT_->CopyConstruct(HeapPtr_, Other.HeapPtr_);
            }
        }
    }

    ~Any() { Reset(); }

    Any& operator=(Any&& Other) noexcept {
        if (this != &Other) {
            Reset();
            VT_ = Other.VT_;
            Engaged_ = Other.Engaged_;
            if (Engaged_) {
                if (VT_->IsSmall) {
                    VT_->MoveConstruct(SmallStorage_, Other.SmallStorage_);
                } else {
                    HeapPtr_ = Other.HeapPtr_;
                    Other.HeapPtr_ = nullptr;
                }
                Other.Engaged_ = false;
                Other.VT_ = nullptr;
            }
        }
        return *this;
    }

    Any& operator=(const Any& Other) {
        Any Tmp(Other);
        *this = Move(Tmp);
        return *this;
    }

    template<typename T>
        requires (!IsSame<RemoveCVRef<T>, Any>)
    Any& operator=(T&& Value) noexcept {
        Any Tmp(Forward<T>(Value));
        *this = Move(Tmp);
        return *this;
    }

    void Reset() noexcept {
        if (Engaged_) {
            VT_->Destroy(Storage());
            if (!VT_->IsSmall && HeapPtr_) {
                FreeHeap(HeapPtr_);
                HeapPtr_ = nullptr;
            }
            Engaged_ = false;
            VT_ = nullptr;
        }
    }

    bool HasValue() const noexcept { return Engaged_; }
    explicit operator bool() const noexcept { return Engaged_; }

    /// @brief Cast to concrete type (unchecked — caller must know the type)
    template<typename T>
    T& As() noexcept {
        ENGINE_ASSERT(Engaged_);
        return *static_cast<T*>(Storage());
    }

    template<typename T>
    const T& As() const noexcept {
        ENGINE_ASSERT(Engaged_);
        return *static_cast<const T*>(Storage());
    }
};

} // namespace Engine

#endif // !ANY_HDR