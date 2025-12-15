#ifndef BIT_HDR
#define BIT_HDR

#include "Platform.hpp"

//VERY IMPORTANT: Remember Windows is little endian. So visually everything flipped. 
// We make use intrinsics in in bit.cpp, so be aware the output is "flipped".

struct Allocator;

static uint32_t bitMask8(uint32_t bit) { return 1 << (bit & 7); };
static uint32_t bitSlot8(uint32_t bit) { return bit / 8; }

uint32_t leadingZerosU32(uint32_t x);
#if defined(_MSC_VER)
uint32_t loadZerosU32msvc(uint32_t x);
#endif
uint32_t trailingZerosU32(uint32_t x);
uint64_t trailingZerosU64(uint64_t x);

uint32_t roundToPowerOf2(uint32_t value);

void printBinary(uint64_t number);
void printBinary(uint32_t number);

//An abstraction over a bitmask. It provides an easy way to iterator through the 
//indexes of the set of bits of a bitmask. When Shift = 0 (platform that support SSE),
//this is a true bitmask. On non-SSE, platforms the arithematic used to emulate the SSE behavior works in bytes (Shift = 3)
// and leaves each byte as either 0x00 or 0x80
//For example
//for(int i : BitMask<uint32_t, 16>(0x5)) -> yeilds 0, 2
//for(int i : BitMask<uint32_t, 8, 3>(0x0000000080800000)) -> yeilds 2, 3

//So shift 3 takes the a bit and pads to turn it into bytes.
template<typename T, int SignificantBits, int Shift = 0>
class BitMask 
{
public:
    explicit BitMask(T mask) : _mask(mask)
    {
    }

    BitMask& operator++() 
    {
        _mask &= (_mask - 1);
        return *this;
    }

    explicit operator bool() const 
    {
        return _mask != 0;
    }

    int operator*() const
    {
        return lowerBitSet();
    }

    uint32_t lowerBitSet() const 
    {
        //Example: _mask = 256
        //that's 8 zero then we bit shift by 3 making it 1 when we are in byte mode.
        //If we aren't in byte mode it's just 8.
        return trailingZerosU32(_mask) >> Shift;
    }

    uint32_t highestBitSet() const 
    {
        return leadingZerosU32(_mask) >> Shift;
    }

    BitMask begin() const 
    {
        return *this;
    }

    BitMask end() const 
    {
        return BitMask(0);
    }

    uint32_t trailingZeros() const 
    {
        return trailingZerosU32(_mask);
    }

    uint32_t leadingZeros() const 
    {
        return leadingZerosU32(_mask);
    }
private:
    friend bool operator==(const BitMask& a, const BitMask& b) 
    {
        return (a._mask == b._mask);
    }

    friend bool operator!=(const BitMask& a, const BitMask& b)
    {
        return (a._mask != b._mask);
    }

    T _mask;
};

struct BitSet 
{
    void init(Allocator* alloc, uint32_t totalBits);
    void shutdown();

    void resize(uint32_t totalBits);

    void setBit(uint32_t index);
    void clearBit(uint32_t index);
    uint8_t getBit(uint32_t index) const;

    Allocator* allocator = nullptr;
    uint8_t* bits = nullptr;
    uint32_t size = 0;
};

template<uint32_t SizeInBytes>
struct BitSetFixed 
{
    void setBit(uint32_t index)
    {
        bits[index / 8] |= bitMask8(index);
    }

    void clearBit(uint32_t index)
    {
        bits[index / 8] &= ~bitMask8(index);
    }

    uint8_t getBit(uint32_t index)
    {
        return bits[index / 8] & bitMask8(index);
    }

    uint8_t bits[SizeInBytes];
};

#endif // !BIT_HDR
