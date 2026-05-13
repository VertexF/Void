#ifndef FOUNDATION_MEMORY_TLS_CACHING_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_TLS_CACHING_ALLOCATOR_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Platform.hpp>

#include <Foundation/Threading/Atomic.hpp>

#include <array>

namespace Engine::Memory {

/// @brief Thread-local caching allocator wrapper
/// @details Reduces contention by caching freed blocks per-thread.
///          Best used with a ThreadSafeAllocator as the backing allocator.
///
/// Each thread maintains a local cache of recently freed allocations.
/// When Allocate() is called, the cache is checked first (fast path).
/// When Free() is called, the block is cached locally if room exists.
/// This amortizes lock contention across multiple allocations.
///
/// Usage:
/// @code
///   MallocAllocator backing;
///   ThreadSafeAllocator safe(&backing);
///   TLSCachingAllocator cached(&safe);
///   void* p = cached.Allocate(64);  // May use cached block (no lock)
///   cached.Free(p);                  // Cached locally (no lock)
/// @endcode
class TLSCachingAllocator : public IAllocator {
public:
    static constexpr size_t kDefaultCacheSize = 32;

    // These structs must be public for thread_local storage access
    struct CachedBlock {
        void* ptr = nullptr;
        size_t size = 0;
        size_t alignment = 0;
        const TLSCachingAllocator* owner = nullptr; // Track which allocator created this block
    };

    struct ThreadLocalCache {
        std::array<CachedBlock, kDefaultCacheSize> blocks{};
        size_t count = 0;
    };

    /// @brief Create a TLS caching allocator
    /// @param backingAllocator The allocator to forward to (default: system)
    /// @param cacheSize Max blocks to cache per thread (default: 32)
    explicit TLSCachingAllocator(IAllocator* backingAllocator = nullptr,
                                  size_t cacheSize = kDefaultCacheSize);
    ~TLSCachingAllocator() override;

    TLSCachingAllocator(const TLSCachingAllocator&) = delete;
    TLSCachingAllocator& operator=(const TLSCachingAllocator&) = delete;

    // ========================================================================
    // IAllocator Interface
    // ========================================================================

    /// @brief Allocate memory, checking thread-local cache first
    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;

    /// @brief Reallocate memory, bypasses cache for growth
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;

    /// @brief Free memory, caching locally if room exists
    void Free(void* ptr) override;

    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;

    // ========================================================================
    // TLS Caching Specific
    // ========================================================================

    /// @brief Flush the current thread's cache back to backing allocator
    void FlushCache();

    /// @brief Get cache hit count (for diagnostics)
    [[nodiscard]] size_t GetCacheHits() const noexcept { return m_cacheHits.Load(MemoryOrder::Relaxed); }

    /// @brief Get cache miss count (for diagnostics)
    [[nodiscard]] size_t GetCacheMisses() const noexcept { return m_cacheMisses.Load(MemoryOrder::Relaxed); }

    /// @brief Get backing allocator
    [[nodiscard]] IAllocator* GetBackingAllocator() const noexcept { return m_backingAllocator; }

private:
    [[nodiscard]] ThreadLocalCache& GetCache();
    void FlushCacheInternal(ThreadLocalCache& cache);

    IAllocator* m_backingAllocator = nullptr;
    size_t m_cacheSize = kDefaultCacheSize;
    Atomic<size_t> m_cacheHits{0};
    Atomic<size_t> m_cacheMisses{0};
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_TLS_CACHING_ALLOCATOR_HDR
