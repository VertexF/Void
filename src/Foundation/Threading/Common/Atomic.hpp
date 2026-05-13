#ifndef FOUNDATION_THREADING_ATOMIC_HDR
#define FOUNDATION_THREADING_ATOMIC_HDR

#include <cstddef>
#include <cstdint>

#include <Foundation/CompilerTraits.hpp>

#if defined(ENGINE_COMPILER_MSVC)
    #include <intrin.h>
#endif

namespace Engine::Threading {

    // ============================================================================
    // Memory Order
    // ============================================================================
    enum class MemoryOrder : size_t {
        Relaxed,
        Acquire,
        Release,
        AcqRel,
        SeqCst
    };

    // ============================================================================
    // Thread Fence
    // ============================================================================
    inline void AtomicFence(MemoryOrder order) noexcept {
        #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
            int mo = __ATOMIC_SEQ_CST;
            switch (order) {
                case MemoryOrder::Relaxed: mo = __ATOMIC_RELAXED; break;
                case MemoryOrder::Acquire: mo = __ATOMIC_ACQUIRE; break;
                case MemoryOrder::Release: mo = __ATOMIC_RELEASE; break;
                case MemoryOrder::AcqRel:  mo = __ATOMIC_ACQ_REL; break;
                case MemoryOrder::SeqCst:  mo = __ATOMIC_SEQ_CST; break;
            }
            __atomic_thread_fence(mo);
        #elif defined(ENGINE_COMPILER_MSVC)
            (void)order;
            _ReadWriteBarrier();
            #if defined(ENGINE_ARCH_X64)
                __faststorefence();
            #elif defined(ENGINE_ARCH_ARM64)
                __dmb(_ARM64_BARRIER_ISH);
            #endif
        #endif
    }

    // ============================================================================
    // Internal Helper
    // ============================================================================
    namespace Detail {
        #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
        ENGINE_FORCE_INLINE int ToBuiltinOrder(MemoryOrder order) noexcept
        {
            switch (order) {
                case MemoryOrder::Relaxed: return __ATOMIC_RELAXED;
                case MemoryOrder::Acquire: return __ATOMIC_ACQUIRE;
                case MemoryOrder::Release: return __ATOMIC_RELEASE;
                case MemoryOrder::AcqRel:  return __ATOMIC_ACQ_REL;
                default:                   return __ATOMIC_SEQ_CST;
            }
        }
        #endif
    } // namespace Detail

    // ============================================================================
    // Atomic<T>
    // ============================================================================
    /// @brief Lock-free atomic for 4-byte and 8-byte trivially copyable types.
    /// @details Uses compiler intrinsics directly. No <atomic> dependency.
    template<typename T>
    class Atomic {
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Atomic supports 1, 2, 4, or 8 byte types only.");
        static_assert(__is_trivially_copyable(T), "Atomic requires trivially copyable type.");

    public:
        constexpr Atomic() noexcept : m_value{} {}
        constexpr Atomic(T x) noexcept : m_value(x) {}

        Atomic(const Atomic&) = delete;
        Atomic& operator=(const Atomic&) = delete;

        inline T Load(MemoryOrder order = MemoryOrder::SeqCst) const noexcept {
            #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
                return __atomic_load_n(&m_value, Detail::ToBuiltinOrder(order));
            #elif defined(ENGINE_COMPILER_MSVC)
                (void)order;
                if constexpr (sizeof(T) == 1) {
                    char prev = _InterlockedCompareExchange8(
                        reinterpret_cast<volatile char*>(const_cast<T*>(&m_value)), 0, 0);
                    return *reinterpret_cast<T*>(&prev);
                } else if constexpr (sizeof(T) == 2) {
                    short prev = _InterlockedCompareExchange16(
                        reinterpret_cast<volatile short*>(const_cast<T*>(&m_value)), 0, 0);
                    return *reinterpret_cast<T*>(&prev);
                } else if constexpr (sizeof(T) == 4) {
                    long prev = _InterlockedCompareExchange(
                        reinterpret_cast<volatile long*>(const_cast<T*>(&m_value)), 0, 0);
                    return *reinterpret_cast<T*>(&prev);
                } else {
                    long long prev = _InterlockedCompareExchange64(
                        reinterpret_cast<volatile long long*>(const_cast<T*>(&m_value)), 0, 0);
                    return *reinterpret_cast<T*>(&prev);
                }
            #endif
        }

        inline void Store(T x, MemoryOrder order = MemoryOrder::SeqCst) noexcept {
            #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
                __atomic_store_n(&m_value, x, Detail::ToBuiltinOrder(order));
            #elif defined(ENGINE_COMPILER_MSVC)
                (void)order;
                if constexpr (sizeof(T) == 1) {
                    _InterlockedExchange8(reinterpret_cast<volatile char*>(&m_value), *reinterpret_cast<char*>(&x));
                } else if constexpr (sizeof(T) == 2) {
                    _InterlockedExchange16(reinterpret_cast<volatile short*>(&m_value), *reinterpret_cast<short*>(&x));
                } else if constexpr (sizeof(T) == 4) {
                    _InterlockedExchange(
                        reinterpret_cast<volatile long*>(&m_value),
                        *reinterpret_cast<long*>(&x));
                } else {
                    _InterlockedExchange64(
                        reinterpret_cast<volatile long long*>(&m_value),
                        *reinterpret_cast<long long*>(&x));
                }
            #endif
        }

        inline T Exchange(T x, MemoryOrder order = MemoryOrder::SeqCst) noexcept {
            #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
                return __atomic_exchange_n(&m_value, x, Detail::ToBuiltinOrder(order));
            #elif defined(ENGINE_COMPILER_MSVC)
                (void)order;
                if constexpr (sizeof(T) == 1) {
                    char prev = _InterlockedExchange8(reinterpret_cast<volatile char*>(&m_value), *reinterpret_cast<char*>(&x));
                    return *reinterpret_cast<T*>(&prev);
                } else if constexpr (sizeof(T) == 2) {
                    short prev = _InterlockedExchange16(reinterpret_cast<volatile short*>(&m_value), *reinterpret_cast<short*>(&x));
                    return *reinterpret_cast<T*>(&prev);
                } else if constexpr (sizeof(T) == 4) {
                    long prev = _InterlockedExchange(
                        reinterpret_cast<volatile long*>(&m_value),
                        *reinterpret_cast<long*>(&x));
                    return *reinterpret_cast<T*>(&prev);
                } else {
                    long long prev = _InterlockedExchange64(
                        reinterpret_cast<volatile long long*>(&m_value),
                        *reinterpret_cast<long long*>(&x));
                    return *reinterpret_cast<T*>(&prev);
                }
            #endif
        }

        inline T FetchAdd(T x, MemoryOrder order = MemoryOrder::SeqCst) noexcept {
            #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
                return __atomic_fetch_add(&m_value, x, Detail::ToBuiltinOrder(order));
            #elif defined(ENGINE_COMPILER_MSVC)
                (void)order;
                if constexpr (sizeof(T) == 1 || sizeof(T) == 2) {
                    T expected = Load(MemoryOrder::Relaxed);
                    while (true) {
                        T desired = static_cast<T>(expected + x);
                        if (CompareExchange(expected, desired, MemoryOrder::SeqCst, MemoryOrder::Relaxed)) {
                            return expected;
                        }
                    }
                } else if constexpr (sizeof(T) == 4) {
                    return static_cast<T>(_InterlockedExchangeAdd(
                        reinterpret_cast<volatile long*>(&m_value),
                        static_cast<long>(x)));
                } else {
                    return static_cast<T>(_InterlockedExchangeAdd64(
                        reinterpret_cast<volatile long long*>(&m_value),
                        static_cast<long long>(x)));
                }
            #endif
        }

        inline T FetchSub(T x, MemoryOrder order = MemoryOrder::SeqCst) noexcept {
            return FetchAdd(static_cast<T>(-static_cast<int64_t>(x)), order);
        }

        inline bool CompareExchange(T& expected, T desired, 
            MemoryOrder success = MemoryOrder::SeqCst,
            MemoryOrder failure = MemoryOrder::SeqCst) noexcept {
    
            #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
                return __atomic_compare_exchange_n(
                    &m_value, &expected, desired, false,
                    Detail::ToBuiltinOrder(success), Detail::ToBuiltinOrder(failure));

            #elif defined(ENGINE_COMPILER_MSVC)
                (void)success; (void)failure;
                if constexpr (sizeof(T) == 1) {
                    char prev = _InterlockedCompareExchange8(
                        reinterpret_cast<volatile char*>(&m_value), 
                        *reinterpret_cast<char*>(&desired), 
                        *reinterpret_cast<char*>(&expected));
                    if (prev == *reinterpret_cast<char*>(&expected)) return true;
                    expected = *reinterpret_cast<T*>(&prev);
                    return false;
                } else if constexpr (sizeof(T) == 2) {
                    short prev = _InterlockedCompareExchange16(
                        reinterpret_cast<volatile short*>(&m_value), 
                        *reinterpret_cast<short*>(&desired), 
                        *reinterpret_cast<short*>(&expected));
                    if (prev == *reinterpret_cast<short*>(&expected)) return true;
                    expected = *reinterpret_cast<T*>(&prev);
                    return false;
                } else if constexpr (sizeof(T) == 4) {
                    long prev = _InterlockedCompareExchange(
                        reinterpret_cast<volatile long*>(&m_value),
                        *reinterpret_cast<long*>(&desired),
                        *reinterpret_cast<long*>(&expected));
                    if (prev == *reinterpret_cast<long*>(&expected)) return true;
                    expected = *reinterpret_cast<T*>(&prev);
                    return false;
                } else {
                    long long prev = _InterlockedCompareExchange64(
                        reinterpret_cast<volatile long long*>(&m_value),
                        *reinterpret_cast<long long*>(&desired),
                        *reinterpret_cast<long long*>(&expected));
                    if (prev == *reinterpret_cast<long long*>(&expected)) return true;
                    expected = *reinterpret_cast<T*>(&prev);
                    return false;
                }
            #endif
        } // END Of CompareExchange

        /// @brief Weak CAS — may spuriously fail. Use in retry loops for better perf on ARM.
        inline bool CompareExchangeWeak(T& expected, T desired, 
            MemoryOrder success = MemoryOrder::SeqCst,
            MemoryOrder failure = MemoryOrder::SeqCst) noexcept {
    
            #if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
                return __atomic_compare_exchange_n(
                    &m_value, &expected, desired, true, // weak = true
                    Detail::ToBuiltinOrder(success), Detail::ToBuiltinOrder(failure));
                    
            #elif defined(ENGINE_COMPILER_MSVC)
                // MSVC x64 has no weak CAS — strong CAS is the only option
                return CompareExchange(expected, desired, success, failure);
            #endif
        }

        // ========================================================================
        // Arithmetic Operators (convenience for counter patterns)
        // ========================================================================
        /// @brief Pre-increment: atomically increments and returns new value
        inline T operator++() noexcept { return FetchAdd(1) + 1; }

        /// @brief Post-increment: atomically increments and returns old value
        inline T operator++(int) noexcept { return FetchAdd(1); }

        /// @brief Pre-decrement: atomically decrements and returns new value
        inline T operator--() noexcept { return FetchSub(1) - 1; }

        /// @brief Post-decrement: atomically decrements and returns old value
        inline T operator--(int) noexcept { return FetchSub(1); }

        /// @brief Atomic add-assign
        inline T operator+=(T x) noexcept { return FetchAdd(x) + x; }

        /// @brief Atomic sub-assign
        inline T operator-=(T x) noexcept { return FetchSub(x) - x; }

    private:
        alignas(sizeof(T)) mutable T m_value;
    };

    /// @brief Pointer specialization convenience.
    template<typename T>
    using AtomicPtr = Atomic<T*>;

} // namespace Engine::Threading

namespace Engine {
    using ::Engine::Threading::Atomic;
    using ::Engine::Threading::AtomicPtr;
    using ::Engine::Threading::MemoryOrder;
} // namespace Engine

#endif // !FOUNDATION_THREADING_ATOMIC_HDR
