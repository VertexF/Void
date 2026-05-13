#include <Foundation/Memory/MemoryTag.hpp>

namespace Engine::Memory {

const char* GetMemoryTagName(MemoryTag tag) noexcept
{
    switch (tag) {
        case MemoryTag::Default:       return "Default";
        case MemoryTag::Core:          return "Core";
        case MemoryTag::System:        return "System";
        case MemoryTag::Physics:       return "Physics";
        case MemoryTag::Render:        return "Render";
        case MemoryTag::Audio:         return "Audio";
        case MemoryTag::Script:        return "Script";
        case MemoryTag::Animation:     return "Animation";
        case MemoryTag::AI:            return "AI";
        case MemoryTag::Network:       return "Network";
        case MemoryTag::UI:            return "UI";
        case MemoryTag::Texture:       return "Texture";
        case MemoryTag::Shader:        return "Shader";
        case MemoryTag::Mesh:          return "Mesh";
        case MemoryTag::AnimationData: return "AnimationData";
        case MemoryTag::Material:      return "Material";
        case MemoryTag::Resource:      return "Resource";
        case MemoryTag::Entity:        return "Entity";
        case MemoryTag::Component:     return "Component";
        case MemoryTag::String:        return "String";
        case MemoryTag::Collection:    return "Collection";
        case MemoryTag::Temporary:     return "Temporary";
        case MemoryTag::Debug:         return "Debug";
        case MemoryTag::Count:         return "Count";
    }

    return "Unknown";
}

} // namespace Engine::Memory
