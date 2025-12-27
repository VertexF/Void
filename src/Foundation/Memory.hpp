#ifndef MEMORY_HDR
#define MEMORY_HDR

#include "Platform.hpp"

//#define VOID_IMGUI

void memoryCopy(void* destination, void* source, size_t size);
//Calculate memory alignment size.
size_t memoryAlign(size_t size, size_t alignment);

struct MemoryStatistics 
{
    size_t allocatedBytes;
    size_t totalBytes;

    uint32_t allocationCount;

    void add(size_t a) 
    {
        if (a) 
        {
            allocatedBytes += a;
            ++allocationCount;
        }
    }
};

struct Allocator
{
    virtual ~Allocator() = default;
    virtual void* allocate(size_t size, size_t alignment) = 0;
    virtual void* allocate(size_t size, size_t alignment, const char* file, int32_t line) = 0;

    virtual void deallocate(void * pointer) = 0;
};

struct HeapAllocator : public Allocator
{
    virtual ~HeapAllocator() override = default;

    void init(size_t size);
    void shutdown();

#if defined VOID_IMGUI
    void debugUI();
#endif //VOID_IMGUI

    virtual void* allocate(size_t size, size_t alignment) override;
    virtual void* allocate(size_t size, size_t alignment, const char* file, int32_t line) override;

    virtual void deallocate(void* pointer) override;

    void* TLSFHandle  = nullptr;
    void* memory = nullptr;
    size_t allocatedSize = 0;
    size_t maxSize = 0;
};

struct StackAllocator : public Allocator
{
    virtual ~StackAllocator() override = default;

    void init(size_t size);
    void shutdown();

    virtual void* allocate(size_t size, size_t alignment) override;
    virtual void* allocate(size_t size, size_t alignment, const char* file, int32_t line) override;

    virtual void deallocate(void* pointer) override;

    size_t getMarker() const;
    void freeMarker(size_t marker);

    void clear();

    uint8_t* memory = nullptr;
    size_t totalSize = 0;
    size_t allocatedSize = 0;
};

struct DoubleStackAllocator : public Allocator
{
    virtual ~DoubleStackAllocator() override = default;

    void init(size_t size);
    void shutdown();

    virtual void* allocate(size_t size, size_t alignment) override;
    virtual void* allocate(size_t size, size_t alignment, const char* file, int32_t line) override;

    virtual void deallocate(void* pointer) override;

    void* allocateTop(size_t size, size_t alignment);
    void* allocateBottom(size_t size, size_t alignment);

    void deallocateTop(size_t size);
    void deallocateBottom(size_t size);

    size_t getTopMarker() const;
    size_t getBottomMarker() const;

    void freeTopMarker(size_t marker);
    void freeBottomMarker(size_t marker);

    void clearTop();
    void clearBottom();

    uint8_t* memory = nullptr;
    size_t totalSize = 0;
    size_t top = 0;
    size_t bottom = 0;
};

//DO NOT use this for runtime processes. ONLY compilation resources.
//Don't use to allocate stuff in run time.
struct MallocAllocator : public Allocator
{
    virtual void* allocate(size_t size, size_t alignment) override;
    virtual void* allocate(size_t size, size_t alignment, const char* file, int32_t line) override;

    virtual void deallocate(void* pointer) override;
};

struct MemoryService
{
    static MemoryService* instance();

    void init(uint64_t heapSize, uint64_t stackSize);
    void shutdown();

#if defined VOID_IMGUI
    void imguiDraw();
#endif

    StackAllocator scratchAllocator{};
    HeapAllocator systemAllocator{};
};

#define void_alloca(size, allocator) ((allocator)->allocate(size, 1, __FILE__, __LINE__))
#define void_allocam(size, allocator) ((uint8_t*)(allocator)->allocate(size, 1, __FILE__, __LINE__))
#define void_allocat(type, allocator) ((type*)(allocator)->allocate(sizeof(type), 1, __FILE__, __LINE__))

#define void_allocaa(size, allocator, alignment) ((allocator)->allocate(size, alignment, __FILE__, __LINE__))

#define void_free(pointer, allocate) ((allocate)->deallocate(pointer))

#define void_kilo(size) (size * 1024)
#define void_mega(size) (size * 1024 * 1024)
#define void_giga(size) (size * 1024 * 1024 * 1024)


#endif // !MEMORY_HDR
