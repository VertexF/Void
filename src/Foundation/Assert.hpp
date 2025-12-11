#ifndef ASSERT_HDR
#define ASSERT_HDR

#include "Log.hpp"

#define VOID_ASSERT(condition) if((condition) == false) { vprint(VOID_FILELINE("FALSE\n")); VOID_DEBUG_BREAK }
#if defined(_MSC_VER)
#define VOID_ASSERTM(condition, message, ...) if((condition) == false) { vprint(VOID_FILELINE(VOID_CONCAT(message, "\n")), __VA_ARGS__); VOID_DEBUG_BREAK; }
#define VOID_ERROR(message, ...)  vprint(VOID_FILELINE(VOID_CONCAT(message, "\n")), __VA_ARGS__); VOID_DEBUG_BREAK;
#else
#define VOID_ASSERTM(condition, message, ...) if((condition) == false) { vprint(VOID_FILELINE(VOID_CONCAT(message, "\n")), ## __VA_ARGS__); VOID_DEBUG_BREAK; }
#define VOID_ERROR(message, ...)  vprint(VOID_FILELINE(VOID_CONCAT(message, "\n")), ## __VA_ARGS__); VOID_DEBUG_BREAK;
#endif



#endif // !ASSERT_HDR
