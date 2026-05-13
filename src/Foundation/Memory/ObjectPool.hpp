#ifndef FOUNDATION_MEMORY_OBJECTPOOL_HDR
#define FOUNDATION_MEMORY_OBJECTPOOL_HDR

#include <Utility/Assert.hpp>
#include <Foundation/Memory/Allocators/PoolAllocator.hpp>
#include <Utility/Move.hpp>

namespace Engine::Memory {

/// @brief Fixed-capacity object pool for `T`.
/// @note Does not track outstanding objects during Reset(); callers must ensure
///       all objects have been released (or accept UB) before resetting.
template<typename T>
class ObjectPool final {
public:
    explicit ObjectPool(size_t capacity, IAllocator* backingAllocator = nullptr)
        : m_pool(sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T), capacity, backingAllocator, alignof(T))
    {}

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    template<typename... Args>
    [[nodiscard]] T* Acquire(Args&&... args)
    {
        void* mem = m_pool.Allocate(sizeof(T), alignof(T));
        if (!mem) {
            return nullptr;
        }
        return new (mem) T(Forward<Args>(args)...);
    }

    void Release(T* obj)
    {
        if (!obj) {
            return;
        }
        ENGINE_ASSERT_MSG(m_pool.Owns(obj), "ObjectPool::Release Pointer not owned by pool");
        obj->~T();
        m_pool.Free(obj);
    }

    [[nodiscard]] bool Owns(const T* obj) const noexcept { return m_pool.Owns(const_cast<T*>(obj)); }
    [[nodiscard]] bool IsFull() const noexcept { return m_pool.IsFull(); }
    [[nodiscard]] size_t Capacity() const noexcept { return m_pool.GetBlockCount(); }
    [[nodiscard]] size_t FreeCount() const noexcept { return m_pool.GetFreeBlockCount(); }

    void Reset() { m_pool.Reset(); }

private:
    PoolAllocator m_pool;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_OBJECTPOOL_HDR