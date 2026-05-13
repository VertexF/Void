#ifndef FOUNDATION_MEMORY_VIRTUALMEMORY_HDR
#define FOUNDATION_MEMORY_VIRTUALMEMORY_HDR

#include <Foundation/Memory/UniquePtr.hpp>
#include <Foundation/Platform.hpp>

// ============================================================================
// Engine - Memory Abstraction Layer (MAL)
// Virtual Memory Interface
// ============================================================================
//
// This interface provides platform-agnostic virtual memory operations.
// Native APIs (VirtualAlloc, mmap) are only used in MAL implementations.
//
// ============================================================================

#include <Utility/Macros.hpp>

namespace Engine::Memory {

/// @brief Platform-agnostic virtual memory interface
/// @details Provides reserve/commit/decommit/release operations for virtual memory.
///          Implementations use native APIs (VirtualAlloc on Windows, mmap on POSIX).
class IVirtualMemory {
public:
    virtual ~IVirtualMemory() = default;

    /// Get system page size in bytes
    [[nodiscard]] virtual size_t PageSize() noexcept = 0;

    /// Reserve virtual address space without committing physical memory
    /// @param size Size in bytes (will be rounded up to page size)
    /// @return Pointer to reserved memory, or nullptr on failure
    [[nodiscard]] virtual void* Reserve(size_t size) noexcept = 0;

    /// Commit physical memory to previously reserved address space
    /// @param address Start address (must be within reserved range)
    /// @param size Size in bytes to commit
    /// @return true on success
    [[nodiscard]] virtual bool Commit(void* address, size_t size) noexcept = 0;

    /// Decommit physical memory while keeping address space reserved
    /// @param address Start address
    /// @param size Size in bytes to decommit
    /// @return true on success
    [[nodiscard]] virtual bool Decommit(void* address, size_t size) noexcept = 0;

    /// Release both physical memory and address space
    /// @param address Previously reserved address
    virtual void Release(void* address) noexcept = 0;

    /// Change memory protection for a range
    enum class MemoryProtection : uint8 {
        NoAccess,
        ReadOnly,
        ReadWrite,
        Execute,
        ExecuteRead,
        ExecuteReadWrite
    };
    [[nodiscard]] virtual bool Protect(void* address, size_t size, MemoryProtection protection) noexcept = 0;

    /// Reserve and commit in one operation
    /// @param size Size in bytes
    /// @return Pointer to committed memory, or nullptr on failure
    [[nodiscard]] virtual void* ReserveAndCommit(size_t size) noexcept = 0;

    // ========================================================================
    // Large Page Support (optional, for TLB optimization)
    // ========================================================================

    /// @brief Get large page size (0 if not available)
    /// @return Large page size in bytes (e.g., 2MB on x64), or 0 if not supported
    [[nodiscard]] virtual size_t LargePageSize() noexcept { return 0; }

    /// @brief Check if large pages are available on this system
    /// @return True if large pages can be allocated
    [[nodiscard]] virtual bool AreLargePagesAvailable() noexcept { return false; }

    /// @brief Reserve and commit memory using large pages
    /// @param size Size in bytes (will be rounded up to large page boundary)
    /// @return Pointer to committed large-page memory, or nullptr on failure
    [[nodiscard]] virtual void* ReserveAndCommitLargePages(size_t size) noexcept { (void)size; return nullptr; }

    // ========================================================================
    // NUMA-Aware Allocation (optional, for NUMA topology optimization)
    // ========================================================================

    /// @brief Get the number of NUMA nodes available on this system
    /// @return Number of NUMA nodes (1 = uniform memory, >1 = NUMA)
    [[nodiscard]] virtual uint32 GetNumaNodeCount() noexcept { return 1; }

    /// @brief Reserve and commit memory on a specific NUMA node
    /// @param size Size in bytes (will be rounded up to page boundary)
    /// @param numaNode NUMA node index to allocate on (0-based)
    /// @return Pointer to committed memory on the specified node, or nullptr on failure
    /// @details Falls back to regular allocation if NUMA is not available or
    ///          the specified node is invalid. Callers should check GetNumaNodeCount()
    ///          to determine if NUMA allocation is meaningful.
    [[nodiscard]] virtual void* ReserveAndCommitOnNode(size_t size, uint32 numaNode) noexcept
    {
        (void)numaNode;
        return ReserveAndCommit(size);
    }
};

/// Create platform-specific virtual memory instance
[[nodiscard]] UniquePtr<IVirtualMemory> CreateVirtualMemory() noexcept;

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_VIRTUALMEMORY_HDR
