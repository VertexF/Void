#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/AllocatorDiagnostics.hpp>
#include <Foundation/Memory/Allocators/Advanced/TLSCachingAllocator.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

namespace {

/// @brief Header prepended to each allocation to track size for caching
struct TLSHeader {
    size_t size;
    size_t alignment;
    size_t adjustment;
    uint32 magic;
};

constexpr size_t kHeaderSize = sizeof(TLSHeader);
constexpr uint32 kTLSMagic = 0x544C5343u; // TLSC
constexpr uint32 kTLSCachedMagic = 0x544C5346u; // TLSF

[[nodiscard]] TLSHeader* HeaderFromUserPointer(void* ptr) noexcept
{
    return reinterpret_cast<TLSHeader*>(static_cast<byte*>(ptr) - kHeaderSize);
}

[[nodiscard]] void* RawFromHeader(TLSHeader* header) noexcept
{
    return reinterpret_cast<byte*>(header) + kHeaderSize - header->adjustment;
}

} // anonymous namespace

// Thread-local cache storage
// Each thread gets its own cache instance
thread_local TLSCachingAllocator::ThreadLocalCache t_cache{};

TLSCachingAllocator::TLSCachingAllocator(IAllocator* backingAllocator, size_t cacheSize)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
    , m_cacheSize(cacheSize > kDefaultCacheSize ? kDefaultCacheSize : cacheSize)
{
}

TLSCachingAllocator::~TLSCachingAllocator()
{
    FlushCacheInternal(GetCache());
}

TLSCachingAllocator::ThreadLocalCache& TLSCachingAllocator::GetCache()
{
    return t_cache;
}

void TLSCachingAllocator::FlushCacheInternal(ThreadLocalCache& cache)
{
    // Only flush blocks owned by this allocator
    size_t writeIdx = 0;
    for (size_t i = 0; i < cache.count; ++i) {
        if (cache.blocks[i].owner == this) {
            // Free this block to our backing allocator
            if (cache.blocks[i].ptr) {
                TLSHeader* header = HeaderFromUserPointer(cache.blocks[i].ptr);
                if (header->magic == kTLSCachedMagic) {
                    header->magic = 0;
                }
                m_backingAllocator->Free(RawFromHeader(header));
            }
            cache.blocks[i] = {};
        } else {
            // Keep blocks from other allocators
            if (writeIdx != i) {
                cache.blocks[writeIdx] = cache.blocks[i];
                cache.blocks[i] = {};
            }
            ++writeIdx;
        }
    }
    cache.count = writeIdx;
}

void* TLSCachingAllocator::Allocate(size_t size, size_t alignment)
{
    if (size == 0) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidRequest, "TLSCachingAllocator: zero-size allocation");
        return nullptr;
    }

    auto& cache = GetCache();

    // Fast path: check cache for matching block owned by this allocator
    for (size_t i = 0; i < cache.count; ++i) {
        if (cache.blocks[i].owner == this &&
            cache.blocks[i].size == size &&
            cache.blocks[i].alignment >= alignment) {
            void* ptr = cache.blocks[i].ptr;
            HeaderFromUserPointer(ptr)->magic = kTLSMagic;
#if !defined(NDEBUG)
            RegisterLiveAllocation(ptr);
#endif
            // Swap with last element and remove
            cache.blocks[i] = cache.blocks[cache.count - 1];
            cache.blocks[cache.count - 1] = {};
            --cache.count;
            m_cacheHits.FetchAdd(1, MemoryOrder::Relaxed);
            m_stats.RecordAllocation(size);
            return ptr;
        }
    }

    // Slow path: allocate from backing allocator with header
    m_cacheMisses.FetchAdd(1, MemoryOrder::Relaxed);

    if (!IsPowerOfTwo(alignment)) {
        alignment = alignof(MaxAlignT);
    }
    if (alignment < alignof(MaxAlignT)) {
        alignment = alignof(MaxAlignT);
    }

    const size_t totalSize = size + kHeaderSize + alignment - 1;
    void* raw = m_backingAllocator->Allocate(totalSize, alignof(MaxAlignT));
    if (!raw) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "TLSCachingAllocator: backing allocation failed");
        return nullptr;
    }

    byte* rawBytes = static_cast<byte*>(raw);
    auto* aligned = static_cast<byte*>(AlignPointer(rawBytes + kHeaderSize, alignment));
    auto* header = reinterpret_cast<TLSHeader*>(aligned - kHeaderSize);
    header->size = size;
    header->alignment = alignment;
    header->adjustment = static_cast<size_t>(aligned - rawBytes);
    header->magic = kTLSMagic;
#if !defined(NDEBUG)
    RegisterLiveAllocation(aligned);
#endif

    m_stats.RecordAllocation(size);
    return aligned;
}

void* TLSCachingAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
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
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "TLSCachingAllocator: reallocate pointer is not live");
        return nullptr;
    }
#endif

    auto* header = HeaderFromUserPointer(ptr);
    if (header->magic != kTLSMagic) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "TLSCachingAllocator: reallocate header is not live");
        return nullptr;
    }
    size_t oldSize = header->size;

    if (oldSize == newSize) {
        return ptr;
    }

    // Standard allocate-copy-free fallback
    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        size_t copySize = (oldSize < newSize) ? oldSize : newSize;
        MemCopy(newPtr, ptr, copySize);
        Free(ptr);
    } else {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::OutOfMemory, "TLSCachingAllocator: reallocate allocation failed");
    }
    return newPtr;
}

void TLSCachingAllocator::Free(void* ptr)
{
    if (!ptr) {
        return;
    }

#if !defined(NDEBUG)
    if (!UnregisterLiveAllocation(ptr)) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::DoubleFree, "TLSCachingAllocator: invalid or double free");
        return;
    }
#endif

    // Read header to get size
    auto* header = HeaderFromUserPointer(ptr);
    if (header->magic != kTLSMagic) {
        ReportAllocatorFailure(m_stats, AllocatorFailureKind::InvalidPointer, "TLSCachingAllocator: free header is not live");
        return;
    }
    size_t size = header->size;
    m_stats.RecordFree(size);

    auto& cache = GetCache();

    // Count how many blocks are owned by this allocator
    size_t myBlockCount = 0;
    for (size_t i = 0; i < cache.count; ++i) {
        if (cache.blocks[i].owner == this) {
            ++myBlockCount;
        }
    }

    // If this allocator's cache has room AND there's physical space, store the block
    if (myBlockCount < m_cacheSize && cache.count < kDefaultCacheSize) {
        header->magic = kTLSCachedMagic;
        cache.blocks[cache.count].ptr = ptr;
        cache.blocks[cache.count].size = size;
        cache.blocks[cache.count].alignment = header->alignment;
        cache.blocks[cache.count].owner = this;
        ++cache.count;
        return;
    }

    // Cache full - free directly to backing allocator
    header->magic = 0;
    m_backingAllocator->Free(RawFromHeader(header));
}

size_t TLSCachingAllocator::AllocatedSize() const
{
    return m_backingAllocator->AllocatedSize();
}

const char* TLSCachingAllocator::Name() const
{
    return "TLSCachingAllocator";
}

bool TLSCachingAllocator::Owns(void* ptr) const
{
    if (!ptr) {
        return false;
    }
#if !defined(NDEBUG)
    Threading::SpinLockGuard guard(m_liveLock);
    return m_liveAllocations.find(ptr) != m_liveAllocations.end();
#else
    auto* header = HeaderFromUserPointer(ptr);
    return header->magic == kTLSMagic;
#endif
}

AllocatorStats TLSCachingAllocator::GetStats() const
{
    AllocatorStats stats = m_stats.Snapshot(Name());
    if (m_backingAllocator) {
        const AllocatorStats backing = m_backingAllocator->GetStats();
        stats.reservedBytes = backing.reservedBytes;
        stats.committedBytes = backing.committedBytes;
        stats.freeBytes = backing.freeBytes;
        stats.largestFreeBlockBytes = backing.largestFreeBlockBytes;
        stats.fragmentationBytes = backing.fragmentationBytes;
    }
    return stats;
}

void TLSCachingAllocator::FlushCache()
{
    FlushCacheInternal(GetCache());
}

#if !defined(NDEBUG)
bool TLSCachingAllocator::UnregisterLiveAllocation(void* ptr)
{
    Threading::SpinLockGuard guard(m_liveLock);
    const auto it = m_liveAllocations.find(ptr);
    if (it == m_liveAllocations.end()) {
        return false;
    }
    m_liveAllocations.erase(it);
    return true;
}

void TLSCachingAllocator::RegisterLiveAllocation(void* ptr)
{
    if (!ptr) {
        return;
    }
    Threading::SpinLockGuard guard(m_liveLock);
    m_liveAllocations.insert(ptr);
}
#endif

} // namespace Engine::Memory
