#include <Foundation/Memory/MemoryTagScope.hpp>

namespace Engine::Memory {

namespace {
thread_local MemoryTag tl_currentTag = MemoryTag::Default;
}

MemoryTag GetCurrentMemoryTag() noexcept
{
    return tl_currentTag;
}

void SetCurrentMemoryTag(MemoryTag tag) noexcept
{
    tl_currentTag = tag;
}

} // namespace Engine::Memory
