#ifndef FOUNDATION_CONTAINERS_CHUNKED_ARRAY_HDR
#define FOUNDATION_CONTAINERS_CHUNKED_ARRAY_HDR

#include <Foundation/Assert.hpp>
#include <Foundation/CompilerTraits.hpp>
#include <Foundation/Containers/Array.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Platform.hpp>

#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#if defined(NDEBUG)
    #define ENGINE_CHUNKED_ARRAY_ASSERT(condition) ((void)0)
#else
    #define ENGINE_CHUNKED_ARRAY_ASSERT(condition) VOID_ASSERT(condition)
#endif

namespace Engine::Containers {
namespace Detail {

[[nodiscard]] constexpr bool IsPowerOfTwo(uint32 value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

[[nodiscard]] constexpr uint32 Log2PowerOfTwo(uint32 value) noexcept
{
    uint32 shift = 0;
    while ((uint32{1} << shift) < value) {
        ++shift;
    }
    return shift;
}

} // namespace Detail

template<typename T, uint32 ChunkCapacity = 256>
class ChunkedArray {
    static_assert(!std::is_reference_v<T>, "ChunkedArray cannot store references");
    static_assert(!std::is_const_v<T>, "ChunkedArray cannot store const element types");
    static_assert(Detail::IsPowerOfTwo(ChunkCapacity), "ChunkedArray chunk capacity must be a power of two");

    struct Chunk {
        alignas(T) uint8 storage[sizeof(T) * ChunkCapacity];

        [[nodiscard]] T* Data() noexcept
        {
            return std::launder(reinterpret_cast<T*>(storage));
        }

        [[nodiscard]] const T* Data() const noexcept
        {
            return std::launder(reinterpret_cast<const T*>(storage));
        }
    };

public:
    using value_type = T;
    using size_type = uint32;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    static constexpr size_type kChunkCapacity = ChunkCapacity;
    static constexpr size_type kChunkShift = Detail::Log2PowerOfTwo(ChunkCapacity);
    static constexpr size_type kChunkMask = ChunkCapacity - 1;

    class AppendWriter {
    public:
        AppendWriter() = default;

        AppendWriter(const AppendWriter&) = delete;
        AppendWriter& operator=(const AppendWriter&) = delete;

        AppendWriter(AppendWriter&& other) noexcept
            : m_owner(other.m_owner),
              m_start(other.m_start),
              m_reserved(other.m_reserved),
              m_written(other.m_written),
              m_current(other.m_current),
              m_remainingInChunk(other.m_remainingInChunk),
              m_committed(other.m_committed)
        {
            other.m_owner = nullptr;
            other.m_current = nullptr;
            other.m_committed = true;
        }

        AppendWriter& operator=(AppendWriter&& other) noexcept
        {
            if (this != &other) {
                Rollback();
                m_owner = other.m_owner;
                m_start = other.m_start;
                m_reserved = other.m_reserved;
                m_written = other.m_written;
                m_current = other.m_current;
                m_remainingInChunk = other.m_remainingInChunk;
                m_committed = other.m_committed;
                other.m_owner = nullptr;
                other.m_current = nullptr;
                other.m_committed = true;
            }
            return *this;
        }

        ~AppendWriter()
        {
            Rollback();
        }

        [[nodiscard]] bool IsValid() const noexcept { return m_owner != nullptr; }
        [[nodiscard]] size_type Count() const noexcept { return m_written; }
        [[nodiscard]] size_type Remaining() const noexcept { return m_reserved - m_written; }

        template<typename... Args>
        reference Emplace(Args&&... args)
            noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_constructible_v<T, Args...>)
        {
            ENGINE_CHUNKED_ARRAY_ASSERT(m_owner != nullptr);
            ENGINE_CHUNKED_ARRAY_ASSERT(m_written < m_reserved);

            if (m_remainingInChunk == 0) {
                LoadCursor();
            }

            pointer element = m_current;
            ::new (static_cast<void*>(element)) T(std::forward<Args>(args)...);
            ++m_current;
            --m_remainingInChunk;
            ++m_written;
            return *element;
        }

        void Push(const T& value)
            noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
        {
            Emplace(value);
        }

        void Push(T&& value)
            noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_move_constructible_v<T>)
        {
            Emplace(std::move(value));
        }

        void Commit() noexcept
        {
            ENGINE_CHUNKED_ARRAY_ASSERT(m_owner != nullptr);
            ENGINE_CHUNKED_ARRAY_ASSERT(m_owner->m_size == m_start);
            m_owner->m_size = m_start + m_written;
            m_committed = true;
            m_owner = nullptr;
            m_current = nullptr;
        }

        void Rollback() noexcept
        {
            if (m_owner && !m_committed) {
                m_owner->DestroyElements(m_start, m_written);
            }
            m_owner = nullptr;
            m_current = nullptr;
            m_start = 0;
            m_reserved = 0;
            m_written = 0;
            m_remainingInChunk = 0;
            m_committed = true;
        }

    private:
        friend class ChunkedArray;

        AppendWriter(ChunkedArray& owner, size_type start, size_type reserved) noexcept
            : m_owner(&owner),
              m_start(start),
              m_reserved(reserved)
        {
            if (reserved > 0) {
                LoadCursor();
            }
        }

        void LoadCursor() noexcept
        {
            const size_type index = m_start + m_written;
            m_current = m_owner->ElementPointer(index);
            m_remainingInChunk = ChunkCapacity - LocalIndex(index);
        }

        ChunkedArray* m_owner = nullptr;
        size_type m_start = 0;
        size_type m_reserved = 0;
        size_type m_written = 0;
        pointer m_current = nullptr;
        size_type m_remainingInChunk = 0;
        bool m_committed = false;
    };

    ChunkedArray()
        : ChunkedArray(Memory::GetDefaultAllocator())
    {
    }

    explicit ChunkedArray(Memory::IAllocator& allocator)
        : m_chunks(allocator),
          m_allocator(&allocator)
    {
    }

    ChunkedArray(const ChunkedArray&) = delete;
    ChunkedArray& operator=(const ChunkedArray&) = delete;

    ChunkedArray(ChunkedArray&& other) noexcept
        : m_chunks(std::move(other.m_chunks)),
          m_size(other.m_size),
          m_allocator(&other.Allocator())
    {
        other.m_size = 0;
    }

    ChunkedArray& operator=(ChunkedArray&& other) noexcept
    {
        if (this != &other) {
            Clear();
            ReleaseMemory();
            m_chunks = std::move(other.m_chunks);
            m_size = other.m_size;
            m_allocator = &other.Allocator();
            other.m_size = 0;
        }
        return *this;
    }

    ~ChunkedArray()
    {
        Clear();
        ReleaseMemory();
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference operator[](size_type index) noexcept
    {
        ENGINE_CHUNKED_ARRAY_ASSERT(index < m_size);
        return *ElementPointer(index);
    }

    [[nodiscard]] ENGINE_FORCE_INLINE const_reference operator[](size_type index) const noexcept
    {
        ENGINE_CHUNKED_ARRAY_ASSERT(index < m_size);
        return *ElementPointer(index);
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference At(size_type index) noexcept { return (*this)[index]; }
    [[nodiscard]] ENGINE_FORCE_INLINE const_reference At(size_type index) const noexcept { return (*this)[index]; }
    [[nodiscard]] ENGINE_FORCE_INLINE bool IsEmpty() const noexcept { return m_size == 0; }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type Size() const noexcept { return m_size; }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type Capacity() const noexcept { return m_chunks.Size() * ChunkCapacity; }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type ChunkCount() const noexcept { return m_chunks.Size(); }
    [[nodiscard]] Memory::IAllocator& Allocator() const noexcept { return *m_allocator; }

    [[nodiscard]] bool TryReserve(size_type capacity)
    {
        const size_type requiredChunks = ChunkCountForCapacity(capacity);
        while (m_chunks.Size() < requiredChunks) {
            Chunk* chunk = AllocateChunk();
            if (!chunk) {
                return false;
            }
            if (!m_chunks.TryPushBack(chunk)) {
                FreeChunk(chunk);
                return false;
            }
        }
        return true;
    }

    void Reserve(size_type capacity)
    {
        if (!TryReserve(capacity)) {
            FatalAllocationFailure();
        }
    }

    template<typename... Args>
    [[nodiscard]] pointer TryEmplaceBack(Args&&... args)
    {
        if (m_size == (std::numeric_limits<size_type>::max)()) {
            return nullptr;
        }
        if (!TryReserve(m_size + 1)) {
            return nullptr;
        }

        pointer element = ElementPointer(m_size);
        ::new (static_cast<void*>(element)) T(std::forward<Args>(args)...);
        ++m_size;
        return element;
    }

    template<typename... Args>
    reference EmplaceBack(Args&&... args)
    {
        pointer element = TryEmplaceBack(std::forward<Args>(args)...);
        if (!element) {
            FatalAllocationFailure();
        }
        return *element;
    }

    void PushBack(const T& value)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
    {
        EmplaceBack(value);
    }

    void PushBack(T&& value)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_move_constructible_v<T>)
    {
        EmplaceBack(std::move(value));
    }

    [[nodiscard]] AppendWriter TryBeginAppend(size_type maxCount)
    {
        if (maxCount > (std::numeric_limits<size_type>::max)() - m_size) {
            return {};
        }

        const size_type start = m_size;
        if (!TryReserve(start + maxCount)) {
            return {};
        }
        return AppendWriter(*this, start, maxCount);
    }

    [[nodiscard]] AppendWriter BeginAppend(size_type maxCount)
    {
        AppendWriter writer = TryBeginAppend(maxCount);
        if (!writer.IsValid() && maxCount > 0) {
            FatalAllocationFailure();
        }
        return writer;
    }

    [[nodiscard]] bool TryAppendRange(const_pointer source, size_type count)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
    {
        ENGINE_CHUNKED_ARRAY_ASSERT(source != nullptr || count == 0);
        if (count == 0) {
            return true;
        }
        if (!source || count > (std::numeric_limits<size_type>::max)() - m_size) {
            return false;
        }

        const size_type start = m_size;
        if (!TryReserve(start + count)) {
            return false;
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            size_type copied = 0;
            while (copied < count) {
                const size_type index = start + copied;
                const size_type remainingInChunk = ChunkCapacity - LocalIndex(index);
                const size_type copyCount = Min(count - copied, remainingInChunk);
                MemCopy(ElementPointer(index), source + copied, static_cast<usize>(copyCount) * sizeof(T));
                copied += copyCount;
            }
            m_size = start + count;
        } else {
            size_type constructed = 0;
#if ENGINE_ENABLE_EXCEPTIONS
            try {
#endif
                for (; constructed < count; ++constructed) {
                    ::new (static_cast<void*>(ElementPointer(start + constructed))) T(source[constructed]);
                }
                m_size = start + count;
#if ENGINE_ENABLE_EXCEPTIONS
            } catch (...) {
                DestroyElements(start, constructed);
                throw;
            }
#endif
        }

        return true;
    }

    void AppendRange(const_pointer source, size_type count)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
    {
        if (!TryAppendRange(source, count)) {
            FatalAllocationFailure();
        }
    }

    void Clear() noexcept
    {
        DestroyElements(0, m_size);
        m_size = 0;
    }

    void ReleaseMemory() noexcept
    {
        for (size_type index = 0; index < m_chunks.Size(); ++index) {
            FreeChunk(m_chunks[index]);
        }
        m_chunks.Clear();
        m_chunks.ShrinkToFit();
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference front() noexcept { return (*this)[0]; }
    [[nodiscard]] ENGINE_FORCE_INLINE const_reference front() const noexcept { return (*this)[0]; }
    [[nodiscard]] ENGINE_FORCE_INLINE bool empty() const noexcept { return IsEmpty(); }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type size() const noexcept { return Size(); }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type capacity() const noexcept { return Capacity(); }
    void reserve(size_type capacity) { Reserve(capacity); }
    void clear() noexcept { Clear(); }
    template<typename... Args>
    reference emplace_back(Args&&... args) { return EmplaceBack(std::forward<Args>(args)...); }
    void push_back(const T& value) { PushBack(value); }
    void push_back(T&& value) { PushBack(std::move(value)); }
    void append_range(const_pointer source, size_type count) { AppendRange(source, count); }
    [[nodiscard]] AppendWriter begin_append(size_type maxCount) { return BeginAppend(maxCount); }

private:
    [[nodiscard]] static constexpr size_type ChunkIndex(size_type index) noexcept
    {
        return index >> kChunkShift;
    }

    [[nodiscard]] static constexpr size_type LocalIndex(size_type index) noexcept
    {
        return index & kChunkMask;
    }

    [[nodiscard]] static constexpr size_type ChunkCountForCapacity(size_type capacity) noexcept
    {
        return capacity == 0 ? 0 : ((capacity - 1) >> kChunkShift) + 1;
    }

    [[nodiscard]] static constexpr size_type Min(size_type lhs, size_type rhs) noexcept
    {
        return lhs < rhs ? lhs : rhs;
    }

    [[nodiscard]] pointer ElementPointer(size_type index) noexcept
    {
        return m_chunks[ChunkIndex(index)]->Data() + LocalIndex(index);
    }

    [[nodiscard]] const_pointer ElementPointer(size_type index) const noexcept
    {
        return m_chunks[ChunkIndex(index)]->Data() + LocalIndex(index);
    }

    [[nodiscard]] Chunk* AllocateChunk()
    {
        void* memory = Allocator().Allocate(sizeof(Chunk), alignof(Chunk));
        if (!memory) {
            return nullptr;
        }
        return new (memory) Chunk();
    }

    void FreeChunk(Chunk* chunk) noexcept
    {
        if (chunk) {
            chunk->~Chunk();
            Allocator().Free(chunk);
        }
    }

    void DestroyElements(size_type first, size_type count) noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type index = 0; index < count; ++index) {
                ElementPointer(first + index)->~T();
            }
        } else {
            (void)first;
            (void)count;
        }
    }

    [[noreturn]] static void FatalAllocationFailure()
    {
        VOID_ASSERT(false);
        std::abort();
    }

    Array<Chunk*> m_chunks;
    size_type m_size = 0;
    Memory::IAllocator* m_allocator = nullptr;
};

} // namespace Engine::Containers

namespace Engine {
using ::Engine::Containers::ChunkedArray;
}

#undef ENGINE_CHUNKED_ARRAY_ASSERT

#endif // FOUNDATION_CONTAINERS_CHUNKED_ARRAY_HDR
