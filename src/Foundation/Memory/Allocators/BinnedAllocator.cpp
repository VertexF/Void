#include <Foundation/Memory/Allocators/BinnedAllocator.hpp>
#include <Foundation/Memory/Binned/CoreHeap.hpp>
#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/Operations.hpp>

namespace Engine::Memory {

namespace {
constexpr uint32 kLargeAllocationFlag = 1u;
}

BinnedAllocator::BinnedAllocator(IAllocator* backingAllocator)
    : m_backingAllocator(backingAllocator ? backingAllocator : &GetDefaultAllocator())
{
    m_pages.fill(nullptr);
}

BinnedAllocator::~BinnedAllocator()
{
    for (Page*& head : m_pages) {
        Page* page = head;
        while (page) {
            Page* next = page->next;
            ReleasePage(page);
            page = next;
        }
        head = nullptr;
    }
    Page* external = m_externalPages;
    while (external) {
        Page* next = external->next;
        ReleasePage(external);
        external = next;
    }
    m_externalPages = nullptr;
    m_baseAddress = nullptr;
}

void* BinnedAllocator::Allocate(size_t size, size_t alignment)
{
    if (!m_backingAllocator || size == 0) {
        return nullptr;
    }

    if (!IsPowerOfTwo(alignment)) {
        alignment = alignof(MaxAlignT);
    }
    if (alignment < alignof(MaxAlignT)) {
        alignment = alignof(MaxAlignT);
    }

    const size_t requiredSize = RequiredBlockSize(size, alignment);
    if (requiredSize <= SizeClassTable::kMaxSize) {
        return AllocateSmall(size, alignment);
    }
    return AllocateLarge(size, alignment);
}

void* BinnedAllocator::AllocateSmall(size_t size, size_t alignment)
{
    const size_t requiredSize = RequiredBlockSize(size, alignment);
    const uint32 sizeClass = m_sizeClasses.GetSizeClass(requiredSize);
    if (sizeClass == SizeClassTable::kInvalidIndex) {
        return AllocateLarge(size, alignment);
    }

    const size_t blockSize = m_sizeClasses.GetBlockSize(sizeClass);
    Threading::SpinLockGuard guard(m_lock);

    Page* page = m_pages[sizeClass];
    while (page && !page->bin.HasFreeBlocks()) {
        page = page->next;
    }

    if (!page) {
        page = AllocatePage(sizeClass, blockSize);
        if (!page) {
            return nullptr;
        }
        page->next = m_pages[sizeClass];
        m_pages[sizeClass] = page;
    }

    void* block = page->bin.Allocate();
    if (!block) {
        return nullptr;
    }

    auto* blockBytes = static_cast<byte*>(block);
    auto* user = static_cast<byte*>(AlignPointer(blockBytes + sizeof(AllocationHeader), alignment));
    auto* header = reinterpret_cast<AllocationHeader*>(user - sizeof(AllocationHeader));
    header->magic = kMagic;
    header->flags = 0;
    header->requestedSize = size;
    header->blockSize = blockSize;
    header->raw = block;
    header->page = page;

    ++page->activeAllocations;
    m_allocatedBytes.FetchAdd(size, MemoryOrder::Relaxed);
    return user;
}

void* BinnedAllocator::AllocateLarge(size_t size, size_t alignment)
{
    const size_t requiredSize = RequiredBlockSize(size, alignment);
    void* raw = m_backingAllocator->Allocate(requiredSize, alignof(MaxAlignT));
    if (!raw) {
        return nullptr;
    }

    auto* rawBytes = static_cast<byte*>(raw);
    auto* user = static_cast<byte*>(AlignPointer(rawBytes + sizeof(AllocationHeader), alignment));
    auto* header = reinterpret_cast<AllocationHeader*>(user - sizeof(AllocationHeader));
    header->magic = kMagic;
    header->flags = kLargeAllocationFlag;
    header->requestedSize = size;
    header->blockSize = requiredSize;
    header->raw = raw;
    header->page = nullptr;

    {
        MemoryProfilingSuppressionScope suppressMetadataProfile;
        Threading::SpinLockGuard guard(m_lock);
        m_largeAllocations.insert(user);
    }

    m_allocatedBytes.FetchAdd(size, MemoryOrder::Relaxed);
    return user;
}

void* BinnedAllocator::Reallocate(void* ptr, size_t newSize, size_t alignment)
{
    if (!ptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Free(ptr);
        return nullptr;
    }

    if (!Owns(ptr)) {
        return nullptr;
    }

    AllocationHeader* header = HeaderFromPointer(ptr);
    const size_t oldSize = header->requestedSize;
    if (newSize <= oldSize && RequiredBlockSize(newSize, alignment) <= header->blockSize) {
        header->requestedSize = newSize;
        m_allocatedBytes.FetchSub(oldSize - newSize, MemoryOrder::Relaxed);
        return ptr;
    }

    void* newPtr = Allocate(newSize, alignment);
    if (newPtr) {
        MemCopy(newPtr, ptr, oldSize < newSize ? oldSize : newSize);
        Free(ptr);
    }
    return newPtr;
}

void BinnedAllocator::Free(void* ptr)
{
    if (!ptr || !m_backingAllocator) {
        return;
    }

    void* rawToFree = nullptr;
    size_t size = 0;
    {
        MemoryProfilingSuppressionScope suppressMetadataProfile;
        Threading::SpinLockGuard guard(m_lock);
        AllocationHeader* header = HeaderFromPointer(ptr);
        if (!header || header->magic != kMagic) {
            return;
        }

        const auto largeIt = m_largeAllocations.find(ptr);
        if (largeIt != m_largeAllocations.end()) {
            if ((header->flags & kLargeAllocationFlag) == 0) {
                return;
            }
            m_largeAllocations.erase(largeIt);
            size = header->requestedSize;
            rawToFree = header->raw;
            header->magic = 0;
        } else {
            Page* page = FindPageContainingPointer(ptr);
            if (!page || header->page != page || !IsPointerInPageBlock(*page, ptr, *header)) {
                return;
            }
            size = header->requestedSize;
            header->magic = 0;
            page->bin.Free(header->raw);
            if (page->activeAllocations > 0) {
                --page->activeAllocations;
            }
        }
    }

    m_allocatedBytes.FetchSub(size, MemoryOrder::Relaxed);

    if (rawToFree) {
        m_backingAllocator->Free(rawToFree);
    }
}

size_t BinnedAllocator::AllocatedSize() const
{
    return m_allocatedBytes.Load(MemoryOrder::Relaxed);
}

const char* BinnedAllocator::Name() const
{
    return "BinnedAllocator";
}

bool BinnedAllocator::Owns(void* ptr) const
{
    if (!ptr) {
        return false;
    }
    MemoryProfilingSuppressionScope suppressMetadataProfile;
    Threading::SpinLockGuard guard(m_lock);
    if (m_largeAllocations.find(ptr) != m_largeAllocations.end()) {
        const AllocationHeader* header = HeaderFromPointer(ptr);
        return header && header->magic == kMagic && (header->flags & kLargeAllocationFlag) != 0;
    }

    Page* page = FindPageContainingPointer(ptr);
    if (!page) {
        return false;
    }

    const AllocationHeader* header = HeaderFromPointer(ptr);
    return header &&
        header->magic == kMagic &&
        header->page == page &&
        IsPointerInPageBlock(*page, ptr, *header);
}

void BinnedAllocator::RefillBin(Bin& bin, uint32 sizeClass)
{
    const size_t blockSize = m_sizeClasses.GetBlockSize(sizeClass);
    if (blockSize == 0 || !m_backingAllocator) {
        return;
    }

    Threading::SpinLockGuard guard(m_lock);
    Page* page = AllocatePage(sizeClass, blockSize);
    if (!page) {
        return;
    }
    bin = page->bin;
    page->next = m_externalPages;
    m_externalPages = page;
}

CoreHeap& BinnedAllocator::GetCurrentCoreHeap() noexcept
{
    static CoreHeap heap;
    return heap;
}

BinnedAllocator::Page* BinnedAllocator::AllocatePage(uint32 sizeClass, size_t blockSize)
{
    void* memory = m_backingAllocator->Allocate(kPageSize, alignof(MaxAlignT));
    if (!memory) {
        return nullptr;
    }

    void* pageMemory = m_backingAllocator->Allocate(sizeof(Page), alignof(Page));
    if (!pageMemory) {
        m_backingAllocator->Free(memory);
        return nullptr;
    }

    auto* page = new (pageMemory) Page{};
    page->memory = memory;
    if (!m_baseAddress) {
        m_baseAddress = memory;
    }
    page->sizeClass = sizeClass;
    page->blockSize = blockSize;
    page->bin.Initialize(memory, kPageSize, static_cast<uint32>(blockSize));
    return page;
}

void BinnedAllocator::ReleasePage(Page* page)
{
    if (!page) {
        return;
    }
    void* memory = page->memory;
    page->~Page();
    m_backingAllocator->Free(page);
    m_backingAllocator->Free(memory);
}

void BinnedAllocator::RemovePage(Page* page)
{
    Page** current = &m_pages[page->sizeClass];
    while (*current) {
        if (*current == page) {
            *current = page->next;
            page->next = nullptr;
            return;
        }
        current = &((*current)->next);
    }
}

BinnedAllocator::Page* BinnedAllocator::FindPageContainingPointer(void* ptr) const noexcept
{
    if (!ptr) {
        return nullptr;
    }

    const auto* bytes = static_cast<const byte*>(ptr);
    for (Page* head : m_pages) {
        for (Page* page = head; page; page = page->next) {
            const auto* begin = static_cast<const byte*>(page->memory);
            const auto* end = begin + kPageSize;
            if (bytes >= begin && bytes < end) {
                return page;
            }
        }
    }

    for (Page* page = m_externalPages; page; page = page->next) {
        const auto* begin = static_cast<const byte*>(page->memory);
        const auto* end = begin + kPageSize;
        if (bytes >= begin && bytes < end) {
            return page;
        }
    }

    return nullptr;
}

bool BinnedAllocator::IsPointerInPageBlock(const Page& page, void* ptr, const AllocationHeader& header) const noexcept
{
    if (!page.memory || !header.raw || page.blockSize == 0) {
        return false;
    }

    const auto* begin = static_cast<const byte*>(page.memory);
    const auto* raw = static_cast<const byte*>(header.raw);
    const auto* user = static_cast<const byte*>(ptr);
    const auto* end = begin + kPageSize;
    if (raw < begin || raw >= end || user < raw || user >= raw + page.blockSize) {
        return false;
    }

    const size_t blockOffset = static_cast<size_t>(raw - begin);
    return (blockOffset % page.blockSize) == 0;
}

BinnedAllocator::AllocationHeader* BinnedAllocator::HeaderFromPointer(void* ptr) noexcept
{
    if (!ptr) {
        return nullptr;
    }
    return reinterpret_cast<AllocationHeader*>(static_cast<byte*>(ptr) - sizeof(AllocationHeader));
}

size_t BinnedAllocator::RequiredBlockSize(size_t size, size_t alignment) noexcept
{
    return sizeof(AllocationHeader) + size + alignment - 1;
}

} // namespace Engine::Memory
