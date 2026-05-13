#include <Foundation/Memory/Binned/SizeClassTable.hpp>
#include <Foundation/Platform.hpp>
#include <gtest/gtest.h>

namespace Engine::Memory {

TEST(MAL_SizeClassTable, CorrectBinning)
{
    SizeClassTable table;
    
    // First 128 bytes: 8-byte increments
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(1)), 8);
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(8)), 8);
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(9)), 16);
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(128)), 128);
    
    // Next range: 128-256 (16-byte increments)
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(129)), 144);
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(144)), 144);
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(145)), 160);
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(256)), 256);
    
    // Check boundary at 32KB
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(32768)), 32768);
    
    // Check OIB (out of binned) behavior - now returns 0 (invalid)
    EXPECT_EQ(table.GetBlockSize(table.GetSizeClass(40000)), 0);
}

TEST(MAL_SizeClassTable, WasteAnalysis)
{
    SizeClassTable table;
    for (size_t s = 129; s <= 32768; ++s)
    {
        uint32 bin = table.GetSizeClass(s);
        size_t blockSize = table.GetBlockSize(bin);
        ASSERT_GE(blockSize, s) << "Size " << s << " mapped to smaller bin " << blockSize;
        
        double waste = (double)(blockSize - s) / blockSize;
        // Maximum waste should be at (BinSize - (PrevBinSize + 1)) / BinSize
        // For 128->144: (144-129)/144 = 0.104
        // We'll allow up to 12% to be safe with this increment strategy.
        EXPECT_LT(waste, 0.12) << "Waste too high for size " << s << " (Bin: " << blockSize << ", Waste: " << waste << ")";
    }
}

TEST(MAL_SizeClassTable, Consistency)
{
    SizeClassTable table;
    uint32 numBins = 0;
    // We expect some bins
    while (table.GetBlockSize(numBins) != 0) {
        numBins++;
    }
    
    EXPECT_GT(numBins, 0u);
    
    size_t lastSize = 0;
    for (uint32 i = 0; i < numBins; ++i) {
        size_t size = table.GetBlockSize(i);
        EXPECT_GT(size, lastSize);
        lastSize = size;
    }
}

} // namespace Engine::Memory
