#include <Foundation/Memory/VirtualMemory.hpp>

#if defined(ENGINE_PLATFORM_LINUX)

#include <Foundation/Memory/UniquePtr.hpp>

#include <sys/mman.h>
#include <unistd.h>

#include <unordered_map>

namespace Engine::Memory {

namespace {

[[nodiscard]] int ToNativeProtection(IVirtualMemory::MemoryProtection protection) noexcept
{
    switch (protection) {
    case IVirtualMemory::MemoryProtection::NoAccess:
        return PROT_NONE;
    case IVirtualMemory::MemoryProtection::ReadOnly:
        return PROT_READ;
    case IVirtualMemory::MemoryProtection::ReadWrite:
        return PROT_READ | PROT_WRITE;
    case IVirtualMemory::MemoryProtection::Execute:
        return PROT_EXEC;
    case IVirtualMemory::MemoryProtection::ExecuteRead:
        return PROT_EXEC | PROT_READ;
    case IVirtualMemory::MemoryProtection::ExecuteReadWrite:
        return PROT_EXEC | PROT_READ | PROT_WRITE;
    }
    return PROT_NONE;
}

class PosixVirtualMemory final : public IVirtualMemory {
public:
    [[nodiscard]] size_t PageSize() noexcept override
    {
        const long pageSize = sysconf(_SC_PAGESIZE);
        return pageSize > 0 ? static_cast<size_t>(pageSize) : 4096u;
    }

    [[nodiscard]] void* Reserve(size_t size) noexcept override
    {
        void* address = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (address == MAP_FAILED) {
            return nullptr;
        }
        m_regions[address] = size;
        return address;
    }

    [[nodiscard]] bool Commit(void* address, size_t size) noexcept override
    {
        if (!address || size == 0) {
            return false;
        }
        return mprotect(address, size, PROT_READ | PROT_WRITE) == 0;
    }

    [[nodiscard]] bool Decommit(void* address, size_t size) noexcept override
    {
        if (!address || size == 0) {
            return false;
        }
        return madvise(address, size, MADV_DONTNEED) == 0 &&
            mprotect(address, size, PROT_NONE) == 0;
    }

    void Release(void* address) noexcept override
    {
        if (!address) {
            return;
        }
        const auto it = m_regions.find(address);
        if (it == m_regions.end()) {
            return;
        }
        munmap(address, it->second);
        m_regions.erase(it);
    }

    [[nodiscard]] bool Protect(void* address, size_t size, MemoryProtection protection) noexcept override
    {
        if (!address || size == 0) {
            return false;
        }
        return mprotect(address, size, ToNativeProtection(protection)) == 0;
    }

    [[nodiscard]] void* ReserveAndCommit(size_t size) noexcept override
    {
        void* address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (address == MAP_FAILED) {
            return nullptr;
        }
        m_regions[address] = size;
        return address;
    }

private:
    std::unordered_map<void*, size_t> m_regions;
};

} // namespace

UniquePtr<IVirtualMemory> CreateVirtualMemory() noexcept
{
    return MakeUnique<PosixVirtualMemory>();
}

} // namespace Engine::Memory

#endif
