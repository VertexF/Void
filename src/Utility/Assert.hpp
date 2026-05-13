#ifndef ENGINE_ASSERT_HPP
#define ENGINE_ASSERT_HPP

#include <Foundation/Platform.hpp>
#include <Utility/Macros.hpp>
#include <Utility/SourceLocation.hpp>

namespace Engine {

// ============================================================================
// Assertion Handler Types
// ============================================================================

/// @brief Information about an assertion failure
struct AssertInfo {
    const char* expression;
    const char* message;
    const char* file;
    int32 line;
    const char* function;
};

/// @brief Assertion handler function type
/// @return true to break into debugger, false to continue
using AssertHandler = bool(*)(const AssertInfo& info);

// ============================================================================
// Assertion Handler Management
// ============================================================================

/// @brief Set a custom assertion handler
/// @param handler The handler function, or nullptr to reset to default
/// @return The previous handler
ENGINE_API AssertHandler SetAssertHandler(AssertHandler handler);

/// @brief Get the current assertion handler
ENGINE_API AssertHandler GetAssertHandler();

// ============================================================================
// Internal Assertion Functions (do not call directly)
// ============================================================================

namespace Detail {

/// @brief Report an assertion failure
/// @return true if should break into debugger
ENGINE_API bool ReportAssertFailure(
    const char* expression,
    const char* message,
    const char* file,
    int32 line,
    const char* function
);

/// @brief Report an assertion failure with a captured source location.
[[nodiscard]] inline bool ReportAssertFailure(
    const char* expression,
    const char* message,
    const SourceLocation& location)
{
    return ReportAssertFailure(expression, message, location.file, location.line, location.function);
}

/// @brief Report a verification failure (always executes, even in release)
ENGINE_API bool ReportVerifyFailure(
    const char* expression,
    const char* message,
    const char* file,
    int32 line,
    const char* function
);

/// @brief Report a verification failure with a captured source location.
[[nodiscard]] inline bool ReportVerifyFailure(
    const char* expression,
    const char* message,
    const SourceLocation& location)
{
    return ReportVerifyFailure(expression, message, location.file, location.line, location.function);
}

} // namespace Detail

} // namespace Engine

// ============================================================================
// Assertion Macros
// ============================================================================
// NOTE: ENGINE_ASSERT_FMT is defined at the bottom of this file.
// Call sites using ENGINE_ASSERT_FMT must explicitly include <Core/Containers/Format.h>.
// We DO NOT include it here to avoid a circular dependency:
// Assert.h → Format.h → StringBuilder.h → String.h → Assert.h
// ============================================================================

#if defined(ENGINE_BUILD_DEBUG)

/// @brief Assert that a condition is true (debug builds only)
/// @param condition The condition to check
#define ENGINE_ASSERT(condition) \
    do { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            if (::Engine::Detail::ReportAssertFailure( \
                #condition, nullptr, engineLoc)) { \
                ENGINE_DEBUG_BREAK(); \
            } \
        } \
    } while (0)

/// @brief Assert that a condition is true with a message (debug builds only)
/// @param condition The condition to check
/// @param message The message to display on failure
#define ENGINE_ASSERT_MSG(condition, message) \
    do { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            if (::Engine::Detail::ReportAssertFailure( \
                #condition, message, engineLoc)) { \
                ENGINE_DEBUG_BREAK(); \
            } \
        } \
    } while (0)

#else // Release builds

#define ENGINE_ASSERT(condition) \
    do { ENGINE_UNUSED(condition); } while (0)

#define ENGINE_ASSERT_MSG(condition, message) \
    do { ENGINE_UNUSED(condition); ENGINE_UNUSED(message); } while (0)

#endif // ENGINE_BUILD_DEBUG

// ============================================================================
// Verify Macros (always execute, even in release)
// ============================================================================

/// @brief Verify that a condition is true (all builds, but only reports in debug)
/// @param condition The condition to check
/// @note The condition is ALWAYS evaluated, unlike ENGINE_ASSERT
#define ENGINE_VERIFY(condition) \
    do { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            if (::Engine::Detail::ReportVerifyFailure( \
                #condition, nullptr, engineLoc)) { \
                ENGINE_DEBUG_BREAK(); \
            } \
        } \
    } while (0)

/// @brief Verify that a condition is true with a message
/// @param condition The condition to check
/// @param message The message to display on failure
#define ENGINE_VERIFY_MSG(condition, message) \
    do { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            if (::Engine::Detail::ReportVerifyFailure( \
                #condition, message, engineLoc)) { \
                ENGINE_DEBUG_BREAK(); \
            } \
        } \
    } while (0)

// ============================================================================
// Check Macros (softer assertions - log but don't break)
// ============================================================================

#if defined(ENGINE_BUILD_DEBUG)

/// @brief Check a condition and log if false (debug builds only)
/// @param condition The condition to check
/// @return true if condition is true, false otherwise
#define ENGINE_CHECK(condition) \
    ([&]() -> bool { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            ::Engine::Detail::ReportAssertFailure( \
                #condition, nullptr, engineLoc); \
            return false; \
        } \
        return true; \
    }())

/// @brief Check a condition and log if false, with message
/// @param condition The condition to check
/// @param message The message to log on failure
/// @return true if condition is true, false otherwise
#define ENGINE_CHECK_MSG(condition, message) \
    ([&]() -> bool { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            ::Engine::Detail::ReportAssertFailure( \
                #condition, message, engineLoc); \
            return false; \
        } \
        return true; \
    }())

#else // Release builds

#define ENGINE_CHECK(condition) (!!(condition))
#define ENGINE_CHECK_MSG(condition, message) (!!(condition))

#endif // ENGINE_BUILD_DEBUG

// ============================================================================
// Fatal Error
// ============================================================================

/// @brief Report a fatal error and terminate
/// @param message The error message
#define ENGINE_FATAL(message) \
    do { \
        const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
        ENGINE_UNUSED(::Engine::Detail::ReportAssertFailure( \
            "FATAL ERROR", message, engineLoc)); \
        ENGINE_DEBUG_BREAK(); \
        abort(); \
    } while (0)

// ============================================================================
// Not Implemented
// ============================================================================

/// @brief Mark code as not implemented
#define ENGINE_NOT_IMPLEMENTED() \
    ENGINE_FATAL("Not implemented")

// ============================================================================
// Unreachable Code
// ============================================================================

/// @brief Mark code that should never be reached
#if defined(ENGINE_BUILD_DEBUG)
#define ENGINE_UNREACHABLE_CODE() \
    do { \
        ENGINE_FATAL("Unreachable code executed"); \
        ENGINE_UNREACHABLE(); \
    } while (0)
#else
#define ENGINE_UNREACHABLE_CODE() ENGINE_UNREACHABLE()
#endif

// ============================================================================
// Null Pointer Checks
// ============================================================================
/// @brief Assert that a Pointer is not null
#define ENGINE_ASSERT_NOT_NULL(ptr) \
    ENGINE_ASSERT_MSG((ptr) != nullptr, "Pointer is null: " #ptr)

/// @brief Verify that a Pointer is not null (all builds)
#define ENGINE_VERIFY_NOT_NULL(ptr) \
    ENGINE_VERIFY_MSG((ptr) != nullptr, "Pointer is null: " #ptr)

// ============================================================================
// Range Checks
// ============================================================================
/// @brief Assert that a value is within a range [min, max]
#define ENGINE_ASSERT_RANGE(value, min, max) \
    ENGINE_ASSERT_MSG(((value) >= (min)) && ((value) <= (max)), \
        "Value out of range: " #value)

/// @brief Assert that an index is valid for a container
#define ENGINE_ASSERT_INDEX(index, size) \
    ENGINE_ASSERT_MSG((index) < (size), "Index out of bounds: " #index)

// ============================================================================
// Formatted Assertion
// ============================================================================
// Callers relying on this macro must explicitly include <Core/Containers/Format.h>.
// ============================================================================

#if defined(ENGINE_BUILD_DEBUG)

/// @brief Assert that a condition is true with a formatted message (debug builds only)
/// @param condition The condition to check
/// @param fmt The format string used by Engine::Format
#define ENGINE_ASSERT_FMT(condition, fmt, ...) \
    do { \
        if (ENGINE_UNLIKELY(!(condition))) { \
            const ::Engine::String engineAssertMsg = ::Engine::Format((fmt), ##__VA_ARGS__); \
            const ::Engine::SourceLocation engineLoc = ::Engine::SourceLocation::Current(); \
            if (::Engine::Detail::ReportAssertFailure( \
                #condition, engineAssertMsg.CStr(), engineLoc)) { \
                ENGINE_DEBUG_BREAK(); \
            } \
        } \
    } while (0)

#else // Release builds

#define ENGINE_ASSERT_FMT(condition, fmt, ...) \
    do { ENGINE_UNUSED(condition); ENGINE_UNUSED(fmt); } while (0)

#endif // ENGINE_BUILD_DEBUG

#endif // ENGINE_ASSERT_HPP