#include <Foundation/Memory/Operations.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace Engine::Memory {

TEST(MemoryOperations, MemCopyCopiesBytes)
{
    std::array<uint8_t, 32> source{};
    std::array<uint8_t, 32> destination{};

    for (size_t i = 0; i < source.size(); ++i) {
        source[i] = static_cast<uint8_t>(i + 3u);
    }

    EXPECT_EQ(MemCopy(destination.data(), source.data(), source.size()), destination.data());
    EXPECT_EQ(MemCompare(destination.data(), source.data(), source.size()), 0);
}

TEST(MemoryOperations, MemMoveHandlesOverlap)
{
    std::array<uint8_t, 8> bytes{0, 1, 2, 3, 4, 5, 6, 7};

    MemMove(bytes.data() + 2, bytes.data(), 4);

    EXPECT_EQ(bytes[0], 0);
    EXPECT_EQ(bytes[1], 1);
    EXPECT_EQ(bytes[2], 0);
    EXPECT_EQ(bytes[3], 1);
    EXPECT_EQ(bytes[4], 2);
    EXPECT_EQ(bytes[5], 3);
}

TEST(MemoryOperations, MemSetAndMemZeroWriteExpectedBytes)
{
    std::array<uint8_t, 16> bytes{};

    MemSet(bytes.data(), 0xAB, bytes.size());
    for (uint8_t value : bytes) {
        EXPECT_EQ(value, 0xAB);
    }

    MemZero(bytes.data(), bytes.size());
    for (uint8_t value : bytes) {
        EXPECT_EQ(value, 0u);
    }
}

TEST(MemoryOperations, FastCopyAndFastSetHandleCompileTimeSizes)
{
    std::array<uint8_t, 32> source{};
    std::array<uint8_t, 32> destination{};

    FastSet<32>(source.data(), 0x5A);
    FastCopy<32>(destination.data(), source.data());

    EXPECT_EQ(MemCompare(destination.data(), source.data(), source.size()), 0);
}

} // namespace Engine::Memory
