#ifndef FOUNDATION_MEMORY_ALLOCATOR_DIAGNOSTICS_HDR
#define FOUNDATION_MEMORY_ALLOCATOR_DIAGNOSTICS_HDR

#include <Foundation/Memory/Allocator.hpp>
#include <Utility/Assert.hpp>

namespace Engine::Memory {

enum class AllocatorFailureKind : uint8 {
    InvalidRequest,
    OutOfMemory,
    InvalidPointer,
    DoubleFree,
    Corruption,
    UnsupportedOperation,
    InternalFailure
};

#if defined(ENGINE_MEMORY_DIAGNOSTICS_STRICT)
    inline constexpr bool kAllocatorStrictDiagnosticsEnabled = true;
#else
    inline constexpr bool kAllocatorStrictDiagnosticsEnabled = false;
#endif

[[nodiscard]] inline constexpr bool IsStrictAllocatorFailure(AllocatorFailureKind kind) noexcept
{
    return kind == AllocatorFailureKind::InvalidPointer ||
        kind == AllocatorFailureKind::DoubleFree ||
        kind == AllocatorFailureKind::Corruption;
}

inline void ReportAllocatorFailure(
    AllocatorStatsTracker& stats,
    AllocatorFailureKind kind,
    const char* message = "Allocator diagnostic failure") noexcept
{
    stats.RecordFailedAllocation();

    if constexpr (kAllocatorStrictDiagnosticsEnabled) {
        if (IsStrictAllocatorFailure(kind)) {
            ENGINE_ASSERT_MSG(false, message);
        }
    } else {
        (void)kind;
        (void)message;
    }
}

} // namespace Engine::Memory

#endif // FOUNDATION_MEMORY_ALLOCATOR_DIAGNOSTICS_HDR
