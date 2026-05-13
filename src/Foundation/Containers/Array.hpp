#ifndef FOUNDATION_CONTAINERS_ARRAY_HDR
#define FOUNDATION_CONTAINERS_ARRAY_HDR

#include <Foundation/Assert.hpp>
#include <Foundation/CompilerTraits.hpp>
#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Platform.hpp>

#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#ifndef ENGINE_ENABLE_EXCEPTIONS
    #define ENGINE_ENABLE_EXCEPTIONS 0
#endif

#if defined(NDEBUG)
    #define ENGINE_ARRAY_ASSERT(condition) ((void)0)
#else
    #define ENGINE_ARRAY_ASSERT(condition) VOID_ASSERT(condition)
#endif

namespace Engine::Containers {
namespace Detail {

template<typename T, usize InlineCapacity>
struct ArrayInlineStorage {
    alignas(T) uint8 bytes[sizeof(T) * InlineCapacity];

    [[nodiscard]] T* Data() noexcept
    {
        return std::launder(reinterpret_cast<T*>(bytes));
    }

    [[nodiscard]] const T* Data() const noexcept
    {
        return std::launder(reinterpret_cast<const T*>(bytes));
    }
};

template<typename T>
struct ArrayInlineStorage<T, 0> {
    [[nodiscard]] T* Data() noexcept { return nullptr; }
    [[nodiscard]] const T* Data() const noexcept { return nullptr; }
};

} // namespace Detail

template<typename T, usize InlineCapacity = 0>
class Array {
    static_assert(!std::is_reference_v<T>, "Array cannot store references");
    static_assert(!std::is_const_v<T>, "Array cannot store const element types");

public:
    using value_type = T;
    using size_type = uint32;
    using difference_type = isize;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using stored_size_type = size_type;

    static constexpr size_type kInlineCapacity = InlineCapacity;
    static_assert(InlineCapacity <= (std::numeric_limits<stored_size_type>::max)(),
        "Array inline capacity exceeds supported element count");

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
              m_committed(other.m_committed)
        {
            other.m_owner = nullptr;
            other.m_start = 0;
            other.m_reserved = 0;
            other.m_written = 0;
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
                m_committed = other.m_committed;
                other.m_owner = nullptr;
                other.m_start = 0;
                other.m_reserved = 0;
                other.m_written = 0;
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
        [[nodiscard]] pointer Data() noexcept { return m_owner ? m_owner->m_data + m_start : nullptr; }
        [[nodiscard]] const_pointer Data() const noexcept { return m_owner ? m_owner->m_data + m_start : nullptr; }

        template<typename... Args>
        reference Emplace(Args&&... args)
            noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_constructible_v<T, Args...>)
        {
            ENGINE_ARRAY_ASSERT(m_owner != nullptr);
            ENGINE_ARRAY_ASSERT(m_written < m_reserved);
            pointer element = m_owner->m_data + m_start + m_written;
            ::new (static_cast<void*>(element)) T(std::forward<Args>(args)...);
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
            ENGINE_ARRAY_ASSERT(m_owner != nullptr);
            ENGINE_ARRAY_ASSERT(m_owner->m_size == m_start);
            m_owner->m_size = static_cast<stored_size_type>(m_start + m_written);
            m_committed = true;
            m_owner = nullptr;
        }

        void Rollback() noexcept
        {
            if (m_owner && !m_committed) {
                DestroyRange(m_owner->m_data + m_start, m_written);
            }
            m_owner = nullptr;
            m_start = 0;
            m_reserved = 0;
            m_written = 0;
            m_committed = true;
        }

    private:
        friend class Array;

        AppendWriter(Array& owner, size_type start, size_type reserved) noexcept
            : m_owner(&owner),
              m_start(start),
              m_reserved(reserved)
        {
        }

        Array* m_owner = nullptr;
        size_type m_start = 0;
        size_type m_reserved = 0;
        size_type m_written = 0;
        bool m_committed = false;
    };

    Array() noexcept
        : Array(Memory::GetDefaultAllocator())
    {
    }

    explicit Array(Memory::IAllocator& allocator) noexcept
        : m_data(InlineData()),
          m_capacity(static_cast<stored_size_type>(InlineCapacity)),
          m_allocator(&allocator)
    {
    }

    explicit Array(size_type count)
        : Array()
    {
        Resize(count);
    }

    Array(size_type count, Memory::IAllocator& allocator)
        : Array(allocator)
    {
        Resize(count);
    }

    Array(size_type count, const T& value)
        : Array()
    {
        Assign(count, value);
    }

    Array(size_type count, const T& value, Memory::IAllocator& allocator)
        : Array(allocator)
    {
        Assign(count, value);
    }

    Array(std::initializer_list<T> values)
        : Array()
    {
        Assign(values.begin(), values.end());
    }

    Array(std::initializer_list<T> values, Memory::IAllocator& allocator)
        : Array(allocator)
    {
        Assign(values.begin(), values.end());
    }

    Array(const Array& other)
        : Array(other.Allocator())
    {
        Assign(other.begin(), other.end());
    }

    Array(const Array& other, Memory::IAllocator& allocator)
        : Array(allocator)
    {
        Assign(other.begin(), other.end());
    }

    Array(Array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : Array(other.Allocator())
    {
        MoveFrom(std::move(other));
    }

    ~Array() noexcept
    {
        Clear();
        FreeHeapStorage();
    }

    Array& operator=(const Array& other)
    {
        if (this != &other) {
            Array copy(other, Allocator());
            Swap(copy);
        }
        return *this;
    }

    Array& operator=(Array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (this != &other) {
            Clear();
            FreeHeapStorage();
            m_allocator = &other.Allocator();
            ResetToInlineStorage();
            MoveFrom(std::move(other));
        }
        return *this;
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference operator[](size_type index) noexcept
    {
        ENGINE_ARRAY_ASSERT(index < m_size);
        return m_data[index];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE const_reference operator[](size_type index) const noexcept
    {
        ENGINE_ARRAY_ASSERT(index < m_size);
        return m_data[index];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference At(size_type index) noexcept
    {
        ENGINE_ARRAY_ASSERT(index < m_size);
        return m_data[index];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE const_reference At(size_type index) const noexcept
    {
        ENGINE_ARRAY_ASSERT(index < m_size);
        return m_data[index];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference Front() noexcept
    {
        ENGINE_ARRAY_ASSERT(m_size > 0);
        return m_data[0];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE const_reference Front() const noexcept
    {
        ENGINE_ARRAY_ASSERT(m_size > 0);
        return m_data[0];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference Back() noexcept
    {
        ENGINE_ARRAY_ASSERT(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE const_reference Back() const noexcept
    {
        ENGINE_ARRAY_ASSERT(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] ENGINE_FORCE_INLINE pointer Data() noexcept { return m_data; }
    [[nodiscard]] ENGINE_FORCE_INLINE const_pointer Data() const noexcept { return m_data; }

    [[nodiscard]] iterator begin() noexcept { return m_data; }
    [[nodiscard]] const_iterator begin() const noexcept { return m_data; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return m_data; }
    [[nodiscard]] iterator end() noexcept { return m_data + m_size; }
    [[nodiscard]] const_iterator end() const noexcept { return m_data + m_size; }
    [[nodiscard]] const_iterator cend() const noexcept { return m_data + m_size; }

    [[nodiscard]] ENGINE_FORCE_INLINE bool IsEmpty() const noexcept { return m_size == 0; }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type Size() const noexcept { return static_cast<size_type>(m_size); }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type Capacity() const noexcept { return static_cast<size_type>(m_capacity); }
    [[nodiscard]] size_type SizeInBytes() const noexcept { return static_cast<size_type>(m_size) * sizeof(T); }
    [[nodiscard]] size_type CapacityInBytes() const noexcept { return static_cast<size_type>(m_capacity) * sizeof(T); }
    [[nodiscard]] bool UsingInlineStorage() const noexcept { return m_data == InlineData(); }
    [[nodiscard]] Memory::IAllocator& Allocator() const noexcept { return *m_allocator; }

    void SetAllocator(Memory::IAllocator& allocator) noexcept
    {
        ENGINE_ARRAY_ASSERT(m_size == 0 && UsingInlineStorage());
        m_allocator = &allocator;
    }

    [[nodiscard]] bool TryReserve(size_type newCapacity)
    {
        if (!CanRepresentSize(newCapacity)) {
            return false;
        }
        if (newCapacity <= static_cast<size_type>(m_capacity)) {
            return true;
        }
        return Reallocate(newCapacity);
    }

    void Reserve(size_type newCapacity)
    {
        if (!TryReserve(newCapacity)) {
            FatalAllocationFailure();
        }
    }

    void ShrinkToFit()
    {
        if (m_size == m_capacity) {
            return;
        }

        if constexpr (InlineCapacity > 0) {
            if (m_size <= InlineCapacity && !UsingInlineStorage()) {
                pointer oldData = m_data;
                const size_type oldSize = m_size;
                m_data = InlineData();
                m_capacity = static_cast<stored_size_type>(InlineCapacity);
                RelocateConstructRange(m_data, oldData, oldSize);
                DestroyRange(oldData, oldSize);
                Allocator().Free(oldData);
                return;
            }
        }

        if (m_size == 0) {
            FreeHeapStorage();
            ResetToInlineStorage();
            return;
        }

        (void)Reallocate(m_size);
    }

    void Clear() noexcept
    {
        DestroyRange(m_data, m_size);
        m_size = 0;
    }

    [[nodiscard]] ENGINE_FORCE_INLINE bool TryPushBack(const T& value)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
    {
        if (m_size >= m_capacity && !GrowForAppend()) {
            return false;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            m_data[m_size++] = value;
        } else {
            ::new (static_cast<void*>(m_data + m_size)) T(value);
            ++m_size;
        }
        return true;
    }

    [[nodiscard]] ENGINE_FORCE_INLINE bool TryPushBack(T&& value)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_move_constructible_v<T>)
    {
        if (m_size >= m_capacity && !GrowForAppend()) {
            return false;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            m_data[m_size++] = std::move(value);
        } else {
            ::new (static_cast<void*>(m_data + m_size)) T(std::move(value));
            ++m_size;
        }
        return true;
    }

    ENGINE_FORCE_INLINE void PushBack(const T& value)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
    {
        if (m_size >= m_capacity && !GrowForAppend()) {
            FatalAllocationFailure();
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            m_data[m_size++] = value;
        } else {
            ::new (static_cast<void*>(m_data + m_size)) T(value);
            ++m_size;
        }
    }

    ENGINE_FORCE_INLINE void PushBack(T&& value)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_move_constructible_v<T>)
    {
        if (m_size >= m_capacity && !GrowForAppend()) {
            FatalAllocationFailure();
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            m_data[m_size++] = std::move(value);
        } else {
            ::new (static_cast<void*>(m_data + m_size)) T(std::move(value));
            ++m_size;
        }
    }

    template<typename... Args>
    [[nodiscard]] pointer TryEmplaceBack(Args&&... args)
    {
        if (!EnsureCapacityForOne()) {
            return nullptr;
        }
        ::new (static_cast<void*>(m_data + m_size)) T(std::forward<Args>(args)...);
        return m_data + m_size++;
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

    [[nodiscard]] pointer TryAddUninitialized(size_type count) noexcept
    {
        static_assert(std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>,
            "AddUninitialized is only valid for trivial POD-style element types");

        const size_type oldSize = m_size;
        if (count == 0) {
            return m_data + oldSize;
        }
        if (count > (std::numeric_limits<size_type>::max)() - oldSize) {
            return nullptr;
        }

        const size_type newSize = oldSize + count;
        if (!TryReserve(newSize)) {
            return nullptr;
        }

        m_size = newSize;
        return m_data + oldSize;
    }

    [[nodiscard]] pointer AddUninitialized(size_type count) noexcept
    {
        pointer first = TryAddUninitialized(count);
        if (!first) {
            FatalAllocationFailure();
        }
        return first;
    }

    [[nodiscard]] bool TryAppendRange(const_pointer source, size_type count)
        noexcept(!ENGINE_ENABLE_EXCEPTIONS || std::is_nothrow_copy_constructible_v<T>)
    {
        ENGINE_ARRAY_ASSERT(source != nullptr || count == 0);
        if (count == 0) {
            return true;
        }
        if (!source || count > (std::numeric_limits<size_type>::max)() - m_size) {
            return false;
        }

        const size_type oldSize = m_size;
        const size_type newSize = oldSize + count;
        if (!TryReserve(newSize)) {
            return false;
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            MemCopy(m_data + oldSize, source, static_cast<usize>(count) * sizeof(T));
            m_size = newSize;
        } else {
            size_type constructed = 0;
#if ENGINE_ENABLE_EXCEPTIONS
            try {
#endif
                for (; constructed < count; ++constructed) {
                    ::new (static_cast<void*>(m_data + oldSize + constructed)) T(source[constructed]);
                }
                m_size = newSize;
#if ENGINE_ENABLE_EXCEPTIONS
            } catch (...) {
                DestroyRange(m_data + oldSize, constructed);
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

    [[nodiscard]] AppendWriter TryBeginAppend(size_type maxCount)
    {
        if (maxCount > (std::numeric_limits<size_type>::max)() - m_size) {
            return {};
        }

        const size_type start = m_size;
        const size_type requiredSize = start + maxCount;
        if (!TryReserve(requiredSize)) {
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

    void PopBack() noexcept
    {
        ENGINE_ARRAY_ASSERT(m_size > 0);
        if (m_size == 0) {
            return;
        }
        --m_size;
        m_data[m_size].~T();
    }

    void Resize(size_type count)
    {
        if (count < static_cast<size_type>(m_size)) {
            DestroyRange(m_data + count, m_size - count);
            m_size = static_cast<stored_size_type>(count);
            return;
        }

        Reserve(count);
        const stored_size_type targetSize = static_cast<stored_size_type>(count);
        while (m_size < targetSize) {
            ::new (static_cast<void*>(m_data + m_size)) T();
            ++m_size;
        }
    }

    void Resize(size_type count, const T& value)
    {
        if (count < static_cast<size_type>(m_size)) {
            DestroyRange(m_data + count, m_size - count);
            m_size = static_cast<stored_size_type>(count);
            return;
        }

        Reserve(count);
        const stored_size_type targetSize = static_cast<stored_size_type>(count);
        while (m_size < targetSize) {
            ::new (static_cast<void*>(m_data + m_size)) T(value);
            ++m_size;
        }
    }

    void Assign(size_type count, const T& value)
    {
        Clear();
        Reserve(count);
        const stored_size_type targetSize = static_cast<stored_size_type>(count);
        while (m_size < targetSize) {
            ::new (static_cast<void*>(m_data + m_size)) T(value);
            ++m_size;
        }
    }

    template<typename InputIt>
    void Assign(InputIt first, InputIt last)
    {
        Clear();
        for (InputIt it = first; it != last; ++it) {
            PushBack(*it);
        }
    }

    iterator Insert(const_iterator position, const T& value)
    {
        return InsertImpl(position, value);
    }

    iterator Insert(const_iterator position, T&& value)
    {
        return InsertImpl(position, std::move(value));
    }

    iterator Erase(const_iterator position)
    {
        return Erase(position, position + 1);
    }

    iterator Erase(const_iterator first, const_iterator last)
    {
        ENGINE_ARRAY_ASSERT(first >= begin() && first <= end());
        ENGINE_ARRAY_ASSERT(last >= first && last <= end());

        const size_type firstIndex = static_cast<size_type>(first - begin());
        const size_type lastIndex = static_cast<size_type>(last - begin());
        const size_type eraseCount = lastIndex - firstIndex;
        if (eraseCount == 0) {
            return begin() + firstIndex;
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            MemMove(m_data + firstIndex, m_data + lastIndex, (m_size - lastIndex) * sizeof(T));
        } else {
            for (size_type index = firstIndex; index < lastIndex; ++index) {
                m_data[index].~T();
            }
            for (size_type index = lastIndex; index < m_size; ++index) {
                ::new (static_cast<void*>(m_data + index - eraseCount)) T(std::move(m_data[index]));
                m_data[index].~T();
            }
        }

        m_size = static_cast<stored_size_type>(m_size - eraseCount);
        return begin() + firstIndex;
    }

    void Swap(Array& other)
        noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (this == &other) {
            return;
        }

        Array temp(std::move(other));
        other = std::move(*this);
        *this = std::move(temp);
    }

    [[nodiscard]] ENGINE_FORCE_INLINE reference front() noexcept { return Front(); }
    [[nodiscard]] ENGINE_FORCE_INLINE const_reference front() const noexcept { return Front(); }
    [[nodiscard]] ENGINE_FORCE_INLINE reference back() noexcept { return Back(); }
    [[nodiscard]] ENGINE_FORCE_INLINE const_reference back() const noexcept { return Back(); }
    [[nodiscard]] ENGINE_FORCE_INLINE pointer data() noexcept { return Data(); }
    [[nodiscard]] ENGINE_FORCE_INLINE const_pointer data() const noexcept { return Data(); }
    [[nodiscard]] ENGINE_FORCE_INLINE bool empty() const noexcept { return IsEmpty(); }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type size() const noexcept { return Size(); }
    [[nodiscard]] ENGINE_FORCE_INLINE size_type capacity() const noexcept { return Capacity(); }
    void reserve(size_type newCapacity) { Reserve(newCapacity); }
    void shrink_to_fit() { ShrinkToFit(); }
    void clear() noexcept { Clear(); }
    void push_back(const T& value) { PushBack(value); }
    void push_back(T&& value) { PushBack(std::move(value)); }
    template<typename... Args>
    reference emplace_back(Args&&... args) { return EmplaceBack(std::forward<Args>(args)...); }
    [[nodiscard]] pointer add_uninitialized(size_type count) noexcept { return AddUninitialized(count); }
    void append_range(const_pointer source, size_type count) { AppendRange(source, count); }
    [[nodiscard]] AppendWriter begin_append(size_type maxCount) { return BeginAppend(maxCount); }
    void pop_back() noexcept { PopBack(); }
    void resize(size_type count) { Resize(count); }
    void resize(size_type count, const T& value) { Resize(count, value); }
    void assign(size_type count, const T& value) { Assign(count, value); }
    template<typename InputIt>
    void assign(InputIt first, InputIt last) { Assign(first, last); }
    iterator insert(const_iterator position, const T& value) { return Insert(position, value); }
    iterator insert(const_iterator position, T&& value) { return Insert(position, std::move(value)); }
    iterator erase(const_iterator position) { return Erase(position); }
    iterator erase(const_iterator first, const_iterator last) { return Erase(first, last); }

private:
    [[nodiscard]] pointer InlineData() noexcept
    {
        if constexpr (InlineCapacity > 0) {
            return m_inline.Data();
        } else {
            return nullptr;
        }
    }

    [[nodiscard]] const_pointer InlineData() const noexcept
    {
        if constexpr (InlineCapacity > 0) {
            return m_inline.Data();
        } else {
            return nullptr;
        }
    }

    void ResetToInlineStorage() noexcept
    {
        m_data = InlineData();
        m_capacity = static_cast<stored_size_type>(InlineCapacity);
    }

    [[nodiscard]] ENGINE_FORCE_INLINE bool EnsureCapacityForOne()
    {
        return m_size < m_capacity || GrowForAppend();
    }

    [[nodiscard]] bool GrowForAppend()
    {
        const size_type grownCapacity = CalculateGrowth(m_size + 1);
        return Reallocate(grownCapacity);
    }

    [[nodiscard]] size_type CalculateGrowth(size_type requiredCapacity) const noexcept
    {
        size_type newCapacity = m_capacity > 0 ? static_cast<size_type>(m_capacity) * 2 : 8;
        if (newCapacity < requiredCapacity) {
            newCapacity = requiredCapacity;
        }
        return newCapacity;
    }

    [[nodiscard]] static constexpr bool CanRepresentSize(size_type count) noexcept
    {
        return count <= (std::numeric_limits<stored_size_type>::max)();
    }

    [[nodiscard]] bool CanAllocate(size_type count) const noexcept
    {
        return count <= (std::numeric_limits<size_type>::max)() / sizeof(T);
    }

    [[nodiscard]] pointer Allocate(size_type count)
    {
        if (count == 0) {
            return nullptr;
        }
        if (!CanRepresentSize(count) || !CanAllocate(count)) {
            return nullptr;
        }
        return static_cast<pointer>(Allocator().Allocate(count * sizeof(T), alignof(T)));
    }

    [[noreturn]] static void FatalAllocationFailure()
    {
        VOID_ASSERT(false);
        std::abort();
    }

    void FreeHeapStorage() noexcept
    {
        if (m_data && !UsingInlineStorage()) {
            Allocator().Free(m_data);
        }
    }

    static void DestroyRange(pointer data, size_type count) noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type index = 0; index < count; ++index) {
                data[index].~T();
            }
        } else {
            (void)data;
            (void)count;
        }
    }

    static void RelocateConstructRange(pointer destination, pointer source, size_type count)
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            MemCopy(destination, source, count * sizeof(T));
        } else {
            size_type constructed = 0;
#if ENGINE_ENABLE_EXCEPTIONS
            try {
#endif
                for (; constructed < count; ++constructed) {
                    ::new (static_cast<void*>(destination + constructed)) T(std::move(source[constructed]));
                }
#if ENGINE_ENABLE_EXCEPTIONS
            } catch (...) {
                DestroyRange(destination, constructed);
                throw;
            }
#endif
        }
    }

    [[nodiscard]] bool Reallocate(size_type newCapacity)
    {
        pointer newData = Allocate(newCapacity);
        if (!newData) {
            return false;
        }

        const size_type oldSize = m_size;
        pointer oldData = m_data;
        const bool oldWasInline = UsingInlineStorage();

        if (oldSize > 0) {
            RelocateConstructRange(newData, oldData, oldSize);
            DestroyRange(oldData, oldSize);
        }

        m_data = newData;
        m_capacity = static_cast<stored_size_type>(newCapacity);

        if (oldData && !oldWasInline) {
            Allocator().Free(oldData);
        }
        return true;
    }

    template<typename U>
    iterator InsertImpl(const_iterator position, U&& value)
    {
        ENGINE_ARRAY_ASSERT(position >= begin() && position <= end());
        const size_type index = static_cast<size_type>(position - begin());

        if (!EnsureCapacityForOne()) {
            FatalAllocationFailure();
        }

        if (index == m_size) {
            return &EmplaceBack(std::forward<U>(value));
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            MemMove(m_data + index + 1, m_data + index, (m_size - index) * sizeof(T));
            m_data[index] = std::forward<U>(value);
        } else {
            ::new (static_cast<void*>(m_data + m_size)) T(std::move(m_data[m_size - 1]));
            for (size_type moveIndex = m_size - 1; moveIndex > index; --moveIndex) {
                m_data[moveIndex] = std::move(m_data[moveIndex - 1]);
            }
            m_data[index] = std::forward<U>(value);
        }

        ++m_size;
        return m_data + index;
    }

    void MoveFrom(Array&& other)
    {
        if (other.IsEmpty()) {
            return;
        }

        if (!other.UsingInlineStorage()) {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.ResetToInlineStorage();
            other.m_size = 0;
            return;
        }

        Reserve(other.m_size);
        RelocateConstructRange(m_data, other.m_data, other.m_size);
        m_size = other.m_size;
        other.Clear();
    }

    pointer m_data = nullptr;
    stored_size_type m_size = 0;
    stored_size_type m_capacity = 0;
    Memory::IAllocator* m_allocator = nullptr;
    [[no_unique_address]] Detail::ArrayInlineStorage<T, InlineCapacity> m_inline{};
};

} // namespace Engine::Containers

namespace Engine {
using ::Engine::Containers::Array;
}

#undef ENGINE_ARRAY_ASSERT

#endif // FOUNDATION_CONTAINERS_ARRAY_HDR
