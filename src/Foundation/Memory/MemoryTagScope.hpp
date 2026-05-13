#ifndef FOUNDATION_MEMORY_MEMORYTAGSCOPE_HDR
#define FOUNDATION_MEMORY_MEMORYTAGSCOPE_HDR

#include <Foundation/Memory/MemoryTag.hpp>

namespace Engine::Memory {

MemoryTag GetCurrentMemoryTag() noexcept;
void SetCurrentMemoryTag(MemoryTag tag) noexcept;

class MemoryTagScope {
public:
    explicit MemoryTagScope(MemoryTag tag) noexcept
        : m_previous(GetCurrentMemoryTag())
    {
        SetCurrentMemoryTag(tag);
    }

    ~MemoryTagScope() noexcept
    {
        SetCurrentMemoryTag(m_previous);
    }

    MemoryTagScope(const MemoryTagScope&) = delete;
    MemoryTagScope& operator=(const MemoryTagScope&) = delete;

private:
    MemoryTag m_previous;
};

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_MEMORYTAGSCOPE_HDR
