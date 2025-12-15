#include "Bit.hpp"
#include "log.hpp"
#include "Memory.hpp"

#if defined(_MSC_VER)
#include <immintrin.h>
#include <intrin0.h>
#endif

#include <string.h>

uint32_t leadingZerosU32(uint32_t x)
{
#if defined(_MSC_VER)
    return _lzcnt_u32(x);
#else
    return __builtin_ctz(x);
#endif
}

#if defined(_MSC_VER)
uint32_t loadZerosU32msvc(uint32_t x) 
{
    unsigned long result = 0;
    if(_BitScanReverse(&result, x))
    {
        return 31 - result;
    }

    return 32;
}
#endif

uint32_t trailingZerosU32(uint32_t x)
{
#if defined(_MSC_VER)
    return _tzcnt_u32(x);
#else
    return __builtin_clz(x);
#endif
}

uint64_t trailingZerosU64(uint64_t x) 
{
#if defined(_MSC_VER)
    return _tzcnt_u64(x);
#else
    return __builtin_ctzl(x);
#endif
}

uint32_t roundToPowerOf2(uint32_t value) 
{
    uint32_t nv = 1 << (32 - leadingZerosU32(value));
    return nv;
}

void printBinary(uint64_t number) 
{
    vprint("0b");
    unsigned sizeOfNumber = sizeof(number) * 8;
    for (uint32_t i = 0; i < sizeOfNumber; ++i)
    {
        uint32_t bit = (number >> (sizeOfNumber - i - 1)) & 0x1;
        vprint("%u", bit);
    }
    vprint("");
}

void printBinary(uint32_t number) 
{
    vprint("0b");
    unsigned sizeOfNumber = sizeof(number) * 8;
    for (uint32_t i = 0; i < sizeOfNumber; ++i)
    {
        uint32_t bit = (number >> (sizeOfNumber - i - 1)) & 0x1;
        vprint("%u", bit);
    }
    vprint("");
}

void BitSet::init(Allocator* alloc, uint32_t totalBits) 
{
    allocator = alloc;
    bits = nullptr;
    size = 0;

    resize(totalBits);
}

void BitSet::shutdown() 
{
    void_free(bits, allocator);
}

void BitSet::resize(uint32_t totalBits) 
{
    uint8_t* oldBits = bits;

    const uint32_t newSize = (totalBits + 7) / 8;
    if (size == newSize) 
    {
        return;
    }

    bits = (uint8_t*)void_allocam(newSize, allocator);

    if (oldBits) 
    {
        memcpy(bits, oldBits, size);
        void_free(oldBits, allocator);
    }
    else 
    {
        memset(bits, 0, newSize);
    }

    size = newSize;
}

void BitSet::setBit(uint32_t index) 
{
    bits[index / 8] |= bitMask8(index);
}

void BitSet::clearBit(uint32_t index) 
{
    bits[index / 8] &= ~bitMask8(index);
}

uint8_t BitSet::getBit(uint32_t index) const
{
    return bits[index / 8] & bitMask8(index);
}
