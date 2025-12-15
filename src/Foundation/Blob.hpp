#ifndef BLOB_HDR
#define BLOB_HDR

#include "Platform.hpp"

//Memory blob to serialise versioned data.
//Uses a serialised offset to track where to read/write memory from/to. It also allocates offsets to track where to allocate 
//memory from when writing. This is so that relative structures like pointers and arrays can be serialised.

//TODO: When finalising and when reading, if the data version matches between the one written in the file and the root structure 
//is marked as 'relative only', memory mappable is doable and thus serialisation is automatic.

struct BlobHeader 
{
    uint32_t version = 0;
    uint32_t mappable = 0;
};

struct Blob 
{
    BlobHeader header = {};
};

#endif // !BLOB_HDR
