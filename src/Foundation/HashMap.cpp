#include "HashMap.hpp"

#include "Assert.hpp"

ProbeSequence::ProbeSequence(uint64_t hash, uint64_t mask) : 
    mask(mask), offset(hash & mask)
{
}

uint64_t ProbeSequence::getOffset() const 
{
    return offset;
}

uint64_t ProbeSequence::getOffset(uint64_t index) const 
{
    return (offset + index) & mask;
}

//This is based on a based-0 index.
uint64_t ProbeSequence::getIndex() const 
{
    return index;
}

void ProbeSequence::next() 
{
    index += WIDTH;
    offset += index;
    offset &= mask;
}

GroupSse2Impl::GroupSse2Impl(const int8_t* pos) 
{
    control = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pos));
}

uint32_t GroupSse2Impl::countLeadingEmptyOrDeleted() const 
{
    auto special = _mm_set1_epi8(CONTROL_BITMASK_SENTINEL);
    return trailingZerosU32(static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpgt_epi8(special, control)) + 1));
}

void GroupSse2Impl::convertSpecialToEmptyAndFullToDelete(int8_t* destination) const
{
    auto msbs = _mm_set1_epi8(static_cast<char>(-128));
    auto x126 = _mm_set1_epi8(126);

    if (SSSE3_SUPPORT)
    {
        auto shuffle = _mm_shuffle_epi8(x126, control);
        auto res = _mm_or_si128(shuffle, msbs);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination), res);
    }
    else
    {
        auto zero = _mm_setzero_si128();
        auto specialMask = _mm_cmpgt_epi8(zero, control);
        auto res = _mm_or_si128(msbs, _mm_andnot_si128(specialMask, x126));

        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination), res);
    }
}