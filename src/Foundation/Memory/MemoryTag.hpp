#ifndef FOUNDATION_MEMORY_MEMORYTAG_HDR
#define FOUNDATION_MEMORY_MEMORYTAG_HDR

#include <cstddef>

namespace Engine::Memory {
/// @brief Categories for memory allocations
enum class MemoryTag : size_t {
    Default = 0,
    Core,
    System,
    Physics,
    Render,
    Audio,
    Script,
    Animation,
    AI,
    Network,
    UI,
    Texture,
    Shader,
    Mesh,
    AnimationData,
    Material,
    Resource,
    Entity,
    Component,
    String,
    Collection,
    Temporary,
    Debug,
    
    Count
};

/// @brief Get human-readable name for a memory tag
const char* GetMemoryTagName(MemoryTag tag) noexcept;

} // namespace Engine::Memory

#endif // !FOUNDATION_MEMORY_MEMORYTAG_HDR
