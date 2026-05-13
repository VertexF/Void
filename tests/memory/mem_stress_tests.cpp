#include <Foundation/Containers/Vector.hpp>
#include <Foundation/Memory/TLSFAllocator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <thread>

namespace Engine::Memory {

TEST(MemoryStress, TLSFDeterministicMixedAllocFree)
{
    TLSFAllocator allocator(512 * 1024);
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<size_t> sizeDist(1, 1024);
    std::uniform_int_distribution<int> actionDist(0, 2);

    Vector<void*> live;
    live.reserve(512);

    for (int i = 0; i < 5000; ++i) {
        if (!live.empty() && actionDist(rng) == 0) {
            const size_t index = static_cast<size_t>(rng()) % live.size();
            allocator.Free(live[index]);
            live[index] = live.back();
            live.pop_back();
            continue;
        }

        void* ptr = allocator.Allocate(sizeDist(rng));
        if (ptr) {
            live.push_back(ptr);
        }
    }

    for (void* ptr : live) {
        allocator.Free(ptr);
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

TEST(MemoryStress, TLSFConcurrentRepeatedAllocFree)
{
    TLSFAllocator allocator(1024 * 1024);
    constexpr int kThreadCount = 8;
    constexpr int kIterations = 1000;

    auto worker = [&allocator](int threadIndex) {
        Vector<void*> live;
        live.reserve(64);

        for (int i = 0; i < kIterations; ++i) {
            const size_t size = 16u + static_cast<size_t>((threadIndex * 13 + i) % 240);
            void* ptr = allocator.Allocate(size);
            if (ptr) {
                live.push_back(ptr);
            }

            if (live.size() >= 32) {
                allocator.Free(live.front());
                live.erase(live.begin());
            }
        }

        for (void* ptr : live) {
            allocator.Free(ptr);
        }
    };

    Vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back(worker, i);
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(allocator.AllocatedSize(), 0u);
}

} // namespace Engine::Memory
