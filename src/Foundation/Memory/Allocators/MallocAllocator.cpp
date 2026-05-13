#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

namespace {
constexpr size_t kMallocMagic = 0x4D414C43u; // MALC
}

void* MallocAllocator::Allocate(size_t size, size_t alignment)
{
    if (size == 0) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "MallocAllocator: zero-size allocation");
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        alignment = alignof(MaxAlignT);
    }

    const size_t headerSize = sizeof(AllocationHeader);
    const size_t totalSize = size + headerSize + alignment;

    void* raw = malloc(totalSize);
    if (!raw) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "MallocAllocator: allocation failed");
        return nullptr;
    }

    byte* rawBytes = static_cast<byte*>(raw);
    byte* aligned = static_cast<byte*>(AlignPointer(rawBytes + headerSize, alignment));
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - headerSize);
    header->size = size;
    header->adjustment = static_cast<size_t>(aligned - rawBytes);
    header->tag = GetCurrentMemoryTag();
    header->padding = kMallocMagic;

    (void)m_allocated.FetchAdd(size, MemoryOrder::Relaxed);
    m_stats.RecordAllocation(size);

    if (!MemoryManager::IsProfilingSuppressed()) {
        if (MemoryProfiler* profiler = MemoryManager::Profiler()) {
            profiler->TrackAlloc(aligned, size, GetCurrentMemoryTag());
        }
    }

#if !defined(NDEBUG)
    {
        MemoryProfilingSuppressionScope suppressMetadataProfile;
        Threading::SpinLockGuard guard(m_liveLock);
        m_liveAllocations.insert(aligned);
    }
#endif

    return aligned;
}

void* MallocAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }
#if !defined(NDEBUG)
    if (!Owns(ptr)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "MallocAllocator: reallocate pointer is not live");
        return nullptr;
    }
#endif

    byte* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));
    size_t oldSize = header->size;

    if (oldSize == newSize) {
        return ptr;
    }

    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        size_t copySize = (oldSize < newSize) ? oldSize : newSize;
        MemCopy(newPtr, ptr, copySize);
        Free(ptr);
    }
    if (!newPtr) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "MallocAllocator: reallocate allocation failed");
    }
    return newPtr;
}

void MallocAllocator::Free(void* ptr)
{
    if (!ptr) {
        return;
    }

#if !defined(NDEBUG)
    {
        MemoryProfilingSuppressionScope suppressMetadataProfile;
        Threading::SpinLockGuard guard(m_liveLock);
        const auto it = m_liveAllocations.find(ptr);
        if (it == m_liveAllocations.end()) {
            ReportAllocatorFailure(m_stats, AllocatorFailureKind::DoubleFree, "MallocAllocator: invalid or double free");
            return;
        }
        m_liveAllocations.erase(it);
    }
#endif

    byte* aligned = static_cast<byte*>(ptr);
    auto* header = reinterpret_cast<AllocationHeader*>(aligned - sizeof(AllocationHeader));
    if (header->padding != kMallocMagic) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::Corruption, "MallocAllocator: allocation header corrupted");
        return;
    }
    size_t size = header->size;

    (void)m_allocated.FetchSub(size, MemoryOrder::Relaxed);
    m_stats.RecordFree(size);

    if (!MemoryManager::IsProfilingSuppressed()) {
        if (MemoryProfiler* profiler = MemoryManager::Profiler()) {
            profiler->TrackFree(ptr, size, GetCurrentMemoryTag());
        }
    }

    byte* raw = aligned - header->adjustment;
    header->padding = 0;
    free(raw);
}

size_t MallocAllocator::AllocatedSize() const
{
    return m_allocated.Load(MemoryOrder::Relaxed);
}

const char* MallocAllocator::Name() const
{
    return "MallocAllocator";
}

bool MallocAllocator::Owns(void* ptr) const
{
    if (!ptr) {
        return false;
    }
#if !defined(NDEBUG)
    MemoryProfilingSuppressionScope suppressMetadataProfile;
    Threading::SpinLockGuard guard(m_liveLock);
    return m_liveAllocations.find(ptr) != m_liveAllocations.end();
#else
    const auto* header = reinterpret_cast<const AllocationHeader*>(static_cast<const byte*>(ptr) - sizeof(AllocationHeader));
    return header->padding == kMallocMagic;
#endif
}

AllocatorStats MallocAllocator::GetStats() const
{
    return m_stats.Snapshot(Name());
}

} // namespace Engine::Memory
