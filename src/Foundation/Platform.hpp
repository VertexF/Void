#ifndef PLATFORM_HDR
#define PLATFORM_HDR

#include <stdint.h>

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

#endif // !PLATFORM_HDR
