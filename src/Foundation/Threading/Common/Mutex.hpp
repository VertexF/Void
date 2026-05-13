#ifndef MUTEX_HDR
#define MUTEX_HDR

#include <Foundation/Threading/Atomic.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Engine::Threading {

// ============================================================================
// Mutex
// ============================================================================

/// @brief Lightweight mutex using platform primitives
/// @details Windows: SRWLOCK (zero-init, no heap, ~pointer-sized)
///          POSIX: pthread_mutex_t
class Mutex {
public:
    Mutex(const char* debugName = nullptr) noexcept
    {
        (void)debugName;
    }

    ~Mutex() noexcept = default;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void Lock() noexcept {
        Lock_.lock();
    }

    bool TryLock() noexcept {
        return Lock_.try_lock();
    }

    void Unlock() noexcept {
        Lock_.unlock();
    }

    /// @brief Native handle for ConditionVariable interop
    std::mutex* NativeHandle() noexcept { return &Lock_; }

private:
    std::mutex Lock_;
};

// ============================================================================
// LockGuard
// ============================================================================

/// @brief RAII lock wrapper — locks on construction, unlocks on destruction
template<typename MutexType>
class LockGuard {
public:
    explicit LockGuard(MutexType& M) noexcept : Mutex_(M) { Mutex_.Lock(); }
    ~LockGuard() noexcept { Mutex_.Unlock(); }

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    MutexType& Mutex_;
};

// ============================================================================
// UniqueLock
// ============================================================================

struct DeferLockTag {};
struct TryLockTag {};

/// @brief Flexible RAII lock wrapper with defer/try/adopt strategies
template<typename MutexType>
class UniqueLock {
public:
    UniqueLock() noexcept : Mutex_(nullptr), Owns_(false) {}

    explicit UniqueLock(MutexType& M) noexcept : Mutex_(&M), Owns_(true) {
        Mutex_->Lock();
    }

    // Deferred — don't lock yet
    UniqueLock(MutexType& M, DeferLockTag) noexcept : Mutex_(&M), Owns_(false) {}

    // Try — attempt lock, non-blocking
    UniqueLock(MutexType& M, TryLockTag) noexcept : Mutex_(&M), Owns_(M.TryLock()) {}

    ~UniqueLock() noexcept {
        if (Owns_) Mutex_->Unlock();
    }

    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;

    UniqueLock(UniqueLock&& Other) noexcept
        : Mutex_(Other.Mutex_), Owns_(Other.Owns_) {
        Other.Mutex_ = nullptr;
        Other.Owns_ = false;
    }

    UniqueLock& operator=(UniqueLock&& Other) noexcept {
        if (Owns_) Mutex_->Unlock();
        Mutex_ = Other.Mutex_;
        Owns_ = Other.Owns_;
        Other.Mutex_ = nullptr;
        Other.Owns_ = false;
        return *this;
    }

    void Lock() noexcept { Mutex_->Lock(); Owns_ = true; }
    bool TryLock() noexcept { Owns_ = Mutex_->TryLock(); return Owns_; }
    void Unlock() noexcept { Mutex_->Unlock(); Owns_ = false; }

    MutexType* GetMutex() const noexcept { return Mutex_; }
    bool OwnsLock() const noexcept { return Owns_; }
    explicit operator bool() const noexcept { return Owns_; }

private:
    MutexType* Mutex_;
    bool Owns_;
};

// ============================================================================
// ScopedLock (multi-mutex)
// ============================================================================

/// @brief RAII wrapper for a single mutex (multi-mutex version is TODO)
template<typename MutexType>
class ScopedLock {
public:
    explicit ScopedLock(MutexType& M) noexcept : Mutex_(M) { Mutex_.Lock(); }
    ~ScopedLock() noexcept { Mutex_.Unlock(); }

    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    MutexType& Mutex_;
};

// ============================================================================
// ConditionVariable
// ============================================================================

/// @brief Condition variable for thread synchronization
/// @details Windows: CONDITION_VARIABLE (SRW-backed). POSIX: pthread_cond_t.
class ConditionVariable {
public:
    ConditionVariable() noexcept = default;

    ~ConditionVariable() noexcept = default;

    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

    void NotifyOne() noexcept {
        Cv_.notify_one();
    }

    void NotifyAll() noexcept {
        Cv_.notify_all();
    }

    void Wait(UniqueLock<Mutex>& Lock) noexcept
    {
        std::unique_lock<std::mutex> nativeLock(*Lock.GetMutex()->NativeHandle(), std::adopt_lock);
        Cv_.wait(nativeLock);
        nativeLock.release();
    }

    template<typename Predicate>
    void Wait(UniqueLock<Mutex>& Lock, Predicate Pred) noexcept {
        while (!Pred()) {
            Wait(Lock);
        }
    }

    /// @return true if notified, false if timeout
    bool WaitFor(UniqueLock<Mutex>& Lock, uint32_t TimeoutMs) noexcept
    {
        std::unique_lock<std::mutex> nativeLock(*Lock.GetMutex()->NativeHandle(), std::adopt_lock);
        const bool notified = Cv_.wait_for(nativeLock, std::chrono::milliseconds(TimeoutMs)) != std::cv_status::timeout;
        nativeLock.release();
        return notified;
    }

private:
    std::condition_variable Cv_;
};

// ============================================================================
// OnceFlag / CallOnce
// ============================================================================

/// @brief Flag for one-time initialization
struct OnceFlag {
    constexpr OnceFlag() noexcept = default;
    OnceFlag(const OnceFlag&) = delete;
    OnceFlag& operator=(const OnceFlag&) = delete;

    Atomic<uint32_t> Called{0};
    Atomic<uint32_t> Lock{0};
};

/// @brief Execute function exactly once (thread-safe)
template<typename Fn, typename... Args>
void CallOnce(OnceFlag& Flag, Fn&& Func, Args&&... Arguments) {
    if (Flag.Called.Load(MemoryOrder::Acquire) != 0) {
        return;
    }

    // Spin-lock for double-checked locking
    uint32_t Expected = 0;
    while (!Flag.Lock.CompareExchangeWeak(Expected, 1, MemoryOrder::Acquire)) {
        Expected = 0;
        // Spin
    }

    if (Flag.Called.Load(MemoryOrder::Relaxed) == 0) {
        static_cast<Fn&&>(Func)(static_cast<Args&&>(Arguments)...);
        Flag.Called.Store(1, MemoryOrder::Release);
    }

    Flag.Lock.Store(0, MemoryOrder::Release);
}

} // namespace Engine::Threading

// ============================================================================
// Namespace Re-exports
// ============================================================================

namespace Engine {
    using ::Engine::Threading::Mutex;
    using ::Engine::Threading::LockGuard;
    using ::Engine::Threading::UniqueLock;
    using ::Engine::Threading::ScopedLock;
    using ::Engine::Threading::ConditionVariable;
    using ::Engine::Threading::OnceFlag;
    using ::Engine::Threading::DeferLockTag;
    using ::Engine::Threading::TryLockTag;
} // namespace Engine

#endif // !MUTEX_HDR
