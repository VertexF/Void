#ifndef ARRAY_THREAD_HDR
#define ARRAY_THREAD_HDR

#include "Memory.hpp"
#include "Assert.hpp"

#include <semaphore>

//AlignedArray
template<typename T>
struct ArrayThread
{
    ArrayThread() = default;
    ~ArrayThread() = default;

    void init(uint32_t initalCapacity, uint32_t initialSize = 0)
    {
        //No thread-safe on purpose.
        data = nullptr;
        size = initialSize;
        capacity = 0;
        allocator = &MemoryService::instance()->threadedHeapAllocator;

        if (initalCapacity > 0)
        {
            grow(initalCapacity);
        }
    }

    void shutdown()
    {
        //No thread-safe on purpose.
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
        lock.lock();
        if (size >= capacity)
        {
            grow(capacity + 1);
        }

        data[size++] = element;
        lock.unlock();
    }

    //Grow the size and return T to be filled.
    T& push_use()
    {
        lock.lock();
        if (size >= capacity)
        {
            grow(capacity + 1);
        }

        ++size;
        lock.unlock();

        return back();
    }

    void pop()
    {
        VOID_ASSERT(size > 0);
        lock.lock();
        --size;
        lock.unlock();
    }

    void deleteSwap(uint32_t index)
    {
        VOID_ASSERT(size > 0 && index < size);
        lock.lock();
        data[index] = data[--size];
        lock.unlock();
    }

    void erase(uint32_t index)
    {
        VOID_ASSERT(size > 0 && index < size);
        lock.lock();
        for (uint32_t i = index; i < size - 1; ++i)
        {
            data[i] = data[i + 1];
        }
        size--;
        lock.unlock();
    }

    void clear()
    {
        size = 0;
    }

    void setSize(uint32_t newSize)
    {
        lock.lock();
        if (newSize > capacity)
        {
            grow(newSize);
        }
        size = newSize;
        lock.unlock();
    }

    void setCapacity(uint32_t newCapacity)
    {
        lock.lock();
        if (newCapacity > capacity)
        {
            grow(newCapacity);
        }
        lock.unlock();
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
            memcpy(newData, data, capacity * sizeof(T));

            allocator->deallocate(data);
        }

        data = newData;
        capacity = newCapacity;
    }

    T& back()
    {
        VOID_ASSERT(size);
        //TODO: - Use conditional variable to order threads to finish removing something from an array, instead of locking up the whole array.
        lock.lock();
        T result = data[size - 1];
        lock.unlock();
        return result;
    }

    const T& back() const
    {
        VOID_ASSERT(size);
        lock.lock();
        const T result = data[size - 1];
        lock.unlock();
        return result;
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

    uint32_t get(uint32_t index) const 
    {
        return data[index];
    }

    void copyArray(const Array<T>& source)
    {
        //This is here to stop people from copying memory from a larger array to a smaller array. This will likely cause memory corrpution
        //As it will overrun the allocated memory of the destination array.
        VOID_ASSERTM(capacity >= source.size, "Source array's size can't be bigger than the capacity of the desination array's size.");

        lock.lock();
        memcpy(data, source.data, source.size * sizeof(T));
        size = source.size;
        lock.unlock();
    }

    void appendArray(const Array<T>& source)
    {
        uint32_t newSize = size + source.size;
        if (newSize > capacity)
        {
            grow(newSize);
        }

        lock.lock();
        memcpy(data + size, source.data, source.size * sizeof(T));
        size += source.size;
        lock.unlock();
    }

    uint32_t size = 0;
    uint32_t capacity = 0;
    T* data = nullptr;
    ThreadedHeapAllocator* allocator = nullptr;
    std::mutex lock;
};

#endif // !ARRAY_THREAD_HDR