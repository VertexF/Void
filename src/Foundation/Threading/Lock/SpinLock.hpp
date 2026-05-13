#ifndef FOUNDATION_THREADING_LOCK_SPINLOCK_HDR
#define FOUNDATION_THREADING_LOCK_SPINLOCK_HDR

#include <cstdint>
#include <Foundation/Threading/Atomic.hpp>

#if defined(ENGINE_COMPILER_MSVC)
    #include <intrin.h>
#endif

namespace Engine::Threading {
    namespace Detail {
        inline void ProcessorPause() noexcept
        {
#if defined(ENGINE_COMPILER_MSVC) && (defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86))
            _mm_pause();
#elif (defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)) && (defined(ENGINE_ARCH_X64) || defined(ENGINE_ARCH_X86))
            __builtin_ia32_pause();
#endif
        }
    } // namespace Detail

    class SpinLock final {
    public:
        SpinLock() noexcept = default;
        SpinLock(const SpinLock&) = delete;
        SpinLock& operator=(const SpinLock&) = delete;

        void Lock() noexcept
        {
            while (true) {
                uint32_t expected = 0;
                if (m_flag.CompareExchange(expected, 1u, MemoryOrder::Acquire, MemoryOrder::Relaxed))
                    break;
                Detail::ProcessorPause();
            }
        }

        bool TryLock() noexcept
        {
            uint32_t expected = 0;
            return m_flag.CompareExchange(expected, 1u, MemoryOrder::Acquire, MemoryOrder::Relaxed);
        }

        void Unlock() noexcept { m_flag.Store(0u, MemoryOrder::Release); }

    private:
        Atomic<uint32_t> m_flag{0u};
    };

    class SpinLockGuard final {
    public:
        explicit SpinLockGuard(SpinLock& lock) noexcept : m_lock(&lock) { m_lock->Lock(); }
        ~SpinLockGuard() noexcept { m_lock->Unlock(); }

        SpinLockGuard(const SpinLockGuard&) = delete;
        SpinLockGuard& operator=(const SpinLockGuard&) = delete;

    private:
        SpinLock* m_lock = nullptr;
    };
} // namespace Engine::Threading

#endif // !FOUNDATION_THREADING_LOCK_SPINLOCK_HDR
