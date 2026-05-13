#include <Foundation/Memory/Alignment.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace Engine::Memory {

TEST(MemoryAlignment, AlignPointerAndAdjustment)
{
    alignas(1) uint8_t buffer[64]{};
    void* base = buffer + 1;

    void* aligned = AlignPointer(base, 8);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned) % 8u, 0u);
    EXPECT_GE(reinterpret_cast<uintptr_t>(aligned), reinterpret_cast<uintptr_t>(base));

    size_t adjustment = AlignmentAdjustment(base, 8);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(base) + adjustment,
              reinterpret_cast<uintptr_t>(aligned));
}

TEST(MemoryAlignment, AlignmentAdjustmentWithHeader)
{
    alignas(1) uint8_t buffer[64]{};
    constexpr size_t headerSize = 12;

    size_t adjustment = AlignmentAdjustmentWithHeader(buffer, 16, headerSize);
    auto* aligned = static_cast<uint8_t*>(static_cast<void*>(buffer)) + adjustment;

    EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned) % 16u, 0u);
    EXPECT_GE(adjustment, headerSize);
}

TEST(MemoryAlignment, PowerOfTwoAndAlignSize)
{
    EXPECT_TRUE(IsPowerOfTwo(1));
    EXPECT_TRUE(IsPowerOfTwo(2));
    EXPECT_TRUE(IsPowerOfTwo(8));
    EXPECT_FALSE(IsPowerOfTwo(0));
    EXPECT_FALSE(IsPowerOfTwo(3));
    EXPECT_FALSE(IsPowerOfTwo(6));

    EXPECT_EQ(AlignSize(1, 8), 8u);
    EXPECT_EQ(AlignSize(8, 8), 8u);
    EXPECT_EQ(AlignSize(9, 8), 16u);
}

TEST(MemoryAlignment, StressAlignments)
{
    alignas(1) uint8_t buffer[256]{};
    for (size_t alignment = 1; alignment <= 64; alignment <<= 1) {
        void* aligned = AlignPointer(buffer + 3, alignment);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned) % alignment, 0u);
    }
}

} // namespace Engine::Memory
