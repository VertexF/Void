#ifndef ARRAY_HDR
#define ARRAY_HDR

#include "Memory.hpp"
#include "Assert.hpp"

//AlignedArray
template<typename T>
struct Array 
{
    Array() = default;
    ~Array() = default;

    void init(Allocator* alloc, uint32_t initalCapacity, uint32_t initialSize = 0)
    {
        data = nullptr;
        size = initialSize;
        capacity = 0;
        allocator = alloc;

        if (initalCapacity > 0)
        {
            grow(initalCapacity);
        }
    }

    void shutdown()
    {
        if (capacity > 0)
        {
            allocator->deallocate(data);
        }

        data = nullptr;
        size = 0;
        capacity = 0;
    }

    void push(const T& element)
    {
        if (size >= capacity)
        {
            grow(capacity + 1);
        }

        data[size++] = element;
    }

    //Grow the size and return T to be filled.
    T& push_use()
    {
        if (size >= capacity)
        {
            grow(capacity + 1);
        }
        ++size;

        return back();
    }

    void pop()
    {
        VOID_ASSERT(size > 0);
        --size;
    }

    void deleteSwap(uint32_t index)
    {
        VOID_ASSERT(size > 0 && index < size);
        data[index] = data[--size];
    }

    T& operator[](uint32_t index)
    {
        VOID_ASSERT(index < size);
        return data[index];
    }

    const T& operator[](uint32_t index) const
    {
        VOID_ASSERT(index < size);
        return data[index];
    }

    void clear()
    {
        size = 0;
    }

    void setSize(uint32_t newSize)
    {
        if (newSize > capacity)
        {
            grow(newSize);
        }
        size = newSize;
    }

    void setCapacity(uint32_t newCapacity)
    {
        if (newCapacity > capacity)
        {
            grow(newCapacity);
        }
    }

    void grow(uint32_t newCapacity)
    {
        if (newCapacity < capacity * 2)
        {
            newCapacity = capacity * 2;
        }
        else if (newCapacity < 4)
        {
            newCapacity = 4;
        }

        T* newData = (T*)allocator->allocate(newCapacity * sizeof(T), alignof(T));
        if (capacity)
        {
            memoryCopy(newData, data, capacity * sizeof(T));

            allocator->deallocate(data);
        }

        data = newData;
        capacity = newCapacity;
    }

    T& back()
    {
        VOID_ASSERT(size);
        return data[size - 1];
    }

    const T& back() const
    {
        VOID_ASSERT(size);
        return data[size - 1];
    }

    T& front()
    {
        VOID_ASSERT(size);
        return data[0];
    }

    const T& front() const
    {
        VOID_ASSERT(size);
        return data[0];
    }

    uint32_t sizeInBytes() const
    {
        return size * sizeof(T);
    }

    uint32_t capacityInByte() const
    {
        return capacity * sizeof(T);
    }

    void copyArray(const Array<T>& source)
    {
        //This is here to stop people from copying memory from a larger array to a smaller array. This will likely cause memory corrpution
        //As it will overrun the allocated memory of the destination array.
        VOID_ASSERTM(capacity >= source.size, "Source array's size can't be bigger than the capacity of the desination array's size.");

        memoryCopy(data, source.data, source.size * sizeof(T));
        size = source.size;
    }

    void appendArray(const Array<T>& source)
    {
        uint32_t newSize = size + source.size;
        if (newSize > capacity)
        {
            grow(newSize);
        }

        memoryCopy(data + size, source.data, source.size * sizeof(T));
        size += source.size;
    }

    T* data = nullptr;
    uint32_t size = 0;
    uint32_t capacity = 0;
    Allocator* allocator = nullptr;
};

template<typename T>
struct ArrayView 
{
    ArrayView(T* d, uint32_t size) : data(d), size(size)
    {
    }

    void set(T* newData, uint32_t newSize)
    {
        data = newData;
        size = newSize;
    }

    T& operator[](uint32_t index)
    {
        VOID_ASSERT(index < size);
        return data[index];
    }

    const T& operator[](uint32_t index) const
    {
        VOID_ASSERT(index < size);
        return data[index];
    }

    T* data = nullptr;
    uint32_t size = 0;
};

#endif // !ARRAY_HDR
