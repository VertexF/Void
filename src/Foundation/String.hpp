#ifndef STRING_HDR
#define STRING_HDR

#include "Platform.hpp"

struct Allocator;

template<typename K, typename V>
struct FlatHashMap;

struct FlatHashMapIterator;

//String view that reference an already existing stream of chars.
struct StringView 
{
    char* text = nullptr;
    size_t length = 0;

    static bool equals(const StringView& rhs, const StringView& lhs);
    static void copyTo(const StringView& string, char* buffer, size_t bufferSize);
};

//A class that pre-allocates a buffer and appends string to it.
//Reserve an additional byte for the null termination when need.
struct StringBuffer 
{
    void init(size_t size, Allocator* newAllocator);
    void shutdown();

    void append(const char* string);
    void append(const StringView& text);
    //Memory version of the append.
    void appendM(void* memory, size_t size);
    void append(const StringBuffer& otherBuffer);
    //Formatted version of append.
    void appendF(const char* format, ...);

    char* appendUse(const char* string);
    char* appendUseF(const char* format, ...);
    //Append and returns a point to the start. Used for strings mostly.
    char* appendUse(const StringView& text);
    //Append a substring of the passed string.
    char* appendUseSubString(const char* string, uint32_t startIndex, uint32_t endIndex);

    void closeCurrentString();

    //Indexing stuff.
    uint32_t getIndex(const char* text) const;
    const char* getText(uint32_t index) const;

    char* reserve(size_t size);
    char* current() const;

    void clear();

    char* data = nullptr;
    Allocator* allocator = nullptr;
    uint32_t bufferSize = 1024;
    uint32_t currentSize = 0;
};

struct StringArray 
{
    void init(uint32_t size, Allocator* alloc);
    void shutdown();
    void clear();

    FlatHashMapIterator* beginStringIteration();
    size_t getStringCount() const;
    const char* getString(uint32_t index) const;
    const char* getNextString(FlatHashMapIterator* it) const;
    bool hasNextString(FlatHashMapIterator* it) const;

    const char* intern(const char* string);

    FlatHashMap<uint64_t, uint32_t>* stringToIndex;
    FlatHashMapIterator* stringIterator;

    char* data = nullptr;
    Allocator* allocator = nullptr;
    uint32_t bufferSize = 1024;
    uint32_t currentSize = 0;
};

#endif // !STRING_HDR
