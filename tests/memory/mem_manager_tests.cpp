#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Debug/DebugAllocator.hpp>
#include <Foundation/Memory/Debug/MemoryProfiler.hpp>
#include <Foundation/Memory/MemoryManager.hpp>
#include <Foundation/Memory/MemoryTagScope.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <cstdint>

namespace Engine::Memory {
namespace {

struct BudgetCallbackState {
    static inline uint32 Calls = 0;
    static inline MemoryTag Tag = MemoryTag::Default;
    static inline size_t RequestedSize = 0;
    static inline size_t CurrentUsage = 0;
    static inline size_t Budget = 0;
};

void OnBudgetPressure(MemoryTag tag, size_t requestedSize, size_t currentUsage, size_t budget)
{
    ++BudgetCallbackState::Calls;
    BudgetCallbackState::Tag = tag;
    BudgetCallbackState::RequestedSize = requestedSize;
    BudgetCallbackState::CurrentUsage = currentUsage;
    BudgetCallbackState::Budget = budget;
}

StringView MakeView(const char* text)
{
    return {const_cast<char*>(text), std::strlen(text)};
}

struct CountedObject {
    explicit CountedObject(int value, int* constructed, int* destroyed)
        : Value(value), Constructed(constructed), Destroyed(destroyed)
    {
        ++(*Constructed);
    }

    ~CountedObject()
    {
        ++(*Destroyed);
    }

    int Value = 0;
    int* Constructed = nullptr;
    int* Destroyed = nullptr;
};

struct alignas(64) OverAlignedObject {
    OverAlignedObject()
    {
        ++Constructed;
    }

    ~OverAlignedObject()
    {
        ++Destroyed;
    }

    int Value = 0;

    static inline int Constructed = 0;
    static inline int Destroyed = 0;
};

} // namespace

TEST(MemoryAllocator, DefaultAllocatorAllocatesAndFrees)
{
    IAllocator& allocator = GetDefaultAllocator();

    void* ptr = allocator.Allocate(128, 16);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 16u, 0u);

    allocator.Free(ptr);
}

TEST(MemoryAllocator, SetDefaultAllocatorNullRestoresSystemAllocator)
{
    SetDefaultAllocator(nullptr);
    IAllocator& allocator = GetDefaultAllocator();

    void* ptr = allocator.Allocate(64, 16);
    ASSERT_NE(ptr, nullptr);
    allocator.Free(ptr);
}

TEST(MemoryAllocator, TypedNewAndDeleteConstructsObject)
{
    int constructed = 0;
    int destroyed = 0;

    CountedObject* object = New<CountedObject>(GetDefaultAllocator(), 42, &constructed, &destroyed);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->Value, 42);
    EXPECT_EQ(constructed, 1);
    EXPECT_EQ(destroyed, 0);

    Delete(GetDefaultAllocator(), object);
    EXPECT_EQ(destroyed, 1);
}

TEST(MemoryAllocator, TypedArrayStoresCountAndDestroysElements)
{
    int* values = NewArray<int>(GetDefaultAllocator(), 4);
    ASSERT_NE(values, nullptr);

    values[0] = 7;
    values[1] = 11;
    values[2] = 13;
    values[3] = 17;

    EXPECT_EQ(values[0] + values[1] + values[2] + values[3], 48);
    DeleteArray(GetDefaultAllocator(), values);
}

TEST(MemoryAllocator, TypedArraySupportsOverAlignedElements)
{
    OverAlignedObject::Constructed = 0;
    OverAlignedObject::Destroyed = 0;

    OverAlignedObject* values = NewArray<OverAlignedObject>(GetDefaultAllocator(), 3);
    ASSERT_NE(values, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(values) % alignof(OverAlignedObject), 0u);
    EXPECT_EQ(OverAlignedObject::Constructed, 3);

    values[0].Value = 7;
    values[1].Value = 11;
    values[2].Value = 13;
    EXPECT_EQ(values[0].Value + values[1].Value + values[2].Value, 31);

    DeleteArray(GetDefaultAllocator(), values);
    EXPECT_EQ(OverAlignedObject::Destroyed, 3);
}

TEST(MemoryManager, DebugAllocatorReportsBudgetPressureBeforeAllocation)
{
    MemoryManager::Shutdown();
    BudgetCallbackState::Calls = 0;
    BudgetCallbackState::Tag = MemoryTag::Default;
    BudgetCallbackState::RequestedSize = 0;
    BudgetCallbackState::CurrentUsage = 0;
    BudgetCallbackState::Budget = 0;

    MemoryProfiler profiler;
    MemoryManager::Initialize();
    MemoryManager::Profiler(&profiler);
    MemoryManager::Budget(MemoryTag::Core, 64);
    MemoryManager::RegisterOOMCallback(&OnBudgetPressure);

    MallocAllocator backing;
    DebugAllocator allocator(&backing, nullptr, &profiler);

    void* first = nullptr;
    void* second = nullptr;
    {
        MemoryTagScope scope(MemoryTag::Core);
        first = allocator.Allocate(48, 16);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(BudgetCallbackState::Calls, 0u);

        second = allocator.Allocate(32, 16);
        ASSERT_NE(second, nullptr);
    }

    EXPECT_EQ(BudgetCallbackState::Calls, 1u);
    EXPECT_EQ(BudgetCallbackState::Tag, MemoryTag::Core);
    EXPECT_EQ(BudgetCallbackState::RequestedSize, 32u);
    EXPECT_EQ(BudgetCallbackState::CurrentUsage, 48u);
    EXPECT_EQ(BudgetCallbackState::Budget, 64u);

    allocator.Free(second);
    allocator.Free(first);
    MemoryManager::Profiler(nullptr);
    MemoryManager::RegisterOOMCallback(nullptr);
    MemoryManager::Budget(MemoryTag::Core, 0);
    MemoryManager::Shutdown();
}

TEST(MemoryProfiler, StoresAllocatorStatsSnapshots)
{
    MemoryProfiler profiler;
    MallocAllocator allocator;

    void* ptr = allocator.Allocate(96, 16);
    ASSERT_NE(ptr, nullptr);
    allocator.Free(ptr);

    profiler.UpdateAllocatorStats(allocator.Name(), allocator.GetStats());

    AllocatorStats stats{};
    ASSERT_TRUE(profiler.GetAllocatorStats("MallocAllocator", stats));
    EXPECT_STREQ(stats.name, "MallocAllocator");
    EXPECT_EQ(stats.liveBytes, 0u);
    EXPECT_GE(stats.peakBytes, 96u);
    EXPECT_EQ(stats.allocationCount, 1u);
    EXPECT_EQ(stats.freeCount, 1u);
}

TEST(MemoryManager, CapturesRegisteredAllocatorStatsIntoProfiler)
{
    MemoryManager::Shutdown();
    MemoryProfiler profiler;
    MallocAllocator allocator;

    MemoryManager::Initialize();
    MemoryManager::Profiler(&profiler);
    MemoryManager::RegisterAllocator(MakeView("main-malloc"), &allocator);

    void* ptr = allocator.Allocate(128, 16);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(MemoryManager::CaptureAllocatorStats(MakeView("main-malloc")));

    AllocatorStats stats{};
    ASSERT_TRUE(MemoryManager::GetAllocatorStats(MakeView("main-malloc"), stats));
    EXPECT_STREQ(stats.name, "main-malloc");
    EXPECT_EQ(stats.liveBytes, 128u);
    EXPECT_GE(stats.peakBytes, 128u);

    allocator.Free(ptr);
    MemoryManager::CaptureAllAllocatorStats();
    ASSERT_TRUE(MemoryManager::GetAllocatorStats(MakeView("main-malloc"), stats));
    EXPECT_EQ(stats.liveBytes, 0u);
    EXPECT_EQ(stats.freeCount, 1u);

    MemoryManager::UnregisterAllocator(MakeView("main-malloc"));
    MemoryManager::Profiler(nullptr);
    MemoryManager::Shutdown();
}

TEST(MemoryManager, DumpsAllocatorDiagnosticsForEditorHud)
{
    MemoryManager::Shutdown();
    MemoryProfiler profiler;
    MallocAllocator allocator;

    MemoryManager::Initialize();
    MemoryManager::Profiler(&profiler);
    MemoryManager::RegisterAllocator(MakeView("hud-malloc"), &allocator);

    void* ptr = allocator.Allocate(160, 16);
    ASSERT_NE(ptr, nullptr);

    const Vector<AllocatorStats> snapshots = MemoryManager::SnapshotAllocatorStats();
    ASSERT_FALSE(snapshots.empty());

    bool foundLiveAllocator = false;
    for (const AllocatorStats& stats : snapshots) {
        if (stats.name && std::strcmp(stats.name, allocator.Name()) == 0) {
            foundLiveAllocator = stats.liveBytes == 160u &&
                stats.peakBytes >= 160u &&
                stats.allocationCount >= 1u;
        }
    }
    EXPECT_TRUE(foundLiveAllocator);

    const std::string json = MemoryManager::DumpAllocatorStatsJson();
    EXPECT_NE(json.find("\"name\":\"hud-malloc\""), std::string::npos);
    EXPECT_NE(json.find("\"liveBytes\":160"), std::string::npos);
    EXPECT_NE(json.find("\"peakBytes\":"), std::string::npos);
    EXPECT_NE(json.find("\"failedAllocationCount\":"), std::string::npos);
    EXPECT_NE(json.find("\"fragmentationBytes\":"), std::string::npos);

    const std::string text = MemoryManager::DumpAllocatorStatsText();
    EXPECT_NE(text.find("hud-malloc"), std::string::npos);
    EXPECT_NE(text.find("live=160"), std::string::npos);
    EXPECT_NE(text.find("failures="), std::string::npos);
    EXPECT_NE(text.find("fragmentation="), std::string::npos);

    AllocatorStats profilerStats{};
    ASSERT_TRUE(MemoryManager::GetAllocatorStats(MakeView("hud-malloc"), profilerStats));
    EXPECT_EQ(profilerStats.liveBytes, 160u);

    allocator.Free(ptr);
    MemoryManager::UnregisterAllocator(MakeView("hud-malloc"));
    MemoryManager::Profiler(nullptr);
    MemoryManager::Shutdown();
}

} // namespace Engine::Memory
