#ifndef FOUNDATION_CONTAINERS_BITARRAY_HDR
#define FOUNDATION_CONTAINERS_BITARRAY_HDR

#include <Platform.hpp>
#include <Engine/Utility/Assert.hpp>
#include <Engine/Memory/Memory.hpp>
#include <Engine/Utility/Move.hpp>
#include <Engine/TypeTraits/IteratorTraits.hpp>
#include <Engine/Algorithm/SimdOps.hpp>
#include <Engine/CompilerTraits.hpp>
#include <Engine/ArchitectureTraits.hpp>
#include <Engine/Memory/IAllocator.hpp>
#include <initializer_list>

namespace Engine::Containers {

/// @brief Dynamic bit array with efficient storage (1 bit per element)
/// @details Uses uint64 as the underlying storage unit for efficient 64-bit operations.
///          Provides O(1) access, O(1) amortized append, and efficient bulk operations.
class BitArray {
public:
    using size_type = size_t;
    using word_type = uint64;
    
    static constexpr size_type kBitsPerWord = sizeof(word_type) * 8;  // 64 bits
    
    // ========================================================================
    // Bit Reference (proxy for non-const access)
    // ========================================================================
    
    /// @brief Proxy class for bit references (allows array[i] = true syntax)
    class BitReference {
    public:
        BitReference(word_type& word, size_type bitIndex) noexcept
            : m_word(word), m_mask(word_type{1} << bitIndex) {}
        
        BitReference& operator=(bool value) noexcept {
            if (value) {
                m_word |= m_mask;
            } else {
                m_word &= ~m_mask;
            }
            return *this;
        }
        
        BitReference& operator=(const BitReference& other) noexcept {
            return *this = static_cast<bool>(other);
        }
        
        operator bool() const noexcept {
            return (m_word & m_mask) != 0;
        }
        
        bool operator~() const noexcept {
            return !static_cast<bool>(*this);
        }
        
        void Flip() noexcept {
            m_word ^= m_mask;
        }
        
    private:
        word_type& m_word;
        word_type m_mask;
    };
    
    // ========================================================================
    // iterator
    // ========================================================================
    
    /// @brief Forward iterator for iterating over bits
    class iterator {
    public:
        using iterator_category = ForwardIteratorTag;
        using value_type = bool;
        using difference_type = ptrdiff_t;
        using Pointer = void;
        using Reference = BitReference;
        
        iterator(word_type* data, size_type index) noexcept
            : m_data(data), m_index(index) {}
        
        BitReference operator*() const noexcept {
            return BitReference(m_data[m_index / kBitsPerWord], m_index % kBitsPerWord);
        }
        
        iterator& operator++() noexcept {
            ++m_index;
            return *this;
        }
        
        iterator operator++(int) noexcept {
            iterator tmp = *this;
            ++m_index;
            return tmp;
        }
        
        bool operator==(const iterator& other) const noexcept {
            return m_index == other.m_index;
        }
        
        bool operator!=(const iterator& other) const noexcept {
            return m_index != other.m_index;
        }
        
    private:
        word_type* m_data;
        size_type m_index;
    };
    
    /// @brief Const forward iterator for iterating over bits
    class const_iterator {
    public:
        using iterator_category = ForwardIteratorTag;
        using value_type = bool;
        using difference_type = ptrdiff_t;
        using Pointer = void;
        using Reference = bool;
        
        const_iterator(const word_type* data, size_type index) noexcept
            : m_data(data), m_index(index) {}
        
        bool operator*() const noexcept {
            return (m_data[m_index / kBitsPerWord] & (word_type{1} << (m_index % kBitsPerWord))) != 0;
        }
        
        const_iterator& operator++() noexcept {
            ++m_index;
            return *this;
        }
        
        const_iterator operator++(int) noexcept {
            const_iterator tmp = *this;
            ++m_index;
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const noexcept {
            return m_index == other.m_index;
        }
        
        bool operator!=(const const_iterator& other) const noexcept {
            return m_index != other.m_index;
        }
        
    private:
        const word_type* m_data;
        size_type m_index;
    };
    
    // ========================================================================
    // Constructors / Destructor
    // ========================================================================
    
    BitArray() noexcept = default;
    
    /// @brief Construct with given size, all bits set to initial value
    explicit BitArray(size_type count, bool value = false) {
        Resize(count, value);
    }
    
    /// @brief Construct from initializer list
    BitArray(std::initializer_list<bool> init) {
        Reserve(init.size());
        for (bool bit : init) {
            PushBack(bit);
        }
    }
    
    BitArray(const BitArray& other) {
        if (other.m_size > 0) {
            const size_type wordCount = WordsNeeded(other.m_size);
            m_data = Allocate(wordCount);
            MemCopy(m_data, other.m_data, wordCount * sizeof(word_type));
            m_size = other.m_size;
            m_capacity = wordCount * kBitsPerWord;
        }
    }
    
    BitArray(BitArray&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }
    
    ~BitArray() {
        if (m_data) {
            Deallocate(m_data);
        }
    }
    
    BitArray& operator=(const BitArray& other) {
        if (this != &other) {
            if (other.m_size == 0) {
                Clear();
            } else {
                const size_type wordCount = WordsNeeded(other.m_size);
                if (m_capacity < other.m_size) {
                    if (m_data) Deallocate(m_data);
                    m_data = Allocate(wordCount);
                    m_capacity = wordCount * kBitsPerWord;
                }
                MemCopy(m_data, other.m_data, wordCount * sizeof(word_type));
                m_size = other.m_size;
            }
        }
        return *this;
    }
    
    BitArray& operator=(BitArray&& other) noexcept {
        if (this != &other) {
            if (m_data) Deallocate(m_data);
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }
    
    // ========================================================================
    // Element Access
    // ========================================================================
    
    /// @brief Access bit at index (const)
    [[nodiscard]] bool operator[](size_type index) const noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        return GetBit(index);
    }
    
    /// @brief Access bit at index (mutable via BitReference)
    [[nodiscard]] BitReference operator[](size_type index) noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        return BitReference(m_data[index / kBitsPerWord], index % kBitsPerWord);
    }
    
    /// @brief Access bit at index with bounds checking
    [[nodiscard]] bool At(size_type index) const {
        ENGINE_ASSERT_INDEX(index, m_size);
        return GetBit(index);
    }
    
    /// @brief Access bit at index with bounds checking (mutable)
    [[nodiscard]] BitReference At(size_type index) {
        ENGINE_ASSERT_INDEX(index, m_size);
        return BitReference(m_data[index / kBitsPerWord], index % kBitsPerWord);
    }
    
    /// @brief Get the first bit
    [[nodiscard]] bool Front() const noexcept {
        ENGINE_ASSERT(m_size > 0);
        return GetBit(0);
    }
    
    /// @brief Get the last bit
    [[nodiscard]] bool Back() const noexcept {
        ENGINE_ASSERT(m_size > 0);
        return GetBit(m_size - 1);
    }
    
    /// @brief Direct access to underlying word storage
    [[nodiscard]] word_type* Data() noexcept { return m_data; }
    [[nodiscard]] const word_type* Data() const noexcept { return m_data; }
    
    // ========================================================================
    // Iterators
    // ========================================================================
    
    iterator begin() noexcept { return iterator(m_data, 0); }
    const_iterator begin() const noexcept { return const_iterator(m_data, 0); }
    const_iterator cbegin() const noexcept { return const_iterator(m_data, 0); }
    iterator end() noexcept { return iterator(m_data, m_size); }
    const_iterator end() const noexcept { return const_iterator(m_data, m_size); }
    const_iterator cend() const noexcept { return const_iterator(m_data, m_size); }
    
    // ========================================================================
    // Capacity
    // ========================================================================
    
    /// @brief Check if array is empty
    [[nodiscard]] bool IsEmpty() const noexcept { return m_size == 0; }
    
    /// @brief Get number of bits
    [[nodiscard]] size_type Size() const noexcept { return m_size; }
    
    /// @brief Get number of bits that can be stored without reallocation
    [[nodiscard]] size_type Capacity() const noexcept { return m_capacity; }
    
    /// @brief Get number of words used for storage
    [[nodiscard]] size_type WordCount() const noexcept { return WordsNeeded(m_size); }
    
    /// @brief Reserve capacity for at least n bits
    void Reserve(size_type newCapacity) {
        if (newCapacity > m_capacity) {
            Grow(newCapacity);
        }
    }
    
    /// @brief Shrink capacity to fit current size
    void ShrinkToFit() {
        if (m_size == 0) {
            if (m_data) {
                Deallocate(m_data);
                m_data = nullptr;
            }
            m_capacity = 0;
        } else if (m_capacity > WordsNeeded(m_size) * kBitsPerWord) {
            Reallocate(m_size);
        }
    }
    
    // ========================================================================
    // Modifiers
    // ========================================================================
    
    /// @brief Clear all bits (size becomes 0)
    void Clear() noexcept {
        m_size = 0;
    }
    
    /// @brief Add bit to the end
    void PushBack(bool value) {
        if (m_size >= m_capacity) {
            Grow(m_capacity == 0 ? kBitsPerWord : m_capacity * 2);
        }
        if (value) {
            SetBit(m_size);
        } else {
            ClearBit(m_size);
        }
        ++m_size;
    }
    
    /// @brief Remove last bit
    void PopBack() noexcept {
        ENGINE_ASSERT(m_size > 0);
        --m_size;
        // Clear the removed bit to maintain invariant that trailing bits are 0
        ClearBit(m_size);
    }
    
    /// @brief Resize array
    void Resize(size_type count, bool value = false) {
        if (count > m_size) {
            Reserve(count);
            // Set new bits to value
            if (value) {
                // Set bits from m_size to count-1
                for (size_type i = m_size; i < count; ++i) {
                    SetBit(i);
                }
            } else {
                // Clear bits from m_size to count-1
                for (size_type i = m_size; i < count; ++i) {
                    ClearBit(i);
                }
            }
        } else {
            // Clear bits beyond new size
            for (size_type i = count; i < m_size; ++i) {
                ClearBit(i);
            }
        }
        m_size = count;
    }
    
    /// @brief Set bit at index to true
    void Set(size_type index) noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        SetBit(index);
    }
    
    /// @brief Set bit at index to value
    void Set(size_type index, bool value) noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        if (value) {
            SetBit(index);
        } else {
            ClearBit(index);
        }
    }
    
    /// @brief Clear bit at index (set to false)
    void Reset(size_type index) noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        ClearBit(index);
    }
    
    /// @brief Flip bit at index
    void Flip(size_type index) noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        FlipBit(index);
    }
    
    /// @brief Set all bits to true
    void SetAll() noexcept {
        if (m_size == 0) return;
        const size_type fullWords = m_size / kBitsPerWord;
        const size_type remainingBits = m_size % kBitsPerWord;
        
        // Set full words to all 1s
        if (fullWords > 0) {
            ::Engine::Algorithm::SimdFill(m_data, m_data + fullWords, ~word_type{0});
        }
        
        // Set remaining bits
        if (remainingBits > 0) {
            m_data[fullWords] = (word_type{1} << remainingBits) - 1;
        }
    }
    
    /// @brief Clear all bits (set to false)
    void ResetAll() noexcept {
        if (m_size > 0) {
            ::Engine::Algorithm::SimdFill(m_data, m_data + WordsNeeded(m_size), word_type{0});
        }
    }
    
    /// @brief Flip all bits
    void FlipAll() noexcept {
        if (m_size == 0) return;
        const size_type fullWords = m_size / kBitsPerWord;
        const size_type remainingBits = m_size % kBitsPerWord;
        
        // Flip full words
        for (size_type i = 0; i < fullWords; ++i) {
            m_data[i] = ~m_data[i];
        }
        
        // Flip remaining bits (mask to avoid setting unused bits)
        if (remainingBits > 0) {
            const word_type mask = (word_type{1} << remainingBits) - 1;
            m_data[fullWords] = (~m_data[fullWords]) & mask;
        }
    }
    
    /// @brief Swap contents with another BitArray
    void Swap(BitArray& other) noexcept {
        ::Engine::Swap(m_data, other.m_data);
        ::Engine::Swap(m_size, other.m_size);
        ::Engine::Swap(m_capacity, other.m_capacity);
    }
    
    // ========================================================================
    // Query Operations
    // ========================================================================
    
    /// @brief Check if bit at index is set
    [[nodiscard]] bool Test(size_type index) const noexcept {
        ENGINE_ASSERT_INDEX(index, m_size);
        return GetBit(index);
    }
    
    /// @brief Check if any bit is set
    [[nodiscard]] bool Any() const noexcept {
        if (m_size == 0) return false;
        const size_type wordCount = WordsNeeded(m_size);
        for (size_type i = 0; i < wordCount; ++i) {
            if (m_data[i] != 0) return true;
        }
        return false;
    }
    
    /// @brief Check if all bits are set
    [[nodiscard]] bool All() const noexcept {
        if (m_size == 0) return true;  // vacuously true
        const size_type fullWords = m_size / kBitsPerWord;
        const size_type remainingBits = m_size % kBitsPerWord;
        
        // Check full words
        for (size_type i = 0; i < fullWords; ++i) {
            if (m_data[i] != ~word_type{0}) return false;
        }
        
        // Check remaining bits
        if (remainingBits > 0) {
            const word_type mask = (word_type{1} << remainingBits) - 1;
            if ((m_data[fullWords] & mask) != mask) return false;
        }
        
        return true;
    }
    
    /// @brief Check if no bits are set
    [[nodiscard]] bool None() const noexcept {
        return !Any();
    }
    
    /// @brief Count number of set bits
    [[nodiscard]] size_type Count() const noexcept {
        if (m_size == 0) return 0;
        size_type count = 0;
        const size_type wordCount = WordsNeeded(m_size);
        for (size_type i = 0; i < wordCount; ++i) {
            count += PopCount64(m_data[i]);
        }
        return count;
    }
    
    /// @brief Find first set bit (returns Size() if none found)
    [[nodiscard]] size_type FindFirstSet() const noexcept {
        if (m_size == 0) return 0;
        const size_type wordCount = WordsNeeded(m_size);
        for (size_type i = 0; i < wordCount; ++i) {
            if (m_data[i] != 0) {
                size_type bitPos = i * kBitsPerWord + CountTrailingZeros64(m_data[i]);
                return (bitPos < m_size) ? bitPos : m_size;
            }
        }
        return m_size;
    }
    
    /// @brief Find first clear bit (returns Size() if none found)
    [[nodiscard]] size_type FindFirstClear() const noexcept {
        if (m_size == 0) return 0;
        const size_type fullWords = m_size / kBitsPerWord;
        const size_type remainingBits = m_size % kBitsPerWord;
        
        // Check full words
        for (size_type i = 0; i < fullWords; ++i) {
            if (m_data[i] != ~word_type{0}) {
                return i * kBitsPerWord + CountTrailingZeros64(~m_data[i]);
            }
        }
        
        // Check remaining bits
        if (remainingBits > 0) {
            const word_type mask = (word_type{1} << remainingBits) - 1;
            const word_type masked = m_data[fullWords] | ~mask;  // Set unused bits to 1
            if (masked != ~word_type{0}) {
                size_type bitPos = fullWords * kBitsPerWord + CountTrailingZeros64(~masked);
                return (bitPos < m_size) ? bitPos : m_size;
            }
        }
        
        return m_size;
    }
    
    // ========================================================================
    // Bitwise Operations
    // ========================================================================
    
    /// @brief Bitwise AND with another BitArray
    BitArray& operator&=(const BitArray& other) noexcept {
        const size_type thisWords = WordsNeeded(m_size);
        const size_type otherWords = WordsNeeded(other.m_size);

        // Common hot path: same-sized masks (e.g. visibility bitsets).
        if (thisWords == otherWords) {
            AndWords(m_data, other.m_data, thisWords);
            return *this;
        }

        const size_type minWords = (thisWords < otherWords) ? thisWords : otherWords;
        AndWords(m_data, other.m_data, minWords);

        // Clear words that have no corresponding source bits in `other`.
        if (thisWords > minWords) {
            ::Engine::Algorithm::SimdFill(m_data + minWords, m_data + thisWords, word_type{0});
        }
        return *this;
    }
    
    /// @brief Bitwise OR with another BitArray
    BitArray& operator|=(const BitArray& other) noexcept {
        const size_type minWords = Min(WordsNeeded(m_size), WordsNeeded(other.m_size));
        OrWords(m_data, other.m_data, minWords);
        return *this;
    }
    
    /// @brief Bitwise XOR with another BitArray
    BitArray& operator^=(const BitArray& other) noexcept {
        const size_type minWords = Min(WordsNeeded(m_size), WordsNeeded(other.m_size));
        XorWords(m_data, other.m_data, minWords);
        return *this;
    }
    
    /// @brief Bitwise NOT (returns new BitArray)
    [[nodiscard]] BitArray operator~() const {
        BitArray result(*this);
        result.FlipAll();
        return result;
    }
    
    // ========================================================================
    // Comparison
    // ========================================================================
    
    [[nodiscard]] bool operator==(const BitArray& other) const noexcept {
        if (m_size != other.m_size) return false;
        if (m_size == 0) return true;
        const size_type wordCount = WordsNeeded(m_size);
        return MemCompare(m_data, other.m_data, wordCount * sizeof(word_type)) == 0;
    }
    
    [[nodiscard]] bool operator!=(const BitArray& other) const noexcept {
        return !(*this == other);
    }
    
private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    
    /// @brief Calculate number of words needed for n bits
    [[nodiscard]] static constexpr size_type WordsNeeded(size_type bits) noexcept {
        return (bits + kBitsPerWord - 1) / kBitsPerWord;
    }
    
    /// @brief Get bit value at index (no bounds check)
    [[nodiscard]] bool GetBit(size_type index) const noexcept {
        return (m_data[index / kBitsPerWord] & (word_type{1} << (index % kBitsPerWord))) != 0;
    }
    
    /// @brief Set bit at index to 1 (no bounds check)
    void SetBit(size_type index) noexcept {
        m_data[index / kBitsPerWord] |= (word_type{1} << (index % kBitsPerWord));
    }
    
    /// @brief Clear bit at index to 0 (no bounds check)
    void ClearBit(size_type index) noexcept {
        m_data[index / kBitsPerWord] &= ~(word_type{1} << (index % kBitsPerWord));
    }
    
    /// @brief Flip bit at index (no bounds check)
    void FlipBit(size_type index) noexcept {
        m_data[index / kBitsPerWord] ^= (word_type{1} << (index % kBitsPerWord));
    }
    
    /// @brief Allocate word array aligned to 32 bytes for AVX2
    [[nodiscard]] static word_type* Allocate(size_type wordCount) {
        word_type* ptr = static_cast<word_type*>(
            Engine::Memory::GetDefaultAllocator().Allocate(wordCount * sizeof(word_type), 32));
        // Zero-initialize to ensure unused bits are 0
        if (wordCount > 0) {
            ::Engine::Algorithm::SimdFill(ptr, ptr + wordCount, word_type{0});
        }
        return ptr;
    }
    
    /// @brief Deallocate word array
    static void Deallocate(word_type* ptr) {
        Engine::Memory::GetDefaultAllocator().Free(ptr);
    }
    
    /// @brief Grow capacity
    void Grow(size_type newCapacity) {
        Reallocate(newCapacity);
    }
    
    /// @brief Reallocate to new capacity
    void Reallocate(size_type newCapacity) {
        const size_type newWordCount = WordsNeeded(newCapacity);
        word_type* newData = Allocate(newWordCount);
        
        if (m_data && m_size > 0) {
            const size_type oldWordCount = WordsNeeded(m_size);
            MemCopy(newData, m_data, oldWordCount * sizeof(word_type));
        }
        
        if (m_data) Deallocate(m_data);
        m_data = newData;
        m_capacity = newWordCount * kBitsPerWord;
    }
    
    /// @brief Population count for 64-bit word
    [[nodiscard]] static size_type PopCount64(word_type x) noexcept {
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
        return static_cast<size_type>(__builtin_popcountll(x));
#elif defined(ENGINE_COMPILER_MSVC)
        return static_cast<size_type>(__popcnt64(x));
#else
        // Fallback: Brian Kernighan's algorithm
        size_type count = 0;
        while (x) {
            x &= x - 1;
            ++count;
        }
        return count;
#endif
    }
    
    /// @brief Count trailing zeros for 64-bit word
    [[nodiscard]] static size_type CountTrailingZeros64(word_type x) noexcept {
        if (x == 0) return 64;
#if defined(ENGINE_COMPILER_GCC) || defined(ENGINE_COMPILER_CLANG)
        return static_cast<size_type>(__builtin_ctzll(x));
#elif defined(ENGINE_COMPILER_MSVC)
        unsigned long index;
        _BitScanForward64(&index, x);
        return static_cast<size_type>(index);
#else
        // Fallback: de Bruijn sequence
        static constexpr int debruijn64[64] = {
            0,  1,  2,  53, 3,  7,  54, 27, 4,  38, 41, 8,  34, 55, 48, 28,
            62, 5,  39, 46, 44, 42, 22, 9,  24, 35, 59, 56, 49, 18, 29, 11,
            63, 52, 6,  26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
            51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12
        };
        return debruijn64[((x & -static_cast<int64>(x)) * 0x022fdd63cc95386dULL) >> 58];
#endif
    }

    /// @brief In-place bitwise AND over word arrays.
    static void AndWords(word_type* dst, const word_type* src, size_type wordCount) noexcept {
#if defined(ENGINE_SIMD_AVX2)
        constexpr size_type kWordsPerVector = 4;
        constexpr size_type kVectorsPerLoop = 4;
        constexpr size_type kWordsPerLoop = kWordsPerVector * kVectorsPerLoop;
        constexpr size_type kMinSimdWords = 32; // 256 bytes; below this scalar wins on setup overhead.
        
        if (wordCount < kMinSimdWords) {
            for (size_type i = 0; i < wordCount; ++i) dst[i] &= src[i];
            return;
        }

        size_type i = 0;
        const size_type unrollWords = wordCount & ~(kWordsPerLoop - 1);

        for (; i < unrollWords; i += kWordsPerLoop) {
            const __m256i l0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
            const __m256i r0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            const __m256i l1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 4));
            const __m256i r1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 4));
            const __m256i l2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 8));
            const __m256i r2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 8));
            const __m256i l3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 12));
            const __m256i r3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 12));

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_and_si256(l0, r0));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 4), _mm256_and_si256(l1, r1));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 8), _mm256_and_si256(l2, r2));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 12), _mm256_and_si256(l3, r3));
        }

        const size_type simdWords = wordCount & ~(kWordsPerVector - 1);
        for (; i < simdWords; i += kWordsPerVector) {
            const __m256i lhs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
            const __m256i rhs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_and_si256(lhs, rhs));
        }

        for (; i < wordCount; ++i) dst[i] &= src[i];
#else
        for (size_type i = 0; i < wordCount; ++i) dst[i] &= src[i];
#endif
    }

    /// @brief In-place bitwise OR over word arrays.
    static void OrWords(word_type* dst, const word_type* src, size_type wordCount) noexcept {
#if defined(ENGINE_SIMD_AVX2)
        constexpr size_type kWordsPerVector = 4;
        constexpr size_type kVectorsPerLoop = 4;
        constexpr size_type kWordsPerLoop = kWordsPerVector * kVectorsPerLoop;
        constexpr size_type kMinSimdWords = 32; // 256 bytes; below this scalar wins on setup overhead.
        
        if (wordCount < kMinSimdWords) {
            for (size_type i = 0; i < wordCount; ++i) dst[i] |= src[i];
            return;
        }

        size_type i = 0;
        const size_type unrollWords = wordCount & ~(kWordsPerLoop - 1);

        for (; i < unrollWords; i += kWordsPerLoop) {
            const __m256i l0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
            const __m256i r0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            const __m256i l1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 4));
            const __m256i r1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 4));
            const __m256i l2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 8));
            const __m256i r2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 8));
            const __m256i l3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 12));
            const __m256i r3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 12));

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_or_si256(l0, r0));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 4), _mm256_or_si256(l1, r1));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 8), _mm256_or_si256(l2, r2));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 12), _mm256_or_si256(l3, r3));
        }

        const size_type simdWords = wordCount & ~(kWordsPerVector - 1);
        for (; i < simdWords; i += kWordsPerVector) {
            const __m256i lhs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
            const __m256i rhs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_or_si256(lhs, rhs));
        }

        for (; i < wordCount; ++i) dst[i] |= src[i];
#else
        for (size_type i = 0; i < wordCount; ++i) dst[i] |= src[i];
#endif
    }

    /// @brief In-place bitwise XOR over word arrays.
    static void XorWords(word_type* dst, const word_type* src, size_type wordCount) noexcept {
#if defined(ENGINE_SIMD_AVX2)
        constexpr size_type kWordsPerVector = 4;
        constexpr size_type kVectorsPerLoop = 4;
        constexpr size_type kWordsPerLoop = kWordsPerVector * kVectorsPerLoop;
        constexpr size_type kMinSimdWords = 32; // 256 bytes; below this scalar wins on setup overhead.
        
        if (wordCount < kMinSimdWords) {
            for (size_type i = 0; i < wordCount; ++i) dst[i] ^= src[i];
            return;
        }

        size_type i = 0;
        const size_type unrollWords = wordCount & ~(kWordsPerLoop - 1);

        for (; i < unrollWords; i += kWordsPerLoop) {
            const __m256i l0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
            const __m256i r0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            const __m256i l1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 4));
            const __m256i r1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 4));
            const __m256i l2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 8));
            const __m256i r2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 8));
            const __m256i l3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i + 12));
            const __m256i r3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i + 12));

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_xor_si256(l0, r0));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 4), _mm256_xor_si256(l1, r1));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 8), _mm256_xor_si256(l2, r2));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 12), _mm256_xor_si256(l3, r3));
        }

        const size_type simdWords = wordCount & ~(kWordsPerVector - 1);
        for (; i < simdWords; i += kWordsPerVector) {
            const __m256i lhs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
            const __m256i rhs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_xor_si256(lhs, rhs));
        }

        for (; i < wordCount; ++i) dst[i] ^= src[i];
#else
        for (size_type i = 0; i < wordCount; ++i) dst[i] ^= src[i];
#endif
    }
    
    word_type* m_data = nullptr;
    size_type m_size = 0;        ///< Number of bits
    size_type m_capacity = 0;    ///< Capacity in bits
};

// ========================================================================
// Non-member Bitwise Operators
// ========================================================================

[[nodiscard]] inline BitArray operator&(const BitArray& lhs, const BitArray& rhs) {
    BitArray result(lhs);
    result &= rhs;
    return result;
}

[[nodiscard]] inline BitArray operator|(const BitArray& lhs, const BitArray& rhs) {
    BitArray result(lhs);
    result |= rhs;
    return result;
}

[[nodiscard]] inline BitArray operator^(const BitArray& lhs, const BitArray& rhs) {
    BitArray result(lhs);
    result ^= rhs;
    return result;
}

// Swap specialization
inline void swap(BitArray& lhs, BitArray& rhs) noexcept {
    lhs.Swap(rhs);
}

} // namespace Engine::Containers

namespace Engine::Containers {
using ::Engine::Containers::BitArray;
} // namespace Engine::Containers


#endif // !FOUNDATION_CONTAINERS_BITARRAY_HDR
