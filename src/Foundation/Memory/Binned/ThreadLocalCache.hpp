#ifndef FOUNDATION_MEMORY_THREAD_LOCAL_CACHE_HDR
#define FOUNDATION_MEMORY_THREAD_LOCAL_CACHE_HDR

#include <Foundation/Memory/Binned/Bin.hpp>
#include <Foundation/Memory/Binned/SizeClassTable.hpp>
#include <Foundation/Memory/Binned/PageHeap.hpp>
#include <Foundation/Platform.hpp>

namespace Engine::Memory {

struct ThreadLocalCache {
    // One bin per size class
    Bin bins[SizeClassTable::kNumSizeClasses];
    PageHeap::PageHeader* currentPages[SizeClassTable::kNumSizeClasses];
    
    // Per-thread statistics (optional)
    size_t allocatedBytes = 0;
    size_t allocationCount = 0;

    void Initialize() noexcept
    {
        for (uint32 i = 0; i < SizeClassTable::kNumSizeClasses; ++i) {
            bins[i].Reset();
            currentPages[i] = nullptr;
        }
        allocatedBytes = 0;
        allocationCount = 0;
    }
};

} // namespace Engine::Memory
#endif // !FOUNDATION_MEMORY_THREAD_LOCAL_CACHE_HDR