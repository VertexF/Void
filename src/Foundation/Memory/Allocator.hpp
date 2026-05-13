#ifndef FOUNDATION_MEMORY_ALLOCATOR_HDR
#define FOUNDATION_MEMORY_ALLOCATOR_HDR

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <Foundation/Memory/Alignment.hpp>
#include <Foundation/Memory/MemoryTag.hpp>
#include <Foundation/Platform.hpp>
#include <new>

#ifdef GetFreeSpace
#undef GetFreeSpace
#endif

namespace Engine::Memory {

    /// @brief Base allocator interface
    class IAllocator {
    public:
        virtual ~IAllocator() = default;

        /// @brief Allocate memory block
        /// @param size Size in bytes to allocate
        /// @param alignment Alignment requirement (must be power of 2)
        /// @return Pointer to allocated memory, or nullptr on failure
        virtual void* Allocate(size_t size, size_t alignment = alignof(MaxAlignT)) = 0;

        /// @brief Free a previously allocated block
        /// @param ptr Pointer returned by Allocate
        virtual void Free(void* ptr) = 0;

        /// @brief Reallocate a memory block
        /// @param ptr Pointer to previously allocated memory (can be nullptr)
        /// @param newSize New size in bytes
        /// @param alignment Alignment requirement (must be power of 2)
        /// @return Pointer to new memory block, or nullptr on failure.
        ///         If ptr is null, acts like Allocate.
        ///         If newSize is 0, acts like Free (returns nullptr).
        virtual void* Reallocate(void* ptr, size_t newSize, size_t alignment = alignof(MaxAlignT)) {
            // Default implementation returns nullptr. Subclasses should override.
            if (newSize == 0) {
                Free(ptr);
                return nullptr;
            }
            if (ptr == nullptr) {
                return Allocate(newSize, alignment);
            }
            return nullptr;
        }

        /// @brief Get total allocated bytes
        virtual size_t AllocatedSize() const = 0;

        /// @brief Get allocator name for debugging
        virtual const char* Name() const = 0;

        /// @brief Check if allocator owns this Pointer
        virtual bool Owns(void* ptr) const = 0;
    };

    /// @brief Allocation header stored before each allocation (for tracking)
    struct AllocationHeader {
        size_t size;
        size_t adjustment;
        MemoryTag tag;
        size_t padding;
    };

    struct ArrayAllocationHeader {
        size_t count;
        void* allocation;
    };

    /// @brief Get the system default allocator (uses malloc/free)
    IAllocator& GetDefaultAllocator();

    /// @brief Set the global default allocator
    void SetDefaultAllocator(IAllocator* allocator);

    // ============================================================================
    // Typed allocation helpers
    // ============================================================================

    /// @brief Allocate and construct a single object
    template<typename T, typename... Args>
    T* New(IAllocator& allocator, Args&&... args) {
        void* memory = allocator.Allocate(sizeof(T), alignof(T));
        if (!memory) return nullptr;
        return new (memory) T(std::forward<Args>(args)...);
    }

    /// @brief Destruct and free a single object
    template<typename T>
    void Delete(IAllocator& allocator, T* ptr) {
        if (ptr) {
            ptr->~T();
            allocator.Free(ptr);
        }
    }

    /// @brief Allocate array of objects (default constructed)
    template<typename T>
    T* NewArray(IAllocator& allocator, size_t count) {
        constexpr size_t headerSize = sizeof(ArrayAllocationHeader);
        constexpr size_t arrayAlignment = alignof(T) > alignof(ArrayAllocationHeader)
            ? alignof(T)
            : alignof(ArrayAllocationHeader);
        constexpr size_t maxAlignmentPadding = arrayAlignment - 1;

        if (count > ((std::numeric_limits<size_t>::max)() - headerSize - maxAlignmentPadding) / sizeof(T)) {
            return nullptr;
        }

        const size_t totalSize = headerSize + maxAlignmentPadding + sizeof(T) * count;

        void* memory = allocator.Allocate(totalSize, arrayAlignment);
        if (!memory) return nullptr;

        T* array = static_cast<T*>(AlignPointer(static_cast<uint8_t*>(memory) + headerSize, arrayAlignment));
        auto* header = reinterpret_cast<ArrayAllocationHeader*>(reinterpret_cast<uint8_t*>(array) - headerSize);
        header->count = count;
        header->allocation = memory;

        for (size_t i = 0; i < count; ++i) {
            new (array + i) T();
        }
        return array;
    }

    /// @brief Destruct and free array
    template<typename T>
    void DeleteArray(IAllocator& allocator, T* array) {
        if (array) {
            auto* header = reinterpret_cast<ArrayAllocationHeader*>(reinterpret_cast<uint8_t*>(array) - sizeof(ArrayAllocationHeader));
            size_t count = header->count;

            for (size_t i = 0; i < count; ++i) {
                array[i].~T();
            }
            allocator.Free(header->allocation);
        }
    }
} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_ALLOCATOR_HDR
