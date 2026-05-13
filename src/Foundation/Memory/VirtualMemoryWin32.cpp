// Virtual Memory - Windows Implementation
// ============================================================================

#include <Foundation/Memory/VirtualMemory.hpp>

#if defined(ENGINE_PLATFORM_WINDOWS)

#include <Foundation/Memory/UniquePtr.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace Engine::Memory {

namespace {

/// @brief Check if the current process has SeLockMemoryPrivilege
/// @details Required for MEM_LARGE_PAGES on Windows. Must be granted
/// by Group Policy (gpedit.msc -> Lock Pages in Memory).
[[nodiscard]] static bool HasLockMemoryPrivilege() noexcept
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        return false;
    }

    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, L"SeLockMemoryPrivilege", &luid))
    {
        CloseHandle(hToken);
        return false;
    }

    PRIVILEGE_SET privSet{};
    privSet.PrivilegeCount = 1;
    privSet.Control = PRIVILEGE_SET_ALL_NECESSARY;
    privSet.Privilege[0].Luid = luid;
    privSet.Privilege[0].Attributes = 0;

    BOOL result = FALSE;
    PrivilegeCheck(hToken, &privSet, &result);
    CloseHandle(hToken);

    return result != FALSE;
}

class Win32VirtualMemory final : public IVirtualMemory {
public:
    [[nodiscard]] size_t PageSize() noexcept override
    {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return static_cast<size_t>(info.dwPageSize);
    }

    [[nodiscard]] void* Reserve(size_t size) noexcept override
    {
        return VirtualAlloc(nullptr, static_cast<SIZE_T>(size), MEM_RESERVE, PAGE_READWRITE);
    }

    [[nodiscard]] bool Commit(void* address, size_t size) noexcept override
    {
        if (!address) return false;
        void* p = VirtualAlloc(address, static_cast<SIZE_T>(size), MEM_COMMIT, PAGE_READWRITE);
        return p != nullptr;
    }

    [[nodiscard]] bool Decommit(void* address, size_t size) noexcept override
    {
        if (!address) return false;
        return VirtualFree(address, static_cast<SIZE_T>(size), MEM_DECOMMIT) != 0;
    }

    void Release(void* address) noexcept override
    {
        if (address) {
            VirtualFree(address, 0, MEM_RELEASE);
        }
    }

    [[nodiscard]] bool Protect(void* address, size_t size, MemoryProtection protection) noexcept override
    {
        if (!address) return false;
        
        DWORD flProtect = 0;
        switch (protection) {
            case MemoryProtection::NoAccess: flProtect = PAGE_NOACCESS; break;
            case MemoryProtection::ReadOnly: flProtect = PAGE_READONLY; break;
            case MemoryProtection::ReadWrite: flProtect = PAGE_READWRITE; break;
            case MemoryProtection::Execute: flProtect = PAGE_EXECUTE; break;
            case MemoryProtection::ExecuteRead: flProtect = PAGE_EXECUTE_READ; break;
            case MemoryProtection::ExecuteReadWrite: flProtect = PAGE_EXECUTE_READWRITE; break;
        }
        
        DWORD oldProtect;
        return VirtualProtect(address, static_cast<SIZE_T>(size), flProtect, &oldProtect) != 0;
    }

    [[nodiscard]] void* ReserveAndCommit(size_t size) noexcept override
    {
        return VirtualAlloc(nullptr, static_cast<SIZE_T>(size), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }

    // ========================================================================
    // Large Page Support
    // ========================================================================

    [[nodiscard]] size_t LargePageSize() noexcept override
    {
        const SIZE_T largePageMin = GetLargePageMinimum();
        return static_cast<size_t>(largePageMin);
    }

    [[nodiscard]] bool AreLargePagesAvailable() noexcept override
    {
        // Cache the result — privilege won't change during process lifetime
        static bool s_checked = false;
        static bool s_available = false;

        if (!s_checked)
        {
            const SIZE_T largePageMin = GetLargePageMinimum();
            s_available = (largePageMin > 0) && HasLockMemoryPrivilege();
            s_checked = true;
        }

        return s_available;
    }

    [[nodiscard]] void* ReserveAndCommitLargePages(size_t size) noexcept override
    {
        if (!AreLargePagesAvailable())
        {
            return nullptr;
        }

        const SIZE_T largePageMin = GetLargePageMinimum();
        if (largePageMin == 0)
        {
            return nullptr;
        }

        // Round up to large page boundary
        const SIZE_T alignedSize = (static_cast<SIZE_T>(size) + largePageMin - 1) & ~(largePageMin - 1);

        return VirtualAlloc(
            nullptr,
            alignedSize,
            MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
            PAGE_READWRITE);
    }

    // ========================================================================
    // NUMA-Aware Allocation
    // ========================================================================

    [[nodiscard]] uint32 GetNumaNodeCount() noexcept override
    {
        ULONG highestNode = 0;
        if (!GetNumaHighestNodeNumber(&highestNode))
        {
            return 1;
        }
        return static_cast<uint32>(highestNode + 1);
    }

    [[nodiscard]] void* ReserveAndCommitOnNode(size_t size, uint32 numaNode) noexcept override
    {
        // Validate NUMA node is within range
        ULONG highestNode = 0;
        if (!GetNumaHighestNodeNumber(&highestNode) || numaNode > highestNode)
        {
            // Invalid node — fall back to default allocation
            return ReserveAndCommit(size);
        }

        void* ptr = VirtualAllocExNuma(
            GetCurrentProcess(),
            nullptr,
            static_cast<SIZE_T>(size),
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE,
            static_cast<DWORD>(numaNode));

        if (!ptr)
        {
            // NUMA allocation failed — fall back to default allocation
            return ReserveAndCommit(size);
        }

        return ptr;
    }
};

} // namespace

UniquePtr<IVirtualMemory> CreateVirtualMemory() noexcept
{
    return MakeUnique<Win32VirtualMemory>();
}

} // namespace Engine::Memory

#endif
