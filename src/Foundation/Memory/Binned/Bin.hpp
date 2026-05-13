#ifndef FOUNDATION_MEMORY_BINNED_BIN_HDR
#define FOUNDATION_MEMORY_BINNED_BIN_HDR

#include <Foundation/Platform.hpp>
#include <Utility/Macros.hpp>

namespace Engine::Memory {

/// @brief Manages a linked list of free blocks within a memory page
/// @details Designed for single-threaded use (TLS cache) or SpinLocked use (Global Heap).
///          Does NOT own the memory it manages.
struct Bin {
    struct FreeNode {
        FreeNode* next;
    };

    FreeNode* freeList = nullptr;
    uint32 blockSize = 0;
    uint32 freeCount = 0;
    uint32 totalCount = 0;

    void Initialize(void* memory, size_t size, uint32 blockSize) noexcept;
    void Reset() noexcept;

    [[nodiscard]] void* Allocate() noexcept;
    void Free(void* ptr) noexcept;

    [[nodiscard]] bool HasFreeBlocks() const noexcept { return freeList != nullptr; }
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_BINNED_BIN_HDR