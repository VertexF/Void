#ifndef FOUNDATION_MEMORY_CORE_HEAP_HDR
#define FOUNDATION_MEMORY_CORE_HEAP_HDR

#include <Foundation/Memory/Binned/Bin.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

class PageHeap;

/// @brief Intermediate heap layer pinned to a physical CPU core
/// @details Reduces contention on the Global Page Heap by caching pages
///          per-core. Refills TLS caches.
class CoreHeap {
public:
    void Initialize(PageHeap* pageHeap) noexcept;
    
    // Fetch a page for a specific size class
    [[nodiscard]] void* AllocatePage(size_t pageSize, uint32 sizeClass) noexcept;
    
    // Return a page
    void FreePage(void* ptr, size_t pageSize) noexcept;

private:
    PageHeap* m_pageHeap = nullptr;
    
    struct FreePageNode {
        FreePageNode* next;
    };
    Atomic<FreePageNode*> m_freePages{nullptr};
    Atomic<uint32> m_numFreePages{0};
    static constexpr uint32 kMaxCachedPages = 64; // Limit per-core cache
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_CORE_HEAP_HDR