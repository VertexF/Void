#ifndef BLOB_SERIALISATION_HDR
#define BLOB_SERIALISATION_HDR

#include "Platform.hpp"
#include "RelativeDataStructures.hpp"
#include "Array.hpp"
#include "Blob.hpp"

struct Allocator;

struct BlobSerialiser 
{
    //Allocate size bytes, set the data version start writing.
    //Data version will be saved at the beginning of the file.
    template<typename T>
    T* writeAndPrepare(Allocator* alloc, uint32_t serialiserVersion, size_t size)
    {
        writeCommon(alloc, serialiserVersion, size);

        //Allocate root data. BlobHeader is already allocated in the writeCommon function.
        allocateStatic(sizeof(T) - sizeof(BlobHeader));

        //Manually managed blob serilisation.
        dataMemory = nullptr;

        return (T*)blobMemory;
    }

    template<typename T>
    void writeAndSerialise(Allocator* alloc, uint32_t serialiserVersion, size_t size, T* rootData)
    {
        VOID_ASSERTM(rootData, "Data should never be null.");

        writeCommon(alloc, serialiserVersion, size);

        //Allocate root data. BlobHeader is already allocated in the writeCommon function.
        allocateStatic(sizeof(T) - sizeof(BlobHeader));

        //Save root data memory offset calculation
        dataMemory = (char*)rootData;
        //Serilise root data.
        serialise(rootData);
    }

    void writeCommon(Allocator* alloc, uint32_t serialiserVersion, size_t size);

    //Init blob in reading mode from a chunk of perallocated memory. 
    //Size is used to check whether readin is heappening out of the chunk.
    //Allocator is used to allocate memory if needed (for example when reading an array.)
    template<typename T>
    T* read(Allocator* alloc, uint32_t serialiserVersion, size_t size, char* blobMemory, bool forceSerialisation = false)
    {
        allocator = alloc;
        this->blobMemory = blobMemory;
        dataMemory = nullptr;

        totalSize = static_cast<uint32_t>(size);

        this->serialiserVersion = serialiserVersion;
        isReading = 1;
        hasAllocatedMemory = 0;

        //Read header from blob.
        BlobHeader* header = (BlobHeader*)blobMemory;
        dataVersion = header->version;
        dataVersion = header->mappable;

        //If serialiser and data are at the same version no need to serialise.
        //TODO: Mappibiliy should be taken is consider.
        if (serialiserVersion == dataVersion && forceSerialisation == false)
        {
            return (T*)(blobMemory);
        }

        hasAllocatedMemory = 1;
        serialiserVersion = dataVersion;

        //Allocate the data baby.
        dataMemory = (char*)void_allocam(size, allocator);
        T* destinationData = (T*)dataMemory;

        serialisedOffset += sizeof(BlobHeader);

        allocateStatic(sizeof(T));
        //Read from blob to data.
        serialise(destinationData);

        return destinationData;
    }

    void shutdown();

    //This functions are used both for reading and writing.
    //Lead of the serialisation 
    void serialise(char* data);
    void serialise(int8_t* data);
    void serialise(uint8_t* data);
    void serialise(int16_t* data);
    void serialise(uint16_t* data);
    void serialise(int32_t* data);
    void serialise(uint32_t* data);
    void serialise(int64_t* data);
    void serialise(uint64_t* data);
    void serialise(float* data);
    void serialise(double* data);
    void serialise(bool* data);
    void serialise(const char* data);

    template<typename T>
    void serialise(RelativePointer<T>* data)
    {
        if (isReading)
        {
            //Reading: Blob -> Data structure.
            int32_t sourceDataOffset;
            serialise(&sourceDataOffset);

            //Early out to not follow nullptr.
            if (sourceDataOffset == 0)
            {
                data->offset = 0;
                return;
            }
            data->offset = getRelativeDataOffset(data);

            //Allocate memory and set pointer.
            allocateStatic<T>();

            //Cache source serialised offset.
            uint32_t cachedSerialised = serialisedOffset;

            //Move serialisation off. The offset is still this->offset, and the serialise offset.
            //Point just right AFTER it, thus move back by sizeof(offset).
            serialisedOffset = cachedSerialised + sourceDataOffset - sizeof(uint32_t);
            //Serialise/visit the pointed data structure.
            serialise(data->get());
            //Restore serialisation offset.
            serialisedOffset = cachedSerialised;
        }
        else
        {
            //Writing
            //Data -> blob calculate offset used by RelativePointer.
            //Remember this: char* address = ((char*)&this->offset) + offset;
            //Serialised offset points to what will be the "this->offset"
            //Allocated offset points to the still note allocated memory,
            //Where we will allocate from.
            int32_t dataOffset = allocatedOffset - serialisedOffset;
            serialise(&dataOffset);

            //To jump anywhere and correctly restore the serialisation process,
            //cache the current serialisation offset.
            uint32_t cachedSerialised = serialisedOffset;

            //Move serialisation to thew newly allocated memory at the of the blob.
            serialisedOffset = allocatedOffset;
            //Allocate memory in the blob
            allocateStatic<T>();
            //Serialise/visit the pointed data structure.
            serialise(data->get());
            //Restore serialised
            serialisedOffset = cachedSerialised;
        }
    }

    template<typename T>
    void serialise(RelativeArray<T>* data)
    {
        if (isReading)
        {
            //Blob --> Data
            serialise(&data->size);

            int32_t sourceDataOffset;
            serialise(&sourceDataOffset);

            //Cache serialised
            uint32_t cachedSerialised = serialisedOffset;

            data->data.offset = getRelativeDataOffset(data) - sizeof(uint32_t);

            //Reserve memory
            allocateStatic(data->size * sizeof(T));

            serialisedOffset = cachedSerialised + sourceDataOffset - sizeof(uint32_t);

            for (uint32_t i = 0; i < data->size; ++i)
            {
                T* destination = &data->get()[i];
                serialise(destination);
                destination = destination;
            }

            serialisedOffset = cachedSerialised;
        }
        else
        {
            //Data -> blob
            sierlaise(&data->size);

            //Data will be copied at the end of the current blob.
            int32_t dataOffset = allocatedOffset - serialisedOffset;
            serialise(&dataOffset);

            uint32_t cachedSerialised = serialisedOffset;
            //Move serialisation to the newly allocated memory, at the end of the blob.
            serialisedOffset = allocatedOffset;
            //Allocated memory in the blob.
            allocateStatic(data->size * sizeof(T));

            for (uint32_t i = 0; i < data->size; ++i)
            {
                T* sourceData = &data->get()[i];
                serialised(sourceData);
                sourceData = sourceData;
            }

            //Restore serialised
            serialisedOffset = cachedSerialised;
        }
    }

    template<typename T>
    void serialise(Array<T>* data)
    {
        if (isReading)
        {
            //Blob -> data
            serialise(&data->size);

            uint64_t serialisationPad;
            serialise(&serialisationPad);
            serialise(&serialisationPad);

            uint32_t packedDataOffset;
            serialise(&packedDataOffset);
            int32_t sourceDataOffset = static_cast<uint32_t>(packedDataOffset & 0x7FFFFFFF);

            //Cached serialised
            uint32_t cachedSerialised = serialisedOffset;

            data->allocator = nullptr;
            data->capacity = data->size;
            //TODO: Learn why this is commented out.
            //data->relative = (packedDataOffset >> 31);
            //Point string to the end.
            data->data = (T*)(dataMemory + allocatedOffset);
            //data->data.offset = getTraltiveDataOffset(data) - 4;

            //Reserve memory
            allocateStatic(data->size * sizeof(T));

            //sizeof(uint64_t) * 2
            serialisedOffset = cachedSerialised + sourceDataOffset - sizeof(uint32_t);

            for (uint32_t i = 0; i < data->size; ++i)
            {
                T* destination = &((*data)[i]);
                serialise(destination);
                destination = destination;
            }

            //Restore serialised
            serialisedOffset = cachedSerialised;
        }
        else
        {
            //Data -> blob
            serialise(&data->size);
            //Add serialisation pads so that we serialise all bytes of the struct Array.
            uint64_t serialisationPad = 0;
            serialise(&serialisationPad);
            serialise(&serialisationPad);

            //Data will be copied at the end of the current blob.
            int32_t dataOffset = allocatedOffset - serialisedOffset;
            //Set higher bit of flag.
            uint32_t packedDataOffset = (static_cast<uint32_t>(dataOffset | (1 << 31)));
            serialise(&packedDataOffset);

            uint32_t cachedSerialised = serialisedOffset;
            //Moved serialisation to the newly allocated memory, at the end of the blob.
            serialisedOffset = allocatedOffset;
            //Allocated memory in the blob
            allocateStatic(data->size * sizeof(T));

            for (uint32_t i = 0; i < data->size; ++i)
            {
                T* sourceData = &((*data)[i]);
                serialise(sourceData);
                sourceData = sourceData;
            }

            //Restore serialised
            serialisedOffset = cachedSerialised;
        }
    }

    template<typename T>
    void serialise(T* data)
    {
        VOID_ASSERTM(false, "This should never be hit!");
    }

    void serialise(RelativeString* data);

    void serialiseMemory(void* data, size_t size);
    void serialiseMemoryBlock(void** data, uint32_t* size);

    //Static allocation from the blob allocated memory.
    //Just allocates the size of bytes and returns. Used to fill in structures.
    char* allocateStatic(size_t size);

    template<typename T>
    T* allocateStatic()
    {
        return (T*)allocateStatic(sizeof(T));
    }

    template<typename T>
    T* allocateAndSet(RelativePointer<T>& data, void* sourceData = nullptr)
    {
        char* destinationMemory = allocateStatic(sizeof(T));
        data.set(destinationMemory);

        if (sourceData)
        {
            memoryCopy(destinationMemory, sourceData, sizeof(T));
        }
    }

    //Allocates an array and sets itit so ic can be accessed.
    template<typename T>
    void allocateAndSet(RelativeArray<T>& data, uint32_t numElements, void* sourceData = nullptr)
    {
        char* destinationMemory = allocateStatic(sizeof(T) * numElements);
        data.set(destinationMemory, numElements);

        if (sourceData)
        {
            memoryCopy(destinationMemory, sourceData, sizeof(T) * numElements);
        }

    }

    //Allocates and sets a static string.
    void allocateAndSet(RelativeString& string, const char* format, ...);
    //Allocates and sets a static string.
    void allocateAndSet(RelativeString& string, const char* text, uint32_t length);

    int32_t getRelativeDataOffset(void* data);

    char* blobMemory = nullptr;
    char* dataMemory = nullptr;

    Allocator* allocator = nullptr;

    uint32_t totalSize = 0;
    uint32_t serialisedOffset = 0;
    uint32_t allocatedOffset = 0;

    //Version coming from the code.
    uint32_t serialiserVersion = UINT32_MAX;
    //Version read from blob or written into blob
    uint32_t dataVersion = UINT32_MAX;

    uint32_t isReading = 0;
    uint32_t isMappable = 0;

    uint32_t hasAllocatedMemory = 0;
};

#endif // !BLOB_SERIALISATION_HDR
