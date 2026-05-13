#ifndef PLATFORM_HDR
#define PLATFORM_HDR

#include <cstddef>
#include <cstdint>

#include <Foundation/CompilerTraits.hpp>

#if !defined(_MSC_VER)
    #include <signal.h>
#endif

#define ArraySize(arr) (sizeof(arr) / sizeof((arr)[0]))

#if defined(_MSC_VER)
    #define VOID_DEBUG_BREAK                    __debugbreak();
    #define VOID_DISABLE_WARNING(warningNumber) __pragma(warning(disable : warningNumber))
    #define VOID_CONCAT_OPERATOR(x, y)          x##y
#else
    #define VOID_DEBUG_BREAK                    raise(SIGTRAP);
    #define VOID_CONCAT_OPERATOR(x, y)          x y
#endif //_MSC_VER

#define VOID_STRINGISE(L)         #L
#define VOID_MARKSTRING(L)        VOID_STRINGISE(L)
#define VOID_CONCAT(x, y)         VOID_CONCAT_OPERATOR(x, y)
#define VOID_LINE_STRING          VOID_MARKSTRING(__LINE__)
#define VOID_FILELINE(MESSAGE)    __FILE__ "(" VOID_LINE_STRING ") : " MESSAGE

#define VOID_UNIQUE_SUFFIX(PARAM) VOID_CONCAT(PARAM, __LINE__)

namespace Engine {

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;
using uintptr = std::uintptr_t;
using byte = std::uint8_t;
using float32 = float;
using float64 = double;

} // namespace Engine

#endif // !PLATFORM_HDR
