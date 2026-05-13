#include <Foundation/Memory/Operations.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace Engine::Memory {
namespace {

struct TrivialPod {
    uint32_t A;
    uint32_t B;
};

} // namespace

TEST(MemoryConvenience, ZeroStructClearsTrivialObject)
{
    TrivialPod pod{0xFFFFFFFFu, 0x12345678u};

    ZeroStruct(pod);

    EXPECT_EQ(pod.A, 0u);
    EXPECT_EQ(pod.B, 0u);
}

TEST(MemoryConvenience, ZeroInitReturnsZeroedTrivialObject)
{
    const TrivialPod pod = ZeroInit<TrivialPod>();

    EXPECT_EQ(pod.A, 0u);
    EXPECT_EQ(pod.B, 0u);
}

TEST(MemoryConvenience, MemCompareOrdersDifferentBuffers)
{
    std::array<uint8_t, 4> low{1, 2, 3, 4};
    std::array<uint8_t, 4> high{1, 2, 3, 5};

    EXPECT_LT(MemCompare(low.data(), high.data(), low.size()), 0);
    EXPECT_GT(MemCompare(high.data(), low.data(), low.size()), 0);
    EXPECT_EQ(MemCompare(low.data(), low.data(), low.size()), 0);
}

TEST(MemoryConvenience, FastCopyHandlesNonPowerOfTwoSize)
{
    std::array<uint8_t, 17> source{};
    std::array<uint8_t, 17> destination{};

    for (size_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<uint8_t>(i * 3u);
    }

    FastCopy<17>(destination.data(), source.data());
    EXPECT_EQ(MemCompare(destination.data(), source.data(), source.size()), 0);
}

} // namespace Engine::Memory
