#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Allocators/Advanced/SecuredAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>

#include <limits>

namespace Engine::Memory {

namespace {

[[nodiscard]] size_t RoundUpToPage(size_t value, size_t pageSize) noexcept
{
    return (value + pageSize - 1) & ~(pageSize - 1);
}

} // namespace

SecuredAllocator::SecuredAllocator() noexcept
    : m_vm(CreateVirtualMemory())
{
}

SecuredAllocator::~SecuredAllocator()
{
    Threading::SpinLockGuard guard(m_lock);
    for (auto& [ptr, info] : m_allocs) {
        (void)ptr;
        m_vm->Release(info.baseAddress);
    }
    m_allocs.clear();
    m_allocatedTotal = 0;
}

void* SecuredAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_vm || size == 0) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "SecuredAllocator: invalid allocation request");
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        alignment = alignof(MaxAlignT);
    }
    if (alignment < alignof(MaxAlignT)) {
        alignment = alignof(MaxAlignT);
    }

    const size_t pageSize = m_vm->PageSize();
    const size_t alignmentPadding = alignment - 1;
    if (size > (std::numeric_limits<size_t>::max)() - alignmentPadding) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "SecuredAllocator: allocation size overflow");
        return nullptr;
    }

    const size_t committedSize = RoundUpToPage(size + alignmentPadding, pageSize);
    if (committedSize > (std::numeric_limits<size_t>::max)() - (pageSize * 2)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "SecuredAllocator: guarded allocation size overflow");
        return nullptr;
    }

    // Layout: [guard page][committed aligned payload range][guard page].
    const size_t totalSize = committedSize + (pageSize * 2);
    void* base = m_vm->Reserve(totalSize);
    if (!base) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "SecuredAllocator: virtual reserve failed");
        return nullptr;
    }

    void* committedAddress = static_cast<uint8*>(base) + pageSize;
    if (!m_vm->Commit(committedAddress, committedSize)) {
        m_vm->Release(base);
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "SecuredAllocator: virtual commit failed");
        return nullptr;
    }

    void* userPtr = AlignPointer(committedAddress, alignment);
    Threading::SpinLockGuard guard(m_lock);
    m_allocs[userPtr] = { base, committedAddress, totalSize, committedSize, size, alignment };
    m_allocatedTotal += size;
    m_stats.RecordAllocation(size);

    return userPtr;
}

void SecuredAllocator::Free(void* ptr)
{
    if (!ptr) return;

    Threading::SpinLockGuard guard(m_lock);
    const auto it = m_allocs.find(ptr);
    if (it == m_allocs.end()) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "SecuredAllocator: free pointer is not live");
        return;
    }

    m_allocatedTotal -= it->second.userSize;
    m_stats.RecordFree(it->second.userSize);
    m_vm->Release(it->second.baseAddress);
    m_allocs.erase(it);
}

void* SecuredAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (!ptr) return Allocate(newSize, alignment);
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    size_t oldSize = 0;
    {
        Threading::SpinLockGuard guard(m_lock);
        const auto it = m_allocs.find(ptr);
        if (it == m_allocs.end()) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "SecuredAllocator: reallocate pointer is not live");
            return nullptr;
        }
        oldSize = it->second.userSize;
    }

    // Hardened realloc: always move to new address to catch dangling pointers
    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        size_t copySize = oldSize < newSize ? oldSize : newSize;
        MemCopy(newPtr, ptr, copySize);
        Free(ptr);
    } else {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "SecuredAllocator: reallocate allocation failed");
    }
    return newPtr;
}

size_t SecuredAllocator::AllocatedSize() const
{
    Threading::SpinLockGuard guard(m_lock);
    return m_allocatedTotal;
}

bool SecuredAllocator::Owns(void* ptr) const
{
    Threading::SpinLockGuard guard(m_lock);
    return m_allocs.find(ptr) != m_allocs.end();
}

AllocatorStats SecuredAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    return stats;
}

AllocatorStats SecuredAllocator::GetDetailedStats() const
{
    AllocatorStats stats = GetStats();
    Threading::SpinLockGuard guard(m_lock);
    stats.liveBytes = m_allocatedTotal;
    for (const auto& entry : m_allocs) {
        stats.reservedBytes += entry.second.totalSize;
        stats.committedBytes += entry.second.committedSize;
    }
    return stats;
}

void SecuredAllocator::MakeReadOnly(void* ptr)
{
    Threading::SpinLockGuard guard(m_lock);
    const auto it = m_allocs.find(ptr);
    if (it == m_allocs.end()) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "SecuredAllocator: read-only pointer is not live");
        return;
    }

    if (!m_vm->Protect(
            it->second.committedAddress,
            it->second.committedSize,
            IVirtualMemory::MemoryProtection::ReadOnly)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InternalFailure, "SecuredAllocator: read-only protection failed");
    }
}

void SecuredAllocator::MakeReadWrite(void* ptr)
{
    Threading::SpinLockGuard guard(m_lock);
    const auto it = m_allocs.find(ptr);
    if (it == m_allocs.end()) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "SecuredAllocator: read-write pointer is not live");
        return;
    }

    if (!m_vm->Protect(
            it->second.committedAddress,
            it->second.committedSize,
            IVirtualMemory::MemoryProtection::ReadWrite)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InternalFailure, "SecuredAllocator: read-write protection failed");
    }
}

void SecuredAllocator::ScrubAndFree(void* ptr)
{
    size_t userSize = 0;
    void* committedAddress = nullptr;
    size_t committedSize = 0;
    {
        Threading::SpinLockGuard guard(m_lock);
        const auto it = m_allocs.find(ptr);
        if (it == m_allocs.end()) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "SecuredAllocator: scrub pointer is not live");
            return;
        }
        userSize = it->second.userSize;
        committedAddress = it->second.committedAddress;
        committedSize = it->second.committedSize;
    }
    if (!m_vm->Protect(committedAddress, committedSize, IVirtualMemory::MemoryProtection::ReadWrite)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InternalFailure, "SecuredAllocator: scrub protection failed");
        return;
    }
    MemSet(ptr, 0, userSize);
    Free(ptr);
}

} // namespace Engine::Memory
