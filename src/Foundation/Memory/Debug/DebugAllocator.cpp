#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/Debug/DebugAllocator.hpp>
#include <Foundation/Memory/Debug/LeakDetector.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Utility/Assert.hpp>
#include <Foundation/Threading/Atomic.hpp>

#include <cstdio>

namespace Engine::Memory {

namespace {
constexpr uint8 kAllocPattern = 0xCDu;
constexpr uint8 kFreePattern = 0xDDu;
constexpr uint8 kPreGuardPattern = 0xAAu;
constexpr uint8 kPostGuardPattern = 0xBBu;
constexpr size_t kGuardSize = 16;
constexpr uint32 kDebugMagic = 0x44454247u; // DEBG

struct DebugHeader {
    size_t size = 0;
    size_t adjustment = 0;
    MemoryTag tag = MemoryTag::Default;
    uint32 magic = kDebugMagic;
};

[[nodiscard]] DebugHeader* HeaderFromUserPointer(void* ptr) noexcept
{
    return reinterpret_cast<DebugHeader*>(static_cast<byte*>(ptr) - kGuardSize - sizeof(DebugHeader));
}
}

DebugAllocator::DebugAllocator(IAllocator* backingAllocator,
                               LeakDetector* leakDetector,
                               MemoryProfiler* profiler)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator()),
      m_leakDetector(leakDetector),
      m_profiler(profiler)
{
}

void* DebugAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_backingAllocator || size == 0) {
        return nullptr;
    }

    const MemoryTag tag = GetCurrentMemoryTag();

    (void)MemoryManager::ReportBudgetPressure(tag, size);

    if (!IsPowerOfTwo(alignment)) {
        alignment = alignof(MaxAlignT);
    }

    const size_t headerSize = sizeof(DebugHeader);
    const size_t totalSize = headerSize + kGuardSize * 2 + size + alignment;
    void* raw = nullptr;
    {
        MemoryProfilingSuppressionScope suppressBackingProfile;
        raw = m_backingAllocator->Allocate(totalSize, alignof(MaxAlignT));
    }
    if (!raw) {
        return nullptr;
    }

    byte* rawBytes = static_cast<byte*>(raw);
    // User pointer must be aligned. We need space for Header and Pre-Guard before it.
    byte* aligned = static_cast<byte*>(AlignPointer(rawBytes + headerSize + kGuardSize, alignment));
    
    auto* header = reinterpret_cast<DebugHeader*>(aligned - kGuardSize - headerSize);
    header->size = size;
    header->adjustment = static_cast<size_t>(aligned - rawBytes);
    header->tag = tag;
    header->magic = kDebugMagic;

    // Fill guards
    MemSet(aligned - kGuardSize, kPreGuardPattern, kGuardSize);
    MemSet(aligned + size, kPostGuardPattern, kGuardSize);

    // Fill user memory
    MemSet(aligned, kAllocPattern, size);

    m_allocatedBytes.FetchAdd(size, MemoryOrder::Relaxed);
    if (m_leakDetector) {
        m_leakDetector->TrackAlloc(aligned, size, header->tag);
    }
    if (m_profiler) {
        m_profiler->TrackAlloc(aligned, size, header->tag);
    }
    {
        Threading::SpinLockGuard guard(m_liveLock);
        m_liveAllocations.insert(aligned);
    }
    return aligned;
}

void* DebugAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (ptr == nullptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!Owns(ptr)) {
        return nullptr;
    }

    byte* aligned = static_cast<byte*>(ptr);
    auto* header = HeaderFromUserPointer(ptr);
    size_t oldSize = header->size;

    if (oldSize == newSize) {
        return ptr;
    }

    // Standard allocate-copy-free fallback. 
    // This naturally handles guards, pattern filling, and tracking.
    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        size_t copySize = (oldSize < newSize) ? oldSize : newSize;
        MemCopy(newPtr, ptr, copySize);
        Free(ptr);
    }
    return newPtr;
}

void DebugAllocator::Free(void* ptr)
{
    if (!m_backingAllocator || !ptr) {
        return;
    }

    {
        Threading::SpinLockGuard guard(m_liveLock);
        const auto it = m_liveAllocations.find(ptr);
        if (it == m_liveAllocations.end()) {
            return;
        }
        m_liveAllocations.erase(it);
    }

    byte* aligned = static_cast<byte*>(ptr);
    auto* header = HeaderFromUserPointer(ptr);
    if (header->magic != kDebugMagic) {
        return;
    }
    const size_t size = header->size;
    const MemoryTag tag = header->tag;

    // Check pre-guard
    for (size_t i = 0; i < kGuardSize; ++i) {
        if (static_cast<uint8>((aligned - kGuardSize)[i]) != kPreGuardPattern) {
            ENGINE_ASSERT_MSG(false, "Memory corruption detected: Pre-guard overwritten!");
            fprintf(stderr, "[MAL] Memory corruption detected: Pre-guard overwritten at %p\n", ptr);
            break;
        }
    }

    // Check post-guard
    for (size_t i = 0; i < kGuardSize; ++i) {
        if (static_cast<uint8>((aligned + size)[i]) != kPostGuardPattern) {
            ENGINE_ASSERT_MSG(false, "Memory corruption detected: Post-guard overwritten!");
            fprintf(stderr, "[MAL] Memory corruption detected: Post-guard overwritten at %p\n", ptr);
            break;
        }
    }

    MemSet(aligned, kFreePattern, size);
    m_allocatedBytes.FetchSub(size, MemoryOrder::Relaxed);
    if (m_leakDetector) {
        m_leakDetector->TrackFree(aligned);
    }
    if (m_profiler) {
        m_profiler->TrackFree(aligned, size, tag);
    }

    byte* raw = aligned - header->adjustment;
    {
        MemoryProfilingSuppressionScope suppressBackingProfile;
        m_backingAllocator->Free(raw);
    }
}

size_t DebugAllocator::AllocatedSize() const
{
    return m_allocatedBytes.Load(MemoryOrder::Relaxed);
}

const char* DebugAllocator::Name() const
{
    return "DebugAllocator";
}

bool DebugAllocator::Owns(void* ptr) const
{
    if (!ptr) {
        return false;
    }
    Threading::SpinLockGuard guard(m_liveLock);
    return m_liveAllocations.find(ptr) != m_liveAllocations.end();
}

} // namespace Engine::Memory
