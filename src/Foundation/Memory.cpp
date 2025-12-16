#include "Memory.hpp"
#include "Assert.hpp"

#include <tlsf.h>

#include <stdlib.h>
#include <memory.h>

#if defined VOID_IMGUI
    #include <vender/imgui/imgui.h>
#endif

#if defined(_MSC_VER)
//Define this and add StackWalker to heavy memory profile.
//Also note this library seems to be windows only.
#define VOID_MEMORY_STACK
#endif

#define HEAP_ALLOCATOR_STATS

#if defined(VOID_MEMORY_STACK)
    #include <StackWalker.h>
#endif //VOID_MEMORY_STACK

#define VOID_MEMORY_DEBUG

#if defined(WIN32)
    #if defined(VOID_MEMORY_DEBUG)
        #define VOID_MEM_ASSERT(condition, message, ...) VOID_ASSERTM(condition, message, __VA_ARGS__)
    #else
        #define VOID_MEM_ASSERT(condition, message, ...)
    #endif //VOID_MEMORY_DEBUG
#elif(__linux__)
    #if defined(VOID_MEMORY_DEBUG)
        #define VOID_MEM_ASSERT(condition, message, ...) VOID_ASSERTM(condition, message, ##__VA_ARGS__)
    #else
        #define VOID_MEM_ASSERT(condition, message, ...)
    #endif //VOID_MEMORY_DEBUG
#endif

static size_t MEMORY_SIZE = void_mega(32) + tlsf_size() + 8;

static void exitWalker(void* ptr, size_t size, int used, void* user);
static void imguiWalker(void* ptr, size_t size, int used, void* user);

MemoryService* MemoryService::instance()
{
    static MemoryService memoryService;
    return &memoryService;
}

void MemoryService::init(void* configuration)
{
    vprint("Memory Service Init.\n");
    MemoryServiceConfiguration* memoryConfiguration = static_cast<MemoryServiceConfiguration*>(configuration);
    systemAllocator.init(memoryConfiguration ? memoryConfiguration->maximumDynamicSize : MEMORY_SIZE);
}

void MemoryService::shutdown()
{
    systemAllocator.shutdown();

    vprint("Memory Service Shutdown.\n");
}

void exitWalker(void* ptr, size_t size, int used, void* user)
{
    MemoryStatistics* stats = (MemoryStatistics*)user;
    stats->add(used ? size : 0);

    if (used)
    {
        vprint("Found active allocation %p %llu\n", ptr, size);

        uint32_t* value = (uint32_t*)ptr;
        vprint("What is the value in 8 bytes: %u\n", value);
    }
}

#if defined(VOID_IMGUI)
void imguiWalker(void* ptr, size_t size, int used, void* user)
{
    uint32_t memorySize = (uint32_t)size;
    //Memory units as in megabytes or petabytes
    const char* memoryUnit = "b";
    if (memorySize > 1024 * 1024)
    {
        memorySize /= 1024 * 1024;
        memoryUnit = "Mb";
    }
    else if (memorySize > 1024)
    {
        memorySize /= 1024;
        memoryUnit = "Kb";
    }
    ImGui::Text("\t%p %s size: %4llu %s\n", ptr, used ? "used" : "free", memorySize, memoryUnit);

    MemoryStatistics* stats = (MemoryStatistics*)user;
    stats->add(used ? size : 0);
}

void MemoryService::imguiDraw()
{
    if (ImGui::Begin("Memory Service"))
    {
        systemAllocator.debugUI();
    }
    ImGui::End();
}
#endif //void_IMGUI

void MemoryService::test()
{
    //TODO Write and test the memory allocators and fix what the API is vs what's in the code.
    //static LinearAllocator linear;
    //linear.init(1024);
}

void HeapAllocator::init(size_t size)
{
    memory = malloc(size);
    maxSize = size;
    allocatedSize = 0;

    memset(memory, 0, size);

    TLSFHandle = tlsf_create_with_pool(memory, size);
    vprint("HeapAllocator of size %llu created.\n", size);
}

void HeapAllocator::shutdown()
{
    MemoryStatistics stats{ 0, maxSize };
    pool_t pool = tlsf_get_pool(TLSFHandle);
    tlsf_walk_pool(pool, exitWalker, (void*)&stats);

    if (stats.allocatedBytes)
    {
        vprint("HeapAllocator Shutdown.\n=========\nFAILURE! Allocated memory detected. Allocated %llu, total %llu\n=========\n", stats.allocatedBytes, stats.totalBytes);
    }
    else
    {
        vprint("HeapAllocator Shutdown - all memory freed.\n");
    }

    VOID_ASSERTM(stats.allocatedBytes == 0, "Allocated bytes are still present.\n");

    tlsf_destroy(TLSFHandle);

    free(memory);
}

#if defined VOID_IMGUI
void HeapAllocator::debugUI()
{
    ImGui::Separator();
    ImGui::Text("Heap Allocator");
    ImGui::Separator();
    MemoryStatistics stats{ 0, maxSize };
    pool_t pool = tlsf_get_pool(TLSFHandle);
    tlsf_walk_pool(pool, imguiWalker, (void*)&stats);

    ImGui::Separator();
    ImGui::Text("\tAllocation count %d", stats.allocationCount);
    ImGui::Text("\tAllocated %llu K, free %llu Mb", stats.allocatedBytes / (1024 * 1024),
        maxSize - stats.allocatedBytes / (1024 * 1024),
        maxSize / (1024 * 1024));
}
#endif //VOID_IMGUI

#if defined (VOID_MEMORY_STACK)
class VoidStackWalker : public StackWalker
{
public:
    virtual ~VoidStackWalker() override = default;

    VoidStackWalker() : StackWalker()
    {
    }

protected:
    virtual void OnOutput(LPCSTR szText) override
    {
        vprint("\nStackL \n%s\n", szText);
        StackWalker::OnOutput(szText);
    }
};

void* HeapAllocator::allocate(size_t size, size_t alignment)
{
    void* memory = tlsf_malloc(TLSFHandle, size);
    vprint("Memory: %p, size %llu \n", memory, size);
    return memory;
}
#else
void* HeapAllocator::allocate(size_t size, size_t alignment)
{
#if defined(HEAP_ALLOCATOR_STATS)
    void* allocatedMemory = alignment == 1 ? tlsf_malloc(TLSFHandle, size) : tlsf_memalign(TLSFHandle, alignment, size);
    size_t actualSize = tlsf_block_size(allocatedMemory);
    allocatedSize += actualSize;

    return allocatedMemory;
#else
    return tlsf_malloc(TLSFHandle, size);
#endif
}
#endif //VOID_MEMORY_STACK

void* HeapAllocator::allocate(size_t size, size_t alignment, const char* file, int32_t line)
{
    return allocate(size, alignment);
}

void HeapAllocator::deallocate(void* pointer)
{
#if defined (HEAP_ALLOCATOR_STATS)
    size_t actualSize = tlsf_block_size(pointer);
    allocatedSize -= actualSize;

    tlsf_free(TLSFHandle, pointer);
#else
    tlsf_free(TLSFHandle, pointer);
#endif
}

void LinearAllocator::init(size_t size)
{
    memory = (uint8_t*)malloc(size);
    totalSize = size;
    allocatedSize = 0;
}

void LinearAllocator::shutdown()
{
    clear();
    free(memory);
}

void* LinearAllocator::allocate(size_t size, size_t alignment)
{
    VOID_ASSERT(size > 0);

    const size_t newStart = memoryAlign(allocatedSize, alignment);
    VOID_ASSERT(newStart < totalSize);
    const size_t newAllocatedSize = newStart + size;
    if (newAllocatedSize > totalSize)
    {
        VOID_MEM_ASSERT(false, "Overflow");
        return nullptr;
    }

    allocatedSize = newAllocatedSize;
    return memory + newStart;
}

void* LinearAllocator::allocate(size_t size, size_t alignment, const char* file, int32_t line)
{
    return allocate(size, alignment);
}

void LinearAllocator::deallocate(void* /*pointer*/)
{
    //This allocator does not allocate on a per pointer bases.
}

void LinearAllocator::clear() 
{
    allocatedSize = 0;
}

void memoryCopy(void* destination, void* source, size_t size) 
{
    memcpy(destination, source, size);
}

size_t memoryAlign(size_t size, size_t alignment) 
{
    const size_t alignmentMask = alignment - 1;
    return (size + alignmentMask) & ~alignmentMask;
}

void* MallocAllocator::allocate(size_t size, size_t alignment) 
{
    return malloc(size);
}

void* MallocAllocator::allocate(size_t size, size_t alignment, const char* file, int32_t line) 
{
    return malloc(size);
}

void MallocAllocator::deallocate(void* pointer) 
{
    free(pointer);
}

void StackAllocator::init(size_t size) 
{
    memory = (uint8_t*)malloc(size);
    allocatedSize = 0;
    totalSize = size;
}

void StackAllocator::shutdown() 
{
    free(memory);
}

void* StackAllocator::allocate(size_t size, size_t alignment) 
{
    VOID_ASSERT(size > 0);

    const size_t newStart = memoryAlign(allocatedSize, alignment);
    VOID_ASSERT(newStart < totalSize);

    const size_t newAllocatedSize = newStart + size;
    if (newAllocatedSize > totalSize)
    {
        VOID_MEM_ASSERT(false, "Overflow");
        return nullptr;
    }

    allocatedSize = newAllocatedSize;
    return memory + newStart;
}

void* StackAllocator::allocate(size_t size, size_t alignment, const char* file, int32_t line) 
{
    return allocate(size, alignment);
}

void StackAllocator::deallocate(void* pointer) 
{
    VOID_ASSERT(pointer >= memory);
    VOID_ASSERTM(pointer < memory + totalSize, "Out of bound free on linear allocator (outside bounds). Try a tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", (uint8_t*)pointer, (uint8_t*)pointer - memory, memory, totalSize, allocatedSize);
    VOID_ASSERTM(pointer < memory + allocatedSize, "Out of bound free on linear allocator (inside bounds, after allocated). Try attempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", (uint8_t*)pointer, (uint8_t*)pointer - memory, memory, totalSize, allocatedSize);

    const size_t sizeAtPointer = (uint8_t *)pointer - memory;
    allocatedSize = sizeAtPointer;
}

size_t StackAllocator::getMarker() const
{
    return allocatedSize;
}

void StackAllocator::freeMarker(size_t marker) 
{
    const size_t difference = marker - allocatedSize;
    if (difference > 0) 
    {
        allocatedSize = marker;
    }
}

void StackAllocator::clear() 
{
    allocatedSize = 0;
}

void DoubleStackAllocator::init(size_t size) 
{
    memory = (uint8_t*)malloc(size);
    top = size;
    bottom = 0;
    totalSize = size;
}

void DoubleStackAllocator::shutdown() 
{
    free(memory);
}

void* DoubleStackAllocator::allocate(size_t size, size_t alignment) 
{
    VOID_ASSERTM(false, "You can't do that with a double stack allocator.");
    return nullptr;
}

void* DoubleStackAllocator::allocate(size_t size, size_t alignment, const char* file, int32_t line) 
{
    VOID_ASSERTM(false, "You can't do that with a double stack allocator.");
    return nullptr;
}

void DoubleStackAllocator::deallocate(void* pointer) 
{
    VOID_ASSERTM(false, "You can't do that with a double stack allocator.");
}

void* DoubleStackAllocator::allocateTop(size_t size, size_t alignment) 
{
    VOID_ASSERT(size > 0);

    const size_t newStart = memoryAlign(top - size, alignment);
    if (newStart <= bottom) 
    {
        VOID_MEM_ASSERT(false, "Overflow Crossing");
        return nullptr;
    }

    top = newStart;
    return memory + newStart;
}

void* DoubleStackAllocator::allocateBottom(size_t size, size_t alignment) 
{
    VOID_ASSERT(size > 0);

    const size_t newStart = memoryAlign(bottom, alignment);
    const size_t newAllocatedSize = newStart + size;
    if (newAllocatedSize >= top) 
    {
        VOID_MEM_ASSERT(false, "Overflow Crossing");
        return nullptr;
    }

    bottom = newAllocatedSize;
    return memory + newStart;
}

void DoubleStackAllocator::deallocateTop(size_t size) 
{
    if (size > totalSize - top) 
    {
        top = totalSize;
    }
    else 
    {
        top += size;
    }
}

void DoubleStackAllocator::deallocateBottom(size_t size) 
{
    if (size > bottom)
    {
        bottom = 0;
    }
    else
    {
        bottom -= size;
    }
}

size_t DoubleStackAllocator::getTopMarker() const
{
    return top;
}

size_t DoubleStackAllocator::getBottomMarker() const
{
    return bottom;
}

void DoubleStackAllocator::freeTopMarker(size_t marker) 
{
    if (marker > top && marker < totalSize) 
    {
        top = marker;
    }
}

void DoubleStackAllocator::freeBottomMarker(size_t marker) 
{
    if (marker < bottom) 
    {
        bottom = marker;
    }
}

void DoubleStackAllocator::clearTop() 
{
    top = totalSize;
}

void DoubleStackAllocator::clearBottom() 
{
    bottom = 0;
}

