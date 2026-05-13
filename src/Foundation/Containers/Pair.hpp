#ifndef FOUNDATION_CONTAINERS_PAIR_HDR
#define FOUNDATION_CONTAINERS_PAIR_HDR

#include <Foundation/Platform.hpp>
#include <Utility/Macros.hpp>
#include <Utility/Move.hpp>

namespace Engine {

/// @brief Zero-dependency pair type.
template<typename T1, typename T2>
struct Pair {
    T1 first;
    T2 second;
};

/// @brief Construct a pair.
template<typename T1, typename T2>
[[nodiscard]] ENGINE_FORCE_INLINE constexpr Pair<T1, T2>
MakePair(T1 a, T2 b) noexcept
{
    return { Move(a), Move(b) };
}

} // namespace Engine


#endif // !FOUNDATION_CONTAINERS_PAIR_HDR