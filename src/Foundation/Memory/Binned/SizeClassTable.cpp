#include <Foundation/Memory/Binned/SizeClassTable.hpp>

namespace Engine::Memory {

SizeClassTable::SizeClassTable() {
    InitializeTables();
}

void SizeClassTable::InitializeTables() {
    m_blockSizeTable.clear();
    m_blockSizeTable.reserve(kNumSizeClasses);
    m_sizeToBinTable.assign(kMaxSize + 1, 0);

    uint32 currentBin = 0;
    
    struct BinRange {
        size_t limit;
        size_t step;
    };
    
    const BinRange ranges[] = {
        { 128, 8 },
        { 256, 16 },
        { 512, 32 },
        { 1024, 64 },
        { 2048, 128 },
        { 4096, 256 },
        { 8192, 512 },
        { 16384, 1024 },
        { 32768, 2048 }
    };
    
    size_t currentSize = 0;
    for (const auto& range : ranges) {
        while (currentSize < range.limit && currentBin < kNumSizeClasses) {
            size_t prevSize = currentSize;
            currentSize += range.step;
            m_blockSizeTable.push_back(currentSize);
            
            for (size_t s = prevSize + 1; s <= currentSize; ++s) {
                if (s < m_sizeToBinTable.size()) {
                    m_sizeToBinTable[s] = currentBin;
                }
            }
            currentBin++;
        }
    }
    
    // Ensure all sizes up to kMaxSize are covered by the last bin
    if (!m_blockSizeTable.empty()) {
        size_t lastSize = m_blockSizeTable.back();
        for (size_t s = lastSize + 1; s < m_sizeToBinTable.size(); ++s) {
            m_sizeToBinTable[s] = static_cast<uint32>(m_blockSizeTable.size() - 1);
        }
    }
}

uint32 SizeClassTable::GetSizeClass(size_t size) const {
    if (size == 0) return 0;
    if (size > kMaxSize) {
        return kInvalidIndex;
    }
    return m_sizeToBinTable[size];
}

size_t SizeClassTable::GetBlockSize(uint32 binIndex) const {
    if (binIndex >= m_blockSizeTable.size()) {
        return 0;
    }
    return m_blockSizeTable[binIndex];
}

} // namespace Engine::Memory
