#include "BlobSerialisation.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <memory.h>


void BlobSerialiser::writeCommon(Allocator* alloc, uint32_t serialiserVersion, size_t size) 
{
    allocator = alloc;
    //Allocate memory.
    blobMemory = (char*)void_alloca(size + sizeof(BlobHeader), allocator);
    VOID_ASSERT(blobMemory != nullptr);

    hasAllocatedMemory = 1;

    totalSize = static_cast<uint32_t>(size) + sizeof(BlobHeader);
    serialisedOffset = allocatedOffset = 0;

    this->serialiserVersion = serialiserVersion;
    //This will be written into the blob.
    dataVersion = serialiserVersion;
    isReading = 0;
    isMappable = 0;

    //Write header.
    BlobHeader* header = (BlobHeader*)allocateStatic(sizeof(BlobHeader));
    header->version = serialiserVersion;
    header->mappable = isMappable;

    serialisedOffset = allocatedOffset;
}

void BlobSerialiser::shutdown() 
{
    if (isReading) 
    {
        //When reading and serialising, we can free blob memory after read.
        //Other we will free pointer when done.
        if (blobMemory && hasAllocatedMemory) 
        {
            void_free(blobMemory, allocator);
        }
    }
    else 
    {
        if (blobMemory) 
        {
            void_free(blobMemory, allocator);
        }
    }

    serialisedOffset = allocatedOffset = 0;
}

//This functions are used both for reading and writing.
//Lead of the serialisation 
void BlobSerialiser::serialise(char* data)
{
    if (isReading) 
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(char));
    }
    else 
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(char));
    }

    serialisedOffset += sizeof(char);
}

void BlobSerialiser::serialise(int8_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(int8_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(int8_t));
    }

    serialisedOffset += sizeof(int8_t);
}

void BlobSerialiser::serialise(uint8_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(uint8_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(uint8_t));
    }

    serialisedOffset += sizeof(uint8_t);
}

void BlobSerialiser::serialise(int16_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(int16_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(int16_t));
    }

    serialisedOffset += sizeof(int16_t);
}

void BlobSerialiser::serialise(uint16_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(uint16_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(uint16_t));
    }

    serialisedOffset += sizeof(uint16_t);
}

void BlobSerialiser::serialise(int32_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(int32_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(int32_t));
    }

    serialisedOffset += sizeof(int32_t);
}

void BlobSerialiser::serialise(uint32_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(uint32_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(uint32_t));
    }

    serialisedOffset += sizeof(uint32_t);
}

void BlobSerialiser::serialise(int64_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(int64_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(int64_t));
    }

    serialisedOffset += sizeof(int64_t);
}

void BlobSerialiser::serialise(uint64_t* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(uint64_t));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(uint64_t));
    }

    serialisedOffset += sizeof(uint64_t);
}

void BlobSerialiser::serialise(float* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(float));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(float));
    }

    serialisedOffset += sizeof(float);
}

void BlobSerialiser::serialise(double* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(double));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(double));
    }

    serialisedOffset += sizeof(double);
}

void BlobSerialiser::serialise(bool* data)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], sizeof(bool));
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, sizeof(bool));
    }

    serialisedOffset += sizeof(bool);
}

void BlobSerialiser::serialise(const char* data)
{
    VOID_ASSERTM(false, "Not yet implemented.");
}

void BlobSerialiser::serialise(RelativeString* data)
{
    if (isReading) 
    {
        //Blob -> data
        serialise(&data->size);

        int32_t sourceDataOffset;
        serialise(&sourceDataOffset);

        if (sourceDataOffset > 0) 
        {
            //Cache serialised
            uint32_t cachedSerialised = serialisedOffset;
            serialisedOffset = allocatedOffset;
            data->data.offset = getRelativeDataOffset(data) - 4;

            //Reserve memory + string ending
            allocateStatic(static_cast<size_t>(data->size + 1));

            char* sourceData = blobMemory + cachedSerialised + sourceDataOffset - 4;
            memoryCopy((char*)data->c_str(), sourceData, (size_t)data->size + 1);
            vprint("Found %s\n", data->c_str());
            //Restore serialised
            serialisedOffset = cachedSerialised;
        }
        else 
        {
            data->setEmpty();
        }
    }
    else 
    {
        //Data -> blob
        serialise(&data->size);
        //Data will be copied at the end of the current blob.
        int32_t dataOffset = allocatedOffset - serialisedOffset;
        serialise(&dataOffset);

        uint32_t cachedSerialised = serialisedOffset;
        //Move serialisation to at the end of the blob.
        serialisedOffset = allocatedOffset;
        //Allocate memory in the blob
        allocateStatic(static_cast<size_t>(data->size + 1));

        char* destinationData = blobMemory + serialisedOffset;
        memoryCopy(destinationData, (char*)data->c_str(), static_cast<size_t>(data->size + 1));
        vprint("Written %s, Found %s\n", data->c_str(), destinationData);

        //Restore serilised
        serialisedOffset = cachedSerialised;
    }
}

void BlobSerialiser::serialiseMemory(void* data, size_t size)
{
    if (isReading)
    {
        memoryCopy(data, &blobMemory[serialisedOffset], size);
    }
    else
    {
        memoryCopy(&blobMemory[serialisedOffset], data, size);
    }

    serialisedOffset += static_cast<uint32_t>(size);
}

void BlobSerialiser::serialiseMemoryBlock(void** data, uint32_t* size)
{
    serialise(size);

    if (isReading) 
    {
        //blob -> data
        int32_t sourceDataOffset;
        serialise(&sourceDataOffset);

        if (sourceDataOffset > 0) 
        {
            //Cached serialised
            uint32_t cachedSerialised = serialisedOffset;

            serialisedOffset = allocatedOffset;
            *data = dataMemory + allocatedOffset;

            //Reserve memory
            allocateStatic(*size);

            char* sourceData = blobMemory + cachedSerialised + sourceDataOffset - 4;
            memoryCopy(*data, sourceData, *size);
            //Restore serialised
            serialisedOffset = cachedSerialised;
        }
        else 
        {
            *data = nullptr;
            size = 0;
        }
    }
    else 
    {
        //Data -> Blob
        //Data will be copied at the end of the current blob
        int32_t dataOffset = allocatedOffset - serialisedOffset;
        serialise(&dataOffset);

        uint32_t cachedSerialised = serialisedOffset;
        //Move serialisation to at the end of the blob.
        serialisedOffset = allocatedOffset;
        //Allocated memory in the blob
        allocateStatic(*size);

        char* destinationdata = blobMemory + serialisedOffset;
        memoryCopy(destinationdata, *data, *size);

        //Restore serialised.
        serialisedOffset = cachedSerialised;
    }
}

//Static allocation from the blob allocated memory.
//Just allocates the size of bytes and returns. Used to fill in structures.
char* BlobSerialiser::allocateStatic(size_t size)
{
    if (allocatedOffset + size > totalSize) 
    {
        vprint("Blob allocation error: allocated, requested, total - %u + %u > %u", allocatedOffset, size, totalSize);
        return nullptr;
    }

    uint32_t offset = allocatedOffset;
    allocatedOffset += static_cast<uint32_t>(size);

    return isReading ? dataMemory + offset : blobMemory + offset;
}

//Allocates and sets a static string.
void BlobSerialiser::allocateAndSet(RelativeString& string, const char* format, ...)
{
    uint32_t cachedOffset = allocatedOffset;

    char* destinationMemory = isReading ? dataMemory : blobMemory;
    va_list args;
    va_start(args, format);
    int writtenChars = vsnprintf(&destinationMemory[allocatedOffset], totalSize - allocatedOffset, format, args);
    allocatedOffset += writtenChars > 0 ? writtenChars : 0;
    va_end(args);

    if (writtenChars < 0) 
    {
        vprint("New string too big for current buffer! Please allocate more size.\n");
    }

    //Add null termination for string.
    //Allocating one extra character for the null termination this is always safe do.
    destinationMemory[allocatedOffset] = 0;
    ++allocatedOffset;

    string.set(destinationMemory + cachedOffset, writtenChars);
}

//Allocates and sets a static string.
void BlobSerialiser::allocateAndSet(RelativeString& string, const char* text, uint32_t length)
{
    if (allocatedOffset + length > totalSize) 
    {
        vprint("New string too big for current buffer! Please allocate more size.\n");
        return;
    }
    uint32_t cachedOffset = allocatedOffset;

    char* destinationMemory = isReading ? dataMemory : blobMemory;
    memcpy(&destinationMemory[allocatedOffset], text, length);

    allocatedOffset += length;

    //Add null termination for string.
    //Allocating one extra character for the null termination this is always safe do.
    destinationMemory[allocatedOffset] = 0;
    ++allocatedOffset;

    string.set(destinationMemory + cachedOffset, length);
}

int32_t BlobSerialiser::getRelativeDataOffset(void* data)
{
    //dataMemory points to the newly allocated data structure to be used at runtime.
    const int32_t dataOffsetFromStart = static_cast<int32_t>((char*)data - dataMemory);
    const int32_t dataOffset = allocatedOffset - dataOffsetFromStart;
    return dataOffset;
}
