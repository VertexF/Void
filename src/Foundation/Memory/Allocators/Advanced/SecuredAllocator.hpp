#ifndef FOUNDATION_MEMORY_SECURED_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_SECURED_ALLOCATOR_HDR

#include <Foundation/Memory/UniquePtr.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/VirtualMemory.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>

#include <unordered_map>

namespace Engine::Memory {

/// @brief Allocator that uses OS page protection for security
/// @details Each allocation is padded with PROT_NONE guard pages.
///          Supports "Read-Only" mode to lock memory after initialization.
class SecuredAllocator final : public IAllocator {
public:
    SecuredAllocator() noexcept;
    ~SecuredAllocator() override;

    // ========================================================================
    // IAllocator Interface
    // ========================================================================

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = 16) override;
    void Free(void* ptr) override;
    
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = 16) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override { return "SecuredAllocator"; }
    [[nodiscard]] bool Owns(void* ptr) const override;

    // ========================================================================
    // Security Operations
    // ========================================================================

    /// @brief Make an allocation read-only
    void MakeReadOnly(void* ptr);

    /// @brief Make an allocation read-write (unlock)
    void MakeReadWrite(void* ptr);

    /// @brief Wipe memory (zero out) and decommit an allocation
    void ScrubAndFree(void* ptr);

private:
    UniquePtr<IVirtualMemory> m_vm;
    size_t m_allocatedTotal = 0;
    
    struct AllocationInfo {
        void* baseAddress; // Including guard pages
        void* committedAddress;
        size_t totalSize;
        size_t committedSize;
        size_t userSize;
        size_t alignment;
    };
    std::unordered_map<void*, AllocationInfo> m_allocs;
    mutable Threading::SpinLock m_lock;
};

} // namespace Engine::Memory
#endif // FOUNDATION_MEMORY_SECURED_ALLOCATOR_HDR
