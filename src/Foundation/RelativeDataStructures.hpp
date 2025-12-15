#ifndef RELATIVE_DATA_STRUCTURES_HDR
#define RELATIVE_DATA_STRUCTURES_HDR

#include "Platform.hpp"

#include "Memory.hpp"
#include "Assert.hpp"
#include "Array.hpp"

//These just set and get data in memory and offset the correct amount depending on the type used in the 
// template so we get the correct address for the type.

//This is used in the code to write blueprints, like data compilers
#define VOID_BLOB_WRITE

struct Allocator;

template<typename T>
struct RelativePointer
{
    T* get() const
    {
        char* address = ((char*)&offset) + offset;
        return offset != 0 ? (T*)address : nullptr;
    }

    bool isEqual(const RelativePointer& other) const
    {
        return get() == other.get();
    }

    bool isNull() const
    {
        return offset == 0;
    }

    bool isNotNull() const
    {
        return offset != 0;
    }

    //This hopefully gives us a cleaner interface.
    T* operator->() const
    {
        return get();
    }

    T& operator*() const
    {
        return *(get());
    }

#if defined(VOID_BLOB_WRITE)
    void set(char* rawPointer)
    {
        offset = rawPointer ? (int32_t)(rawPointer - (char*)this) : 0;
    }

    void setNull()
    {
        offset = 0;
    }
#endif
    int32_t offset = 0;
};

template <typename T>
struct RelativeArray 
{
    const T& operator[](uint32_t index) const
    {
        VOID_ASSERT(index < size);
        return data.get()[index];
    }

    T& operator[](uint32_t index)
    {
        VOID_ASSERT(index < size);
        return data.get()[index];
    }

    const T* get() const
    {
        return data.get();
    }

    T* get()
    {
        return data.get();
    }

#if defined(VOID_BLOB_WRITE)
    void set(char* rawPointer, uint32_t size)
    {
        data.set(rawPointer);
        this->size = size;
    }

    void setEmpty()
    {
        size = 0;
        data.setNull();
    }
#endif

    uint32_t size = 0;
    RelativePointer<T> data{};
};

struct RelativeString : public RelativeArray<char> 
{
    const char* c_str() const { return data.get(); }
    void set(char* ptr, uint32_t size) { return RelativeArray<char>::set(ptr, size); }
};

#endif // !RELATIVE_DATA_STRUCTURES_HDR
