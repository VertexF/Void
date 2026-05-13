#ifndef ENGINE_SOURCE_LOCATION_HPP
#define ENGINE_SOURCE_LOCATION_HPP
// NOTE: Prefer `SourceLocation` for reliable call-site capture on modern compilers.

#include <Foundation/Platform.hpp>

namespace Engine {

/// @brief Source code location for assertions, logging, and diagnostics.
struct SourceLocation {
    const char* file = "";
    const char* function = "";
    int32 line = 0;

    /// @brief Capture the current call site.
    ///
    /// Usage: `SourceLocation loc = SourceLocation::Current();`
    [[nodiscard]] static constexpr SourceLocation Current(
        const char* fileName = __builtin_FILE(),
        const char* functionName = __builtin_FUNCTION(),
        int32 lineNumber = static_cast<int32>(__builtin_LINE())) noexcept
    {
        return {fileName, functionName, lineNumber};
    }

    /// @brief Get just the filename without path.
    [[nodiscard]] constexpr const char* Filename() const noexcept {
        const char* lastSlash = file;
        for (const char* p = file; *p; ++p) {
            if (*p == '/' || *p == '\\') {
                lastSlash = p + 1;
            }
        }
        return lastSlash;
    }

    /// @brief Check if the location has file information.
    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return file != nullptr && file[0] != '\0';
    }
};

} // namespace Engine

#endif // !ENGINE_SOURCE_LOCATION_HPP