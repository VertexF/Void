#ifndef FOUNDATION_CONTAINERS_VECTOR_HDR
#define FOUNDATION_CONTAINERS_VECTOR_HDR

#include <Foundation/Containers/Array.hpp>

namespace Engine::Containers {

template<typename T>
using Vector = Array<T>;

} // namespace Engine::Containers

// ============================================================================
// Namespace Re-exports
// ============================================================================

namespace Engine {
using ::Engine::Containers::Vector;
} // namespace Engine

namespace Engine::Containers {
using ::Engine::Containers::Vector;
} // namespace Engine::Containers

#endif // !FOUNDATION_CONTAINERS_VECTOR_HDR
