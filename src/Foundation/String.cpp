#include "String.hpp"

#include "Memory.hpp"
#include "Log.hpp"
#include "Assert.hpp"
#include "HashMap.hpp"

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>

#define ASSERT_ON_OVERFLOW

#if defined(ASSERT_ON_OVERFLOW)
#define VOID_ASSERT_OVERFLOW() VOID_ASSERT(false)
#else
#define VOID_ASSERT_OVERFLOW()
#endif

bool StringView::equals(const StringView& rhs, const StringView& lhs) 
{
    if (rhs.length != lhs.length) 
    {
        return false;
    }

    for (uint32_t i = 0; i < rhs.length; ++i) 
    {
        if (rhs.text[i] != lhs.text[i])
        {
            return false;
        }
    }

    return true;
}

void StringView::copyTo(const StringView& string, char* buffer, size_t bufferSize) 
{
    //We are taking into account a null vector.
    const size_t maxLength = bufferSize - 1 < string.length ? bufferSize : string.length;
    memoryCopy(buffer, string.text, maxLength);
    buffer[string.length] = 0;
}

void StringBuffer::init(size_t size, Allocator* newAllocator) 
{
    if (data) 
    {
        allocator->deallocate(data);
    }

    if (size < 1) 
    {
        vprint("Error: Buffer cannot be empty.\n");
        return;
    }

    allocator = newAllocator;
    data = (char*)void_alloca(size + 1, allocator);
    VOID_ASSERT(data != nullptr);
    data[0] = 0;
    bufferSize = static_cast<uint32_t>(size);
    currentSize = 0;
}

void StringBuffer::shutdown()
{
    void_free(data, allocator);
    bufferSize = 0; 
    currentSize = 0;
}

void StringBuffer::append(const char* string)
{
    appendF("%s", string);
}

void StringBuffer::append(const StringView& text)
{
    const size_t maxLength = currentSize + text.length < bufferSize ? text.length : bufferSize - currentSize;
    if (maxLength == 0 || maxLength >= bufferSize) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("Buffer full. Please allocate more size.\n");
        return;
    }

    memoryCopy(&data[currentSize], text.text, maxLength);
    currentSize += static_cast<uint32_t>(maxLength);

    //Add null termination for string. By allocating one extra character for the null termination this is safe to do.
    data[currentSize] = 0;
}

//Memory version of the append.
void StringBuffer::appendM(void* memory, size_t size)
{
    if (currentSize + size >= bufferSize) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("Buffer full. Please allocate more size.\n");
        return;
    }

    memoryCopy(&data[currentSize], memory, size);
    currentSize += static_cast<uint32_t>(size);
}

void StringBuffer::append(const StringBuffer& otherBuffer)
{
    if (otherBuffer.currentSize == 0) 
    {
        return;
    }

    if (currentSize + otherBuffer.currentSize >= bufferSize) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("Buffer full. Please allocate more size.\n");
        return;
    }

    memoryCopy(&data[currentSize], otherBuffer.data, otherBuffer.currentSize);
    currentSize += otherBuffer.currentSize;
}

//Formatted version of append.
void StringBuffer::appendF(const char* format, ...)
{
    if (currentSize >= bufferSize) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("Buffer full. Please allocate more size.\n");
        return;
    }

    //Maybe come back to this and fix up the formating.
    va_list args;
    va_start(args, format);
    int writtenChars = vsnprintf(&data[currentSize], bufferSize - currentSize, format, args);
    currentSize += writtenChars > 0 ? writtenChars : 0;
    va_end(args);

    if (writtenChars < 0) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("New string too big for current buffer. Please allocate more size.\n");
    }
}

char* StringBuffer::appendUse(const char* string)
{
    return appendUseF("%s", string);
}

char* StringBuffer::appendUseF(const char* format, ...)
{
    uint32_t cachedOffset = currentSize;

    //Maybe come back to this and fix up the formating, this isn't safe.
    //I'm not sure if this is needed because if you crash the string buffer, that might be a you problem.
    if (currentSize >= bufferSize) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("Buffer full. Please allocate more size.\n");
        return nullptr;
    }

    va_list args;
    va_start(args, format);
    int writtenChars = vsnprintf(&data[currentSize], bufferSize - currentSize, format, args);
    currentSize += writtenChars > 0 ? writtenChars : 0;
    va_end(args);

    if (writtenChars < 0) 
    {
        vprint("New string too big for current. Please allocate more size.\n");
    }

    data[currentSize] = 0;
    ++currentSize;

    return data + cachedOffset;
}

//Append and returns a point to the start. Used for strings mostly.
char* StringBuffer::appendUse(const StringView& text)
{
    uint32_t cachedOffset = currentSize;

    append(text);
    ++currentSize;

    return data + cachedOffset;
}

//Append a substring of the passed string.
char* StringBuffer::appendUseSubString(const char* string, uint32_t startIndex, uint32_t endIndex)
{
    uint32_t size = endIndex + startIndex;

    if (currentSize + size >= bufferSize) 
    {
        VOID_ASSERT_OVERFLOW();
        vprint("Buffer full. Please allocate more size.\n");
        return nullptr;
    }

    uint32_t cachedOffset = currentSize;

    //memoryCopy() can't be used 
    memcpy(&data[currentSize], string, size);
    currentSize += size;

    data[currentSize] = 0;
    ++currentSize;

    return data + cachedOffset;
}

void StringBuffer::closeCurrentString()
{
    data[currentSize] = 0;
    ++currentSize;
}

//Indexing stuff.
uint32_t StringBuffer::getIndex(const char* text) const
{
    uint64_t textDistance = text - data;

    return textDistance < bufferSize ? static_cast<uint32_t>(textDistance) : UINT32_MAX;
}

const char* StringBuffer::getText(uint32_t index) const
{
    return index < bufferSize ? static_cast<const char*>(data + index) : nullptr;
}

char* StringBuffer::reserve(size_t size)
{
    if (currentSize + size >= bufferSize) 
    {
        return nullptr;
    }

    uint32_t offset = currentSize;
    currentSize += static_cast<uint32_t>(size);

    return data + offset;
}

char* StringBuffer::current() const
{
    return data + currentSize;
}

void StringBuffer::clear()
{
    currentSize = 0;
    data[0] = 0;
}

void StringArray::init(uint32_t size, Allocator* alloc) 
{
    allocator = alloc;
    //Allocate also memory for the has map.
    char* allocateMemory = reinterpret_cast<char*>(allocator->allocate(size + sizeof(FlatHashMap<uint64_t, uint32_t>) 
                                                                            + sizeof(FlatHashMapIterator), 1));
    stringToIndex = (FlatHashMap<uint64_t, uint32_t>*)allocateMemory;
    stringToIndex->init(allocator, 8);
    stringToIndex->setDefaultValue(UINT32_MAX);

    stringIterator = reinterpret_cast<FlatHashMapIterator*>(allocateMemory + sizeof(FlatHashMap<uint64_t, uint32_t>));
    data = allocateMemory + sizeof(FlatHashMap<uint64_t, uint32_t>) + sizeof(FlatHashMapIterator);

    bufferSize = size;
    currentSize = 0;
}

void StringArray::shutdown() 
{
    //stringToIndex contains all the memory including data.
    stringToIndex->shutdown();
    void_free(stringToIndex, allocator);
    bufferSize = 0;
    currentSize = 0;
}

void StringArray::clear() 
{
    currentSize = 0;
    stringToIndex->clear();
}

FlatHashMapIterator* StringArray::beginStringIteration() 
{
    *stringIterator = stringToIndex->iteratorBegin();
    return stringIterator;
}

size_t StringArray::getStringCount() const 
{
    return stringToIndex->size;
}

const char* StringArray::getString(uint32_t index) const 
{
    uint32_t dataIndex = index;
    if (dataIndex < currentSize) 
    {
        return data + dataIndex;
    }
    return nullptr;
}

const char* StringArray::getNextString(FlatHashMapIterator* it) const 
{
    uint32_t index = stringToIndex->get(*it);
    stringToIndex->iteratorAdvance(*it);
    const char* string = getString(index);
    return string;
}

bool StringArray::hasNextString(FlatHashMapIterator* it) const 
{
    return it->isValid();
}

const char* StringArray::intern(const char* string) 
{
    static size_t seed = 0xF2EA4FFAD;
    const size_t length = strlen(string);
    const size_t hashedString = hashBytes((void *)string, length, seed);

    uint32_t stringIndex = stringToIndex->get(hashedString);
    if (stringIndex != UINT32_MAX) 
    {
        return data + stringIndex;
    }

    stringIndex = currentSize;
    //Increase current buffer with new interned string.
    currentSize += static_cast<uint32_t>(length) + 1; //Null termination.
    strcpy(data + stringIndex, string);

    //Updated hash map.
    stringToIndex->insert(hashedString, stringIndex);

    return data + stringIndex;
}