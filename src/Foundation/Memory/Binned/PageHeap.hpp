#ifndef FOUNDATION_MEMORY_PAGE_HEAP_HDR
#define FOUNDATION_MEMORY_PAGE_HEAP_HDR

#include <Foundation/Memory/VirtualMemory.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

class PageHeap {
public:
    void Initialize(IVirtualMemory* vm, void* baseAddress, size_t reservedSize) noexcept;
    
    // Allocate a page (usually 64KB or similar)
    [[nodiscard]] void* AllocatePage(size_t pageSize) noexcept;
    
    // Return a page
    void FreePage(void* ptr, size_t pageSize) noexcept;

    struct PageHeader {
        Atomic<void*> freeList;     // Thread-safe free list for this page
        Atomic<uint32> activeAllocations; // Blocks currently vended to TLS/App
        uint32 sizeClass;
        uint32 numBlocks;               // Total blocks this page can hold
    };

private:
    IVirtualMemory* m_vm = nullptr;
    uint8* m_baseAddress = nullptr;
    size_t m_reservedSize = 0;
    
    // Lock-free bump pointer components
    Atomic<size_t> m_bumpOffset{0};
    
    // Lock-free stack of recycled pages
    struct FreePageNode {
        FreePageNode* next;
    };
    Atomic<FreePageNode*> m_freeList{nullptr};
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_PAGE_HEAP_HDR