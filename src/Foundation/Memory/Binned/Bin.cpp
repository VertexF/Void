#include <Foundation/Memory/Binned/Bin.hpp>

namespace Engine::Memory {

void Bin::Initialize(void* memory, size_t size, uint32 blockSz) noexcept
{
    if (!memory || blockSz < sizeof(FreeNode)) {
        Reset();
        return;
    }

    blockSize = blockSz;
    freeCount = 0;
    
    // Build the free list
    uint8* current = static_cast<uint8*>(memory);
    const size_t numBlocks = size / blockSize;
    totalCount = static_cast<uint32>(numBlocks);
    
    if (numBlocks == 0) {
        Reset();
        return;
    }

    freeList = reinterpret_cast<FreeNode*>(current);
    FreeNode* node = freeList;
    
    for (size_t i = 0; i < numBlocks - 1; ++i) {
        FreeNode* nextNode = reinterpret_cast<FreeNode*>(current + blockSize);
        node->next = nextNode;
        node = nextNode;
        current += blockSize;
        freeCount++;
    }
    
    node->next = nullptr; // Last node
    freeCount++; // Count the last one
}

void Bin::Reset() noexcept
{
    freeList = nullptr;
    blockSize = 0;
    freeCount = 0;
    totalCount = 0;
}

void* Bin::Allocate() noexcept
{
    if (!freeList) return nullptr;

    void* ptr = freeList;
    freeList = freeList->next;
    freeCount--;
    
    return ptr;
}

void Bin::Free(void* ptr) noexcept
{
    if (!ptr) return;

    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = freeList;
    freeList = node;
    freeCount++;
}

} // namespace Engine::Memory
