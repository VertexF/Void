#ifndef FOUNDATION_MEMORY_BINNED_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_BINNED_ALLOCATOR_HDR

#include <array>
#include <unordered_set>

#include <Foundation/Memory/Binned/Bin.hpp>
#include <Foundation/Memory/Binned/SizeClassTable.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Threading/Lock/SpinLock.hpp>
#include <Foundation/Threading/Atomic.hpp>

namespace Engine::Memory {

class CoreHeap;
struct Bin;

class BinnedAllocator final : public IAllocator {
public:
    explicit BinnedAllocator(IAllocator* backingAllocator = nullptr);
    ~BinnedAllocator() override;

    BinnedAllocator(const BinnedAllocator&) = delete;
    BinnedAllocator& operator=(const BinnedAllocator&) = delete;

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) override;
    [[nodiscard]] void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) override;
    void Free(void* ptr) override;
    [[nodiscard]] size_t AllocatedSize() const override;
    [[nodiscard]] const char* Name() const override;
    [[nodiscard]] bool Owns(void* ptr) const override;
    [[nodiscard]] AllocatorStats GetStats() const override;
    [[nodiscard]] AllocatorStats GetDetailedStats() const override;

    [[nodiscard]] void* GetBaseAddress() const noexcept { return m_baseAddress; }
    void RefillBin(Bin& bin, uint32 sizeClass);
    [[nodiscard]] CoreHeap& GetCurrentCoreHeap() noexcept;
    static void ResetTLS() {}

private:
    static constexpr size_t kPageSize = 64ull * 1024ull;
    static constexpr uint32 kMagic = 0x42494E44u; // BIND

    struct Page;

    struct AllocationHeader {
        uint32 magic = kMagic;
        uint32 flags = 0;
        size_t requestedSize = 0;
        size_t blockSize = 0;
        void* raw = nullptr;
        Page* page = nullptr;
    };

    struct Page {
        void* memory = nullptr;
        Bin bin{};
        Page* next = nullptr;
        uint32 sizeClass = SizeClassTable::kInvalidIndex;
        uint32 activeAllocations = 0;
        size_t blockSize = 0;
    };

    [[nodiscard]] void* AllocateSmall(size_t size, size_t alignment);
    [[nodiscard]] void* AllocateLarge(size_t size, size_t alignment);
    [[nodiscard]] Page* AllocatePage(uint32 sizeClass, size_t blockSize);
    void ReleasePage(Page* page);
    void RemovePage(Page* page);
    [[nodiscard]] Page* FindPageContainingPointer(void* ptr) const noexcept;
    [[nodiscard]] bool IsPointerInPageBlock(const Page& page, void* ptr, const AllocationHeader& header) const noexcept;
    [[nodiscard]] static AllocationHeader* HeaderFromPointer(void* ptr) noexcept;
    [[nodiscard]] static size_t RequiredBlockSize(size_t size, size_t alignment) noexcept;

    IAllocator* m_backingAllocator = nullptr;
    SizeClassTable m_sizeClasses;
    std::array<Page*, SizeClassTable::kNumSizeClasses> m_pages{};
    Page* m_externalPages = nullptr;
    std::unordered_set<void*> m_largeAllocations;
    void* m_baseAddress = nullptr;
    Atomic<size_t> m_allocatedBytes{0};
    AllocatorStatsTracker m_stats;
    mutable Threading::SpinLock m_lock;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_BINNED_ALLOCATOR_HDR
