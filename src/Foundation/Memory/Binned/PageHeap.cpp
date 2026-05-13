#include "PageHeap.hpp"
#include <Foundation/Memory/VirtualMemory.hpp> 
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

void PageHeap::Initialize(IVirtualMemory* vm, void* baseAddress, size_t reservedSize) noexcept
{
    m_vm = vm;
    m_baseAddress = static_cast<uint8*>(baseAddress);
    m_reservedSize = reservedSize;
    m_bumpOffset.Store(0, MemoryOrder::Relaxed);
    m_freeList.Store(nullptr, MemoryOrder::Relaxed);
}

void* PageHeap::AllocatePage(size_t pageSize) noexcept
{
    if (!m_vm || !m_baseAddress || pageSize == 0 || pageSize > m_reservedSize) {
        return nullptr;
    }

    // 1. Try to pop from recycled list (lock-free stack)
    FreePageNode* oldHead = m_freeList.Load(MemoryOrder::Acquire);
    while (oldHead) {
        if (m_freeList.CompareExchangeWeak(oldHead, oldHead->next,
                                            MemoryOrder::Release,
                                            MemoryOrder::Acquire)) {
            void* ptr = oldHead;
            auto* header = static_cast<PageHeader*>(ptr);
            header->freeList.Store(nullptr, MemoryOrder::Relaxed);
            header->activeAllocations.Store(0, MemoryOrder::Relaxed);
            header->numBlocks = 0;
            return ptr;
        }
    }

    // 2. Atomic bump allocation for new pages
    size_t offset = m_bumpOffset.Load(MemoryOrder::Acquire);
    while (true) {
        if (offset > m_reservedSize || pageSize > m_reservedSize - offset) {
            return nullptr; // OOM in reserved space
        }

        const size_t nextOffset = offset + pageSize;
        if (m_bumpOffset.CompareExchangeWeak(offset, nextOffset,
                                             MemoryOrder::Release,
                                             MemoryOrder::Acquire)) {
            break;
        }
    }

    void* ptr = m_baseAddress + offset;
    // Commit memory (VirtualAlloc is thread-safe on Windows)
    if (!m_vm->Commit(ptr, pageSize)) {
        return nullptr; // Commit failed
    }
    
    auto* header = static_cast<PageHeader*>(ptr);
    header->freeList.Store(nullptr, MemoryOrder::Relaxed);
    header->activeAllocations.Store(0, MemoryOrder::Relaxed);
    header->numBlocks = 0;
    
    return ptr;
}

void PageHeap::FreePage(void* ptr, size_t /*pageSize*/) noexcept
{
    if (!ptr) return;

    // Lock-free push to recycled list
    FreePageNode* node = static_cast<FreePageNode*>(ptr);
    FreePageNode* oldHead = m_freeList.Load(MemoryOrder::Relaxed);
    do {
        node->next = oldHead;
    } while (!m_freeList.CompareExchangeWeak(oldHead, node,
                                              MemoryOrder::Release,
                                              MemoryOrder::Relaxed));
}

} // namespace Engine::Memory
