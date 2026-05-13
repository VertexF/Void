#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Foundation/Memory/Allocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/BuddyAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/SecuredAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/TLSCachingAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadLocalLinearAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeAllocator.hpp>
#include <Foundation/Memory/Allocators/Advanced/ThreadSafeLinearAllocator.hpp>
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
#include <Foundation/Memory/Operations.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using Engine::Memory::IAllocator;

static volatile std::uint64_t g_sink = 0;
static std::atomic<int> g_failures{0};

constexpr std::size_t kArenaSize = 64ull * 1024ull * 1024ull;
constexpr std::size_t kPoolBlockSize = 256;
constexpr std::size_t kPoolBlockCount = 65536;
constexpr int kArenaRounds = 64;
constexpr std::size_t kArenaBlocksPerRound = 4096;
constexpr int kThreadCount = 8;

enum class AllocatorKind {
    Malloc,
    Aligned64,
    Binned,
    TLSF,
    Buddy,
    Pool,
    Linear,
    Frame,
    Stack,
    Monotonic,
    ThreadSafe,
    ThreadSafeLinear,
    ThreadLocalLinear,
    TLSCaching,
    Tracked,
    Debug,
    Secured
};

enum Scenario : std::uint32_t {
    ScenarioFixed = 1u << 0u,
    ScenarioMixed = 1u << 1u,
    ScenarioArena = 1u << 2u,
    ScenarioThreaded = 1u << 3u
};

struct AllocatorInfo {
    AllocatorKind kind;
    const char* arg;
    const char* name;
    std::uint32_t scenarios;
    std::size_t fixedCount;
    std::size_t mixedOperations;
    std::size_t threadedAllocationsPerThread;
    double fixedGateMs;
    double mixedGateMs;
    double arenaGateMs;
    double threadedGateMs;
};

constexpr AllocatorInfo kAllocatorInfos[] = {
    {AllocatorKind::Malloc, "malloc", "Malloc", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 25.0, 75.0, 0.0, 120.0},
    {AllocatorKind::Aligned64, "aligned64", "Aligned64", ScenarioFixed | ScenarioMixed, 32768, 65536, 0, 35.0, 90.0, 0.0, 0.0},
    {AllocatorKind::Binned, "binned", "Binned", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 45.0, 110.0, 0.0, 160.0},
    {AllocatorKind::TLSF, "tlsf", "TLSF-64MB", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 45.0, 110.0, 0.0, 160.0},
    {AllocatorKind::Buddy, "buddy", "Buddy-64MB", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 80.0, 500.0, 0.0, 500.0},
    {AllocatorKind::Pool, "pool", "Pool-256x65536", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 30.0, 80.0, 0.0, 140.0},
    {AllocatorKind::Linear, "linear", "Linear-64MB", ScenarioArena, 0, 0, 0, 0.0, 0.0, 40.0, 0.0},
    {AllocatorKind::Frame, "frame", "Frame-64MB", ScenarioArena, 0, 0, 0, 0.0, 0.0, 40.0, 0.0},
    {AllocatorKind::Stack, "stack", "Stack-64MB", ScenarioArena, 0, 0, 0, 0.0, 0.0, 55.0, 0.0},
    {AllocatorKind::Monotonic, "monotonic", "Monotonic", ScenarioArena, 0, 0, 0, 0.0, 0.0, 70.0, 0.0},
    {AllocatorKind::ThreadSafe, "threadsafe", "ThreadSafe", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 60.0, 140.0, 0.0, 260.0},
    {AllocatorKind::ThreadSafeLinear, "threadsafe-linear", "ThreadSafeLinear-64MB", ScenarioArena, 0, 0, 0, 0.0, 0.0, 90.0, 0.0},
    {AllocatorKind::ThreadLocalLinear, "threadlocal-linear", "ThreadLocalLinear-64MB", ScenarioArena, 0, 0, 0, 0.0, 0.0, 70.0, 0.0},
    {AllocatorKind::TLSCaching, "tls-cache", "TLSCaching", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 45.0, 110.0, 0.0, 160.0},
    {AllocatorKind::Tracked, "tracked", "Tracked", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 32768, 65536, 4096, 180.0, 280.0, 0.0, 420.0},
    {AllocatorKind::Debug, "debug", "Debug", ScenarioFixed | ScenarioMixed | ScenarioThreaded, 16384, 32768, 2048, 260.0, 360.0, 0.0, 520.0},
    {AllocatorKind::Secured, "secured", "Secured", ScenarioFixed | ScenarioMixed, 512, 2048, 0, 700.0, 1800.0, 0.0, 0.0}
};

struct Allocation {
    void* ptr = nullptr;
    std::size_t size = 0;
    std::uint8_t pattern = 0;
};

struct BenchmarkResult {
    AllocatorKind allocator;
    const char* scenario = nullptr;
    std::size_t operations = 0;
    double medianMs = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    double gateMs = 0.0;
    bool gated = false;
};

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::duration<double, std::milli>;

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
    return (Info(kind).scenarios & static_cast<std::uint32_t>(scenario)) != 0u;
}

[[nodiscard]] double TimeMs(auto&& fn)
{
    const auto start = Clock::now();
    fn();
    return Ms(Clock::now() - start).count();
}

[[nodiscard]] double Median(std::vector<double>& values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2u];
}

void RecordFailure(const char* allocatorName, const char* scenario, const char* message)
{
    std::fprintf(stderr, "benchmark failure: %s / %s: %s\n", allocatorName, scenario, message);
    g_failures.fetch_add(1, std::memory_order_relaxed);
}

void FillPattern(Allocation allocation)
{
    auto* bytes = static_cast<std::uint8_t*>(allocation.ptr);
    for (std::size_t i = 0; i < allocation.size; ++i) {
        bytes[i] = static_cast<std::uint8_t>(allocation.pattern + static_cast<std::uint8_t>(i * 13u));
    }
}

[[nodiscard]] bool PatternMatches(const Allocation& allocation) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(allocation.ptr);
    for (std::size_t i = 0; i < allocation.size; ++i) {
        if (bytes[i] != static_cast<std::uint8_t>(allocation.pattern + static_cast<std::uint8_t>(i * 13u))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool PatternPrefixMatches(const Allocation& allocation, std::size_t count) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(allocation.ptr);
    for (std::size_t i = 0; i < count; ++i) {
        if (bytes[i] != static_cast<std::uint8_t>(allocation.pattern + static_cast<std::uint8_t>(i * 13u))) {
            return false;
        }
    }
    return true;
}

void FlushOrReset(IAllocator& allocator)
{
    if (auto* tls = dynamic_cast<Engine::Memory::TLSCachingAllocator*>(&allocator)) {
        tls->FlushCache();
    } else if (auto* linear = dynamic_cast<Engine::Memory::LinearAllocator*>(&allocator)) {
        linear->Reset();
    } else if (auto* frame = dynamic_cast<Engine::Memory::FrameAllocator*>(&allocator)) {
        frame->BeginFrame();
    } else if (auto* stack = dynamic_cast<Engine::Memory::StackAllocator*>(&allocator)) {
        stack->Reset();
    } else if (auto* monotonic = dynamic_cast<Engine::Memory::MonotonicAllocator*>(&allocator)) {
        monotonic->Reset();
    } else if (auto* threadSafeLinear = dynamic_cast<Engine::Memory::ThreadSafeLinearAllocator*>(&allocator)) {
        threadSafeLinear->Reset();
    } else if (auto* threadLocalLinear = dynamic_cast<Engine::Memory::ThreadLocalLinearAllocator*>(&allocator)) {
        threadLocalLinear->Reset();
    }
}

template<typename F>
void WithAllocator(AllocatorKind kind, F&& fn)
{
    IAllocator& system = Engine::Memory::GetDefaultAllocator();
    switch (kind) {
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
    case AllocatorKind::Pool: {
        Engine::Memory::PoolAllocator allocator(kPoolBlockSize, kPoolBlockCount, &system, 16);
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
    case AllocatorKind::Monotonic: {
        Engine::Memory::MonotonicAllocator allocator(system, 64ull * 1024ull);
        fn(allocator);
        break;
    }
    case AllocatorKind::ThreadSafe: {
        Engine::Memory::MallocAllocator backing;
        Engine::Memory::ThreadSafeAllocator allocator(&backing);
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
        Engine::Memory::MallocAllocator backing;
        Engine::Memory::ThreadSafeAllocator safeAllocator(&backing);
        Engine::Memory::TLSCachingAllocator allocator(&safeAllocator);
        fn(allocator);
        allocator.FlushCache();
        break;
    }
    case AllocatorKind::Tracked: {
        Engine::Memory::MallocAllocator backing;
        Engine::Memory::TrackedAllocator allocator(&backing);
        fn(allocator);
        break;
    }
    case AllocatorKind::Debug: {
        Engine::Memory::MallocAllocator backing;
        Engine::Memory::DebugAllocator allocator(&backing);
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

template<typename F>
std::vector<double> RunSamples(AllocatorKind allocatorKind, int runs, F&& fn)
{
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(runs));
    for (int run = 0; run < runs; ++run) {
        double elapsedMs = 0.0;
        WithAllocator(allocatorKind, [&](IAllocator& allocator) {
            elapsedMs = fn(allocator);
            FlushOrReset(allocator);
            if (allocator.AllocatedSize() != 0u) {
                RecordFailure(Info(allocatorKind).name, "cleanup", "allocator retained live bytes after workload cleanup");
            }
        });
        samples.push_back(elapsedMs);
    }
    return samples;
}

[[nodiscard]] BenchmarkResult MakeResult(
    AllocatorKind allocator,
    const char* scenario,
    std::size_t operations,
    double gateMs,
    std::vector<double> samples)
{
    const double median = Median(samples);
    return {
        allocator,
        scenario,
        operations,
        median,
        samples.front(),
        samples.back(),
        gateMs,
        gateMs > 0.0
    };
}

std::vector<double> BenchFixedAllocFree(AllocatorKind allocatorKind, int runs)
{
    const AllocatorInfo& info = Info(allocatorKind);
    return RunSamples(allocatorKind, runs, [&](IAllocator& allocator) {
        std::vector<void*> pointers(info.fixedCount, nullptr);
        return TimeMs([&] {
            for (std::size_t i = 0; i < pointers.size(); ++i) {
                void* ptr = allocator.Allocate(64, 16);
                if (!ptr) {
                    RecordFailure(info.name, "fixed alloc/free", "allocation failed");
                    break;
                }
                auto* bytes = static_cast<std::uint8_t*>(ptr);
                bytes[0] = static_cast<std::uint8_t>(i);
                bytes[63] = static_cast<std::uint8_t>(i >> 8u);
                pointers[i] = ptr;
            }
            for (std::size_t i = pointers.size(); i > 0; --i) {
                if (pointers[i - 1u]) {
                    g_sink += reinterpret_cast<std::uintptr_t>(pointers[i - 1u]) & 0xFFu;
                    allocator.Free(pointers[i - 1u]);
                }
            }
        });
    });
}

std::vector<double> BenchMixedChurn(AllocatorKind allocatorKind, int runs)
{
    const AllocatorInfo& info = Info(allocatorKind);
    const std::size_t maxSize = allocatorKind == AllocatorKind::Pool ? kPoolBlockSize : 1024u;
    return RunSamples(allocatorKind, runs, [&](IAllocator& allocator) {
        std::vector<Allocation> live;
        live.reserve(2048);
        return TimeMs([&] {
            std::mt19937 rng(0x51A771u);
            std::uniform_int_distribution<int> actionDist(0, 99);
            std::uniform_int_distribution<std::size_t> sizeDist(1, maxSize);

            for (std::size_t operation = 0; operation < info.mixedOperations; ++operation) {
                const int action = actionDist(rng);
                if (live.empty() || action < 54) {
                    Allocation allocation{};
                    allocation.size = sizeDist(rng);
                    allocation.pattern = static_cast<std::uint8_t>(operation & 0xFFu);
                    allocation.ptr = allocator.Allocate(allocation.size, 16);
                    if (allocation.ptr) {
                        FillPattern(allocation);
                        live.push_back(allocation);
                    }
                    continue;
                }

                const std::size_t index = static_cast<std::size_t>(rng()) % live.size();
                Allocation allocation = live[index];
                if (!PatternMatches(allocation)) {
                    RecordFailure(info.name, "mixed churn", "payload corrupted before action");
                }

                if (action < 80) {
                    allocator.Free(allocation.ptr);
                    live[index] = live.back();
                    live.pop_back();
                    continue;
                }

                const std::size_t newSize = sizeDist(rng);
                void* reallocated = allocator.Reallocate(allocation.ptr, newSize, 16);
                if (!reallocated) {
                    if (!PatternMatches(allocation)) {
                        RecordFailure(info.name, "mixed churn", "payload corrupted after failed reallocate");
                    }
                    continue;
                }

                allocation.ptr = reallocated;
                if (!PatternPrefixMatches(allocation, allocation.size < newSize ? allocation.size : newSize)) {
                    RecordFailure(info.name, "mixed churn", "payload corrupted after reallocate");
                }
                allocation.size = newSize;
                allocation.pattern = static_cast<std::uint8_t>((operation * 17u) & 0xFFu);
                FillPattern(allocation);
                live[index] = allocation;
            }

            for (Allocation allocation : live) {
                if (!PatternMatches(allocation)) {
                    RecordFailure(info.name, "mixed churn", "payload corrupted during cleanup");
                }
                allocator.Free(allocation.ptr);
            }
            live.clear();
        });
    });
}

std::vector<double> BenchArenaReset(AllocatorKind allocatorKind, int runs)
{
    return RunSamples(allocatorKind, runs, [&](IAllocator& allocator) {
        return TimeMs([&] {
            for (int round = 0; round < kArenaRounds; ++round) {
                for (std::size_t block = 0; block < kArenaBlocksPerRound; ++block) {
                    void* ptr = allocator.Allocate(64, 16);
                    if (!ptr) {
                        RecordFailure(Info(allocatorKind).name, "arena reset", "allocation failed");
                        break;
                    }
                    auto* bytes = static_cast<std::uint8_t*>(ptr);
                    bytes[0] = static_cast<std::uint8_t>(block);
                    g_sink += bytes[0];
                }
                FlushOrReset(allocator);
            }
        });
    });
}

std::vector<double> BenchThreadedAllocFree(AllocatorKind allocatorKind, int runs)
{
    const AllocatorInfo& info = Info(allocatorKind);
    return RunSamples(allocatorKind, runs, [&](IAllocator& allocator) {
        return TimeMs([&] {
            std::atomic<int> ready{0};
            std::atomic<bool> start{false};
            std::vector<std::uint64_t> threadSinks(static_cast<std::size_t>(kThreadCount), 0u);
            std::vector<std::thread> threads;
            threads.reserve(static_cast<std::size_t>(kThreadCount));

            for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
                threads.emplace_back([&, threadIndex] {
                    std::vector<void*> pointers(info.threadedAllocationsPerThread, nullptr);
                    ready.fetch_add(1, std::memory_order_release);
                    while (!start.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }

                    for (std::size_t i = 0; i < pointers.size(); ++i) {
                        void* ptr = allocator.Allocate(64, 16);
                        if (!ptr) {
                            RecordFailure(info.name, "threaded alloc/free", "allocation failed");
                            break;
                        }
                        auto* bytes = static_cast<std::uint8_t*>(ptr);
                        bytes[0] = static_cast<std::uint8_t>(i + static_cast<std::size_t>(threadIndex));
                        bytes[63] = static_cast<std::uint8_t>(threadIndex);
                        pointers[i] = ptr;
                    }

                    for (std::size_t i = pointers.size(); i > 0; --i) {
                        void* ptr = pointers[i - 1u];
                        if (ptr) {
                            threadSinks[static_cast<std::size_t>(threadIndex)] +=
                                reinterpret_cast<std::uintptr_t>(ptr) & 0xFFu;
                            allocator.Free(ptr);
                        }
                    }
                });
            }

            while (ready.load(std::memory_order_acquire) != kThreadCount) {
                std::this_thread::yield();
            }
            start.store(true, std::memory_order_release);

            for (std::thread& thread : threads) {
                thread.join();
            }

            for (std::uint64_t value : threadSinks) {
                g_sink += value;
            }
        });
    });
}

void PrintResult(const BenchmarkResult& result, bool gate)
{
    const double nsPerOp = result.operations > 0u
        ? (result.medianMs * 1'000'000.0) / static_cast<double>(result.operations)
        : 0.0;
    const bool pass = !gate || !result.gated || result.medianMs <= result.gateMs;
    std::printf("%-20s %-24s %9zu ops median %8.3f ms  %9.2f ns/op  min %8.3f  max %8.3f",
        Info(result.allocator).name,
        result.scenario,
        result.operations,
        result.medianMs,
        nsPerOp,
        result.minMs,
        result.maxMs);
    if (gate && result.gated) {
        std::printf("  gate <= %8.3f ms  %s", result.gateMs, pass ? "PASS" : "FAIL");
        if (!pass) {
            g_failures.fetch_add(1, std::memory_order_relaxed);
        }
    }
    std::printf("\n");
}

void PrintSkip(AllocatorKind allocator, const char* scenario)
{
    std::printf("%-20s %-24s skipped\n", Info(allocator).name, scenario);
}

void RunAllocatorBenchmarks(AllocatorKind allocatorKind, int runs, bool gate)
{
    const AllocatorInfo& info = Info(allocatorKind);
    if (Supports(allocatorKind, ScenarioFixed)) {
        PrintResult(MakeResult(
            allocatorKind,
            "fixed alloc/free 64B",
            info.fixedCount * 2u,
            info.fixedGateMs,
            BenchFixedAllocFree(allocatorKind, runs)), gate);
    } else {
        PrintSkip(allocatorKind, "fixed alloc/free 64B");
    }

    if (Supports(allocatorKind, ScenarioMixed)) {
        PrintResult(MakeResult(
            allocatorKind,
            "mixed realloc churn",
            info.mixedOperations,
            info.mixedGateMs,
            BenchMixedChurn(allocatorKind, runs)), gate);
    } else {
        PrintSkip(allocatorKind, "mixed realloc churn");
    }

    if (Supports(allocatorKind, ScenarioArena)) {
        PrintResult(MakeResult(
            allocatorKind,
            "arena reset alloc",
            static_cast<std::size_t>(kArenaRounds) * kArenaBlocksPerRound,
            info.arenaGateMs,
            BenchArenaReset(allocatorKind, runs)), gate);
    } else {
        PrintSkip(allocatorKind, "arena reset alloc");
    }

    if (Supports(allocatorKind, ScenarioThreaded)) {
        PrintResult(MakeResult(
            allocatorKind,
            "threaded alloc/free",
            static_cast<std::size_t>(kThreadCount) * info.threadedAllocationsPerThread * 2u,
            info.threadedGateMs,
            BenchThreadedAllocFree(allocatorKind, runs)), gate);
    } else {
        PrintSkip(allocatorKind, "threaded alloc/free");
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
    int runs = 5;
    bool gate = false;
    std::vector<AllocatorKind> allocators = AllAllocatorKinds();

    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--runs=", 7) == 0) {
            runs = std::atoi(argv[i] + 7);
            if (runs <= 0) {
                runs = 1;
            }
        } else if (std::strcmp(argv[i], "--gate") == 0) {
            gate = true;
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
                    static_cast<int>(value.size()),
                    value.data());
                return 2;
            }
            allocators.clear();
            allocators.push_back(kind);
        } else {
            std::fprintf(stderr, "Unknown argument '%s'.\n", argv[i]);
            return 2;
        }
    }

    std::printf("Allocator benchmark gates, median of %d runs%s\n", runs, gate ? ", gate mode" : "");
    std::printf("Arena size: %zu MiB, pool: %zu x %zu B, threaded workers: %d\n",
        kArenaSize / (1024u * 1024u),
        kPoolBlockCount,
        kPoolBlockSize,
        kThreadCount);
    std::printf("%-20s %-24s %s\n", "Allocator", "Scenario", "Result");

    for (AllocatorKind allocator : allocators) {
        RunAllocatorBenchmarks(allocator, runs, gate);
    }

    std::printf("sink=%llu\n", static_cast<unsigned long long>(g_sink));
    const int failures = g_failures.load(std::memory_order_relaxed);
    if (failures != 0) {
        std::fprintf(stderr, "%d allocator benchmark gate failure(s)\n", failures);
        return 1;
    }
    return 0;
}
