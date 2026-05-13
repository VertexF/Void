#ifndef FOUNDATION_MEMORY_SIZE_CLASS_TABLE_HDR
#define FOUNDATION_MEMORY_SIZE_CLASS_TABLE_HDR

#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Platform.hpp>

namespace Engine::Memory {

// Forward declaration
class SizeClassTable;

class SizeClassTable final {
public:
    // Constants for the size class table
    static constexpr size_t kMaxSize = 32768; // 32KB
    static constexpr uint32 kNumSizeClasses = 80; // Number of bins, based on ranges up to 32KB
    static constexpr uint32 kInvalidIndex = 0xFFFFFFFF;

    SizeClassTable(); // Default constructor

    // Maps a requested size to its corresponding bin index.
    // Returns 0 for size 0. Returns the index of the largest bin for sizes > kMaxSize.
    [[nodiscard]] uint32 GetSizeClass(size_t size) const;

    // Returns the actual block size for a given bin index.
    // Returns 0 for invalid binIndex.
    [[nodiscard]] size_t GetBlockSize(uint32 binIndex) const;

private:
    // Actual size of each bin in bytes
    Vector<size_t> m_blockSizeTable;
    // Maps a requested size to its bin index (size + 1 for array index)
    Vector<uint32> m_sizeToBinTable;

    // Private helper to calculate the number of bins more precisely
    void InitializeTables();
};

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_SIZE_CLASS_TABLE_HDR
