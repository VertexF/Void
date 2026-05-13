#include "CoreHeap.hpp"
#include "PageHeap.hpp"

namespace Engine::Memory {

void CoreHeap::Initialize(PageHeap* pageHeap) noexcept
{
    m_pageHeap = pageHeap;
    m_freePages.Store(nullptr, MemoryOrder::Relaxed);
    m_numFreePages.Store(0, MemoryOrder::Relaxed);
}

void* CoreHeap::AllocatePage(size_t pageSize, uint32 sizeClass) noexcept
{
    (void)sizeClass;
    
    // Lock-free pop from local cache
    FreePageNode* oldHead = m_freePages.Load(MemoryOrder::Acquire);
    while (oldHead) {
        if (m_freePages.CompareExchangeWeak(oldHead, oldHead->next,
                                             MemoryOrder::Release,
                                             MemoryOrder::Acquire)) {
            m_numFreePages.FetchSub(1, MemoryOrder::Relaxed);
            return oldHead;
        }
    }
    
    if (m_pageHeap) {
        return m_pageHeap->AllocatePage(pageSize);
    }
    return nullptr;
}

void CoreHeap::FreePage(void* ptr, size_t pageSize) noexcept
{
    if (!ptr) return;

    if (m_numFreePages.Load(MemoryOrder::Relaxed) < kMaxCachedPages) {
        FreePageNode* node = static_cast<FreePageNode*>(ptr);
        FreePageNode* oldHead = m_freePages.Load(MemoryOrder::Relaxed);
        do {
            node->next = oldHead;
        } while (!m_freePages.CompareExchangeWeak(oldHead, node,
                                                   MemoryOrder::Release,
                                                   MemoryOrder::Relaxed));
        m_numFreePages.FetchAdd(1, MemoryOrder::Relaxed);
    } else {
        if (m_pageHeap) {
            m_pageHeap->FreePage(ptr, pageSize);
        }
    }
}

} // namespace Engine::Memory
