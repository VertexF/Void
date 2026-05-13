#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Foundation/Array.hpp>
#include <Foundation/Containers/Array.hpp>
#include <Foundation/Containers/ChunkedArray.hpp>
#include <Foundation/Memory/Allocators/Advanced/BuddyAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/SecuredAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadLocalLinearAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeLinearAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TLSCachingAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TrackedAllocator.hpp>
#include <Foundation/Memory/Allocators/AlignedAllocator.hpp>
#include <Foundation/Memory/Allocators/BinnedAllocator.hpp>
#include <Foundation/Memory/Allocators/FrameAllocator.hpp>
#include <Foundation/Memory/Allocators/LinearAllocator.hpp>
#include <Foundation/Memory/Allocators/MallocAllocator.hpp>
#include <Foundation/Memory/Allocators/MonotonicAllocator.hpp>
#include <Foundation/Memory/Allocators/PoolAllocator.hpp>
#include <Foundation/Memory/Allocators/StackAllocator.hpp>
#include <Foundation/Memory/Debug/DebugAllocator.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace {

using Engine::uint32;

static volatile uint64_t g_sink = 0;

constexpr size_t kLargeCount = 1'000'000;
constexpr int kRefillRounds = 10;
constexpr size_t kSmallArrays = 200'000;
constexpr size_t kArenaSize = 128ull * 1024ull * 1024ull;
constexpr size_t kPoolBlockSize = 8ull * 1024ull * 1024ull;
constexpr size_t kPoolBlockCount = 8;

class LegacyAllocatorAdapter final : public Allocator {
public:
    explicit LegacyAllocatorAdapter(Engine::Memory::IAllocator& allocator)
        : m_allocator(&allocator)
    {
    }

    void* allocate(size_t size, size_t alignment) override
    {
        return m_allocator->Allocate(size, alignment);
    }

    void* allocate(size_t size, size_t alignment, const char*, int32_t) override
    {
        return allocate(size, alignment);
    }

    void deallocate(void* pointer) override
    {
        m_allocator->Free(pointer);
    }

private:
    Engine::Memory::IAllocator* m_allocator = nullptr;
};

enum class AllocatorKind {
    System,
    Malloc,
    Aligned64,
    Binned,
    TLSF,
    Buddy,
    Linear,
    Frame,
    Stack,
    Pool,
    Monotonic,
    ThreadSafe,
    ThreadSafeLinear,
    ThreadLocalLinear,
    TLSCaching,
    Tracked,
    Debug,
    Secured
};

enum Scenario : uint32_t {
    ScenarioContiguous = 1u << 0u,
    ScenarioChunked = 1u << 1u,
    ScenarioGrowth = 1u << 2u,
    ScenarioRefill = 1u << 3u,
    ScenarioSmall = 1u << 4u,
    ScenarioAll = ScenarioContiguous | ScenarioChunked | ScenarioGrowth | ScenarioRefill | ScenarioSmall
};

struct AllocatorInfo {
    AllocatorKind kind;
    const char* arg;
    const char* name;
    uint32_t scenarios;
};

constexpr AllocatorInfo kAllocatorInfos[] = {
    {AllocatorKind::System, "system", "System", ScenarioAll},
    {AllocatorKind::Malloc, "malloc", "Malloc", ScenarioAll},
    {AllocatorKind::Aligned64, "aligned64", "Aligned64", ScenarioAll},
    {AllocatorKind::Binned, "binned", "Binned", ScenarioAll},
    {AllocatorKind::TLSF, "tlsf", "TLSF-128MB", ScenarioAll},
    {AllocatorKind::Buddy, "buddy", "Buddy-128MB", ScenarioAll},
    {AllocatorKind::Linear, "linear", "Linear-128MB", ScenarioAll},
    {AllocatorKind::Frame, "frame", "Frame-128MB", ScenarioAll},
    {AllocatorKind::Stack, "stack", "Stack-128MB", 0u},
    {AllocatorKind::Pool, "pool", "Pool-8MBx64", ScenarioContiguous | ScenarioGrowth | ScenarioRefill | ScenarioSmall},
    {AllocatorKind::Monotonic, "monotonic", "Monotonic", ScenarioAll},
    {AllocatorKind::ThreadSafe, "threadsafe", "ThreadSafe", ScenarioAll},
    {AllocatorKind::ThreadSafeLinear, "threadsafe-linear", "ThreadSafeLinear-128MB", ScenarioAll},
    {AllocatorKind::ThreadLocalLinear, "threadlocal-linear", "ThreadLocalLinear-128MB", ScenarioAll},
    {AllocatorKind::TLSCaching, "tls-cache", "TLSCaching", ScenarioAll},
    {AllocatorKind::Tracked, "tracked", "Tracked", ScenarioAll},
    {AllocatorKind::Debug, "debug", "Debug", ScenarioAll},
    {AllocatorKind::Secured, "secured", "Secured", ScenarioContiguous | ScenarioGrowth | ScenarioRefill}
};

[[nodiscard]] const AllocatorInfo& Info(AllocatorKind kind) noexcept
{
    for (const AllocatorInfo& info : kAllocatorInfos) {
        if (info.kind == kind) {
            return info;
        }
    }
    return kAllocatorInfos[0];
}

[[nodiscard]] bool Supports(AllocatorKind kind, Scenario scenario) noexcept
{
    return (Info(kind).scenarios & static_cast<uint32_t>(scenario)) != 0;
}

template<typename F>
void WithAllocator(AllocatorKind kind, F&& fn)
{
    Engine::Memory::IAllocator& system = Engine::Memory::GetDefaultAllocator();
    switch (kind) {
    case AllocatorKind::System:
        fn(system);
        break;
    case AllocatorKind::Malloc: {
        Engine::Memory::MallocAllocator allocator;
        fn(allocator);
        break;
    }
    case AllocatorKind::Aligned64: {
        Engine::Memory::AlignedAllocator allocator(64, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Binned: {
        Engine::Memory::BinnedAllocator allocator(&system);
        fn(allocator);
        break;
    }
    case AllocatorKind::TLSF: {
        Engine::Memory::TLSFAllocator allocator(kArenaSize, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Buddy: {
        Engine::Memory::BuddyAllocator allocator(kArenaSize, 256, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Linear: {
        Engine::Memory::LinearAllocator allocator(kArenaSize, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Frame: {
        Engine::Memory::FrameAllocator allocator(kArenaSize, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Stack: {
        Engine::Memory::StackAllocator allocator(kArenaSize, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Pool: {
        Engine::Memory::PoolAllocator allocator(kPoolBlockSize, kPoolBlockCount, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Monotonic: {
        Engine::Memory::MonotonicAllocator allocator(system, 64ull * 1024ull);
        fn(allocator);
        break;
    }
    case AllocatorKind::ThreadSafe: {
        Engine::Memory::ThreadSafeAllocator allocator(&system);
        fn(allocator);
        break;
    }
    case AllocatorKind::ThreadSafeLinear: {
        Engine::Memory::ThreadSafeLinearAllocator allocator(kArenaSize, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::ThreadLocalLinear: {
        Engine::Memory::ThreadLocalLinearAllocator allocator(kArenaSize, &system);
        fn(allocator);
        break;
    }
    case AllocatorKind::TLSCaching: {
        Engine::Memory::ThreadSafeAllocator safeAllocator(&system);
        Engine::Memory::TLSCachingAllocator allocator(&safeAllocator);
        fn(allocator);
        allocator.FlushCache();
        break;
    }
    case AllocatorKind::Tracked: {
        Engine::Memory::TrackedAllocator allocator(&system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Debug: {
        Engine::Memory::DebugAllocator allocator(&system);
        fn(allocator);
        break;
    }
    case AllocatorKind::Secured: {
        Engine::Memory::SecuredAllocator allocator;
        fn(allocator);
        break;
    }
    }
}

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::duration<double, std::milli>;

template<typename F>
double TimeMs(F&& fn)
{
    const auto start = Clock::now();
    fn();
    return Ms(Clock::now() - start).count();
}

template<typename F>
std::vector<double> Run(AllocatorKind allocatorKind, int runs, F&& fn)
{
    std::vector<double> times;
    times.reserve(static_cast<size_t>(runs));
    for (int run = 0; run < runs; ++run) {
        times.push_back(TimeMs([&] {
            WithAllocator(allocatorKind, fn);
        }));
    }
    return times;
}

double Median(std::vector<double>& values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

void PrintResult(AllocatorKind allocatorKind, const char* scenario, const char* implementation, std::vector<double> values)
{
    const double median = Median(values);
    std::printf("%-24s %-30s %-34s median %8.3f ms  min %8.3f  max %8.3f\n",
        Info(allocatorKind).name, scenario, implementation, median, values.front(), values.back());
}

void PrintSkip(AllocatorKind allocatorKind, Scenario scenario, const char* scenarioName)
{
    if (!Supports(allocatorKind, scenario)) {
        std::printf("%-24s %-30s %-34s skipped\n",
            Info(allocatorKind).name, scenarioName, "allocator model mismatch");
    }
}

std::vector<int> MakeSource(size_t count)
{
    std::vector<int> source(count);
    for (size_t index = 0; index < count; ++index) {
        source[index] = static_cast<int>(index);
    }
    return source;
}

uint64_t SumPointer(const int* data, uint32 count)
{
    uint64_t sum = 0;
    for (uint32 index = 0; index < count; ++index) {
        sum += static_cast<uint32_t>(data[index]);
    }
    return sum;
}

std::vector<double> BenchLegacyReservedPush(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        LegacyAllocatorAdapter legacyAllocator(allocator);
        ::Array<int> values;
        values.init(&legacyAllocator, static_cast<uint32_t>(count));
        for (uint32 i = 0; i < count; ++i) {
            values.push(static_cast<int>(i));
        }
        g_sink += SumPointer(values.data, values.size);
        values.shutdown();
    });
}

std::vector<double> BenchArrayReservedPush(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        values.Reserve(static_cast<uint32>(count));
        for (uint32 i = 0; i < count; ++i) {
            values.PushBack(static_cast<int>(i));
        }
        g_sink += SumPointer(values.Data(), values.Size());
    });
}

std::vector<double> BenchArrayWriter(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        auto writer = values.BeginAppend(static_cast<uint32>(count));
        for (uint32 i = 0; i < count; ++i) {
            writer.Emplace(static_cast<int>(i));
        }
        writer.Commit();
        g_sink += SumPointer(values.Data(), values.Size());
    });
}

std::vector<double> BenchArrayUninitialized(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        int* data = values.AddUninitialized(static_cast<uint32>(count));
        for (uint32 i = 0; i < count; ++i) {
            data[i] = static_cast<int>(i);
        }
        g_sink += SumPointer(data, values.Size());
    });
}

std::vector<double> BenchArrayAppendRange(AllocatorKind allocatorKind, const std::vector<int>& source, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        values.AppendRange(source.data(), static_cast<uint32>(source.size()));
        g_sink += SumPointer(values.Data(), values.Size());
    });
}

std::vector<double> BenchChunkedReservedPush(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::ChunkedArray<int, 256> values(allocator);
        values.Reserve(static_cast<uint32>(count));
        for (uint32 i = 0; i < count; ++i) {
            values.PushBack(static_cast<int>(i));
        }
        uint64_t sum = 0;
        for (uint32 i = 0; i < values.Size(); ++i) {
            sum += static_cast<uint32_t>(values[i]);
        }
        g_sink += sum;
    });
}

std::vector<double> BenchChunkedWriter(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::ChunkedArray<int, 256> values(allocator);
        auto writer = values.BeginAppend(static_cast<uint32>(count));
        for (uint32 i = 0; i < count; ++i) {
            writer.Emplace(static_cast<int>(i));
        }
        writer.Commit();
        uint64_t sum = 0;
        for (uint32 i = 0; i < values.Size(); ++i) {
            sum += static_cast<uint32_t>(values[i]);
        }
        g_sink += sum;
    });
}

std::vector<double> BenchChunkedAppendRange(AllocatorKind allocatorKind, const std::vector<int>& source, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::ChunkedArray<int, 256> values(allocator);
        values.AppendRange(source.data(), static_cast<uint32>(source.size()));
        uint64_t sum = 0;
        for (uint32 i = 0; i < values.Size(); ++i) {
            sum += static_cast<uint32_t>(values[i]);
        }
        g_sink += sum;
    });
}

std::vector<double> BenchLegacyGrowthPush(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        LegacyAllocatorAdapter legacyAllocator(allocator);
        ::Array<int> values;
        values.init(&legacyAllocator, 0);
        for (uint32 i = 0; i < count; ++i) {
            values.push(static_cast<int>(i));
        }
        g_sink += static_cast<uint64_t>(values.back());
        values.shutdown();
    });
}

std::vector<double> BenchArrayGrowthPush(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        for (uint32 i = 0; i < count; ++i) {
            values.PushBack(static_cast<int>(i));
        }
        g_sink += static_cast<uint64_t>(values.Back());
    });
}

std::vector<double> BenchChunkedGrowthPush(AllocatorKind allocatorKind, size_t count, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::ChunkedArray<int, 256> values(allocator);
        for (uint32 i = 0; i < count; ++i) {
            values.PushBack(static_cast<int>(i));
        }
        g_sink += static_cast<uint64_t>(values[values.Size() - 1]);
    });
}

std::vector<double> BenchLegacyClearRefill(AllocatorKind allocatorKind, size_t count, int rounds, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        LegacyAllocatorAdapter legacyAllocator(allocator);
        ::Array<int> values;
        values.init(&legacyAllocator, static_cast<uint32_t>(count));
        for (int round = 0; round < rounds; ++round) {
            values.clear();
            for (uint32 i = 0; i < count; ++i) {
                values.push(static_cast<int>(i + round));
            }
        }
        g_sink += static_cast<uint64_t>(values.back());
        values.shutdown();
    });
}

std::vector<double> BenchArrayClearRefill(AllocatorKind allocatorKind, size_t count, int rounds, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        values.Reserve(static_cast<uint32>(count));
        for (int round = 0; round < rounds; ++round) {
            values.Clear();
            for (uint32 i = 0; i < count; ++i) {
                values.PushBack(static_cast<int>(i + round));
            }
        }
        g_sink += static_cast<uint64_t>(values.Back());
    });
}

std::vector<double> BenchArrayWriterRefill(AllocatorKind allocatorKind, size_t count, int rounds, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::Array<int> values(allocator);
        values.Reserve(static_cast<uint32>(count));
        for (int round = 0; round < rounds; ++round) {
            values.Clear();
            auto writer = values.BeginAppend(static_cast<uint32>(count));
            for (uint32 i = 0; i < count; ++i) {
                writer.Emplace(static_cast<int>(i + round));
            }
            writer.Commit();
        }
        g_sink += static_cast<uint64_t>(values.Back());
    });
}

std::vector<double> BenchChunkedWriterRefill(AllocatorKind allocatorKind, size_t count, int rounds, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        Engine::Containers::ChunkedArray<int, 256> values(allocator);
        values.Reserve(static_cast<uint32>(count));
        for (int round = 0; round < rounds; ++round) {
            values.Clear();
            auto writer = values.BeginAppend(static_cast<uint32>(count));
            for (uint32 i = 0; i < count; ++i) {
                writer.Emplace(static_cast<int>(i + round));
            }
            writer.Commit();
        }
        g_sink += static_cast<uint64_t>(values[values.Size() - 1]);
    });
}

std::vector<double> BenchLegacySmallArrays(AllocatorKind allocatorKind, size_t arrays, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        LegacyAllocatorAdapter legacyAllocator(allocator);
        uint64_t sum = 0;
        for (size_t n = 0; n < arrays; ++n) {
            ::Array<int> values;
            values.init(&legacyAllocator, 0);
            for (int i = 0; i < 32; ++i) {
                values.push(i);
            }
            sum += static_cast<uint32_t>(values.back());
            values.shutdown();
        }
        g_sink += sum;
    });
}

std::vector<double> BenchArraySmallSbo(AllocatorKind allocatorKind, size_t arrays, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        uint64_t sum = 0;
        for (size_t n = 0; n < arrays; ++n) {
            Engine::Containers::Array<int, 32> values(allocator);
            for (int i = 0; i < 32; ++i) {
                values.PushBack(i);
            }
            sum += static_cast<uint32_t>(values.Back());
        }
        g_sink += sum;
    });
}

std::vector<double> BenchChunkedSmall(AllocatorKind allocatorKind, size_t arrays, int runs)
{
    return Run(allocatorKind, runs, [&](Engine::Memory::IAllocator& allocator) {
        uint64_t sum = 0;
        for (size_t n = 0; n < arrays; ++n) {
            Engine::Containers::ChunkedArray<int, 32> values(allocator);
            for (int i = 0; i < 32; ++i) {
                values.PushBack(i);
            }
            sum += static_cast<uint32_t>(values[31]);
        }
        g_sink += sum;
    });
}

void RunAllocatorBenchmarks(AllocatorKind allocatorKind, const std::vector<int>& source, int runs)
{
    if (Supports(allocatorKind, ScenarioContiguous)) {
        PrintResult(allocatorKind, "reserved push + sum", "legacy Foundation/Array", BenchLegacyReservedPush(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "reserved push + sum", "Array PushBack", BenchArrayReservedPush(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "reserved fill + sum", "Array AppendWriter", BenchArrayWriter(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "reserved fill + sum", "Array AddUninitialized", BenchArrayUninitialized(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "append range + sum", "Array AppendRange", BenchArrayAppendRange(allocatorKind, source, runs));
    } else {
        PrintSkip(allocatorKind, ScenarioContiguous, "reserved contiguous");
    }

    if (Supports(allocatorKind, ScenarioChunked)) {
        PrintResult(allocatorKind, "reserved push + sum", "ChunkedArray<256> PushBack", BenchChunkedReservedPush(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "reserved fill + sum", "ChunkedArray<256> Writer", BenchChunkedWriter(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "append range + sum", "ChunkedArray<256> Range", BenchChunkedAppendRange(allocatorKind, source, runs));
    } else {
        PrintSkip(allocatorKind, ScenarioChunked, "chunked array");
    }

    if (Supports(allocatorKind, ScenarioGrowth)) {
        PrintResult(allocatorKind, "growth push", "legacy Foundation/Array", BenchLegacyGrowthPush(allocatorKind, kLargeCount, runs));
        PrintResult(allocatorKind, "growth push", "Array PushBack", BenchArrayGrowthPush(allocatorKind, kLargeCount, runs));
        if (Supports(allocatorKind, ScenarioChunked)) {
            PrintResult(allocatorKind, "growth push", "ChunkedArray<256> PushBack", BenchChunkedGrowthPush(allocatorKind, kLargeCount, runs));
        }
    } else {
        PrintSkip(allocatorKind, ScenarioGrowth, "growth push");
    }

    if (Supports(allocatorKind, ScenarioRefill)) {
        PrintResult(allocatorKind, "clear + refill", "legacy Foundation/Array", BenchLegacyClearRefill(allocatorKind, kLargeCount, kRefillRounds, runs));
        PrintResult(allocatorKind, "clear + refill", "Array PushBack", BenchArrayClearRefill(allocatorKind, kLargeCount, kRefillRounds, runs));
        PrintResult(allocatorKind, "clear + refill", "Array AppendWriter", BenchArrayWriterRefill(allocatorKind, kLargeCount, kRefillRounds, runs));
        if (Supports(allocatorKind, ScenarioChunked)) {
            PrintResult(allocatorKind, "clear + refill", "ChunkedArray<256> Writer", BenchChunkedWriterRefill(allocatorKind, kLargeCount, kRefillRounds, runs));
        }
    } else {
        PrintSkip(allocatorKind, ScenarioRefill, "clear + refill");
    }

    if (Supports(allocatorKind, ScenarioSmall)) {
        PrintResult(allocatorKind, "small arrays", "legacy Foundation/Array", BenchLegacySmallArrays(allocatorKind, kSmallArrays, runs));
        PrintResult(allocatorKind, "small arrays", "Array<int,32> SBO", BenchArraySmallSbo(allocatorKind, kSmallArrays, runs));
        if (Supports(allocatorKind, ScenarioChunked)) {
            PrintResult(allocatorKind, "small arrays", "ChunkedArray<int,32>", BenchChunkedSmall(allocatorKind, kSmallArrays, runs));
        }
    } else {
        PrintSkip(allocatorKind, ScenarioSmall, "small arrays");
    }
}

void PrintAllocatorList()
{
    std::printf("Available allocators:\n");
    std::printf("  all\n");
    for (const AllocatorInfo& info : kAllocatorInfos) {
        std::printf("  %s\n", info.arg);
    }
}

bool TryParseAllocator(std::string_view value, AllocatorKind& kind)
{
    for (const AllocatorInfo& info : kAllocatorInfos) {
        if (value == info.arg) {
            kind = info.kind;
            return true;
        }
    }
    return false;
}

std::vector<AllocatorKind> AllAllocatorKinds()
{
    std::vector<AllocatorKind> result;
    result.reserve(sizeof(kAllocatorInfos) / sizeof(kAllocatorInfos[0]));
    for (const AllocatorInfo& info : kAllocatorInfos) {
        result.push_back(info.kind);
    }
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    int runs = 7;
    std::vector<AllocatorKind> allocators = AllAllocatorKinds();

    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--runs=", 7) == 0) {
            runs = std::atoi(argv[i] + 7);
            if (runs <= 0) {
                runs = 1;
            }
        } else if (std::strcmp(argv[i], "--list-allocators") == 0) {
            PrintAllocatorList();
            return 0;
        } else if (std::strncmp(argv[i], "--allocator=", 12) == 0) {
            const std::string_view value(argv[i] + 12);
            if (value == "all") {
                allocators = AllAllocatorKinds();
                continue;
            }

            AllocatorKind kind{};
            if (!TryParseAllocator(value, kind)) {
                std::fprintf(stderr, "Unknown allocator '%.*s'. Use --list-allocators.\n",
                    static_cast<int>(value.size()), value.data());
                return 2;
            }
            allocators.clear();
            allocators.push_back(kind);
        }
    }

    const std::vector<int> source = MakeSource(kLargeCount);

    std::printf("Container benchmark, MSVC /O2 /EHsc /DNDEBUG, int elements, median of %d runs\n", runs);
    std::printf("Large count: %zu, refill rounds: %d, small arrays: %zu x 32 ints\n", kLargeCount, kRefillRounds, kSmallArrays);
    std::printf("%-24s %-30s %-34s %s\n", "Allocator", "Scenario", "Implementation", "Time");

    for (AllocatorKind allocator : allocators) {
        RunAllocatorBenchmarks(allocator, source, runs);
    }

    std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
    return 0;
}
