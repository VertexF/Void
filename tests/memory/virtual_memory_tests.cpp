#include <Foundation/Memory/VirtualMemory.hpp>

#include <gtest/gtest.h>

namespace Engine::Memory {

TEST(VirtualMemory, ReserveCommitProtectAndRelease)
{
    UniquePtr<IVirtualMemory> vm = CreateVirtualMemory();
    ASSERT_TRUE(vm);

    const size_t pageSize = vm->PageSize();
    ASSERT_GT(pageSize, 0u);

    void* reserved = vm->Reserve(pageSize);
    ASSERT_NE(reserved, nullptr);
    EXPECT_TRUE(vm->Commit(reserved, pageSize));
    EXPECT_TRUE(vm->Protect(reserved, pageSize, IVirtualMemory::MemoryProtection::ReadWrite));
    EXPECT_TRUE(vm->Decommit(reserved, pageSize));
    vm->Release(reserved);
}

TEST(VirtualMemory, ReserveAndCommitReturnsWritableMemory)
{
    UniquePtr<IVirtualMemory> vm = CreateVirtualMemory();
    ASSERT_TRUE(vm);

    const size_t pageSize = vm->PageSize();
    void* memory = vm->ReserveAndCommit(pageSize);
    ASSERT_NE(memory, nullptr);

    auto* bytes = static_cast<unsigned char*>(memory);
    bytes[0] = 0x5Au;
    bytes[pageSize - 1] = 0xA5u;
    EXPECT_EQ(bytes[0], 0x5Au);
    EXPECT_EQ(bytes[pageSize - 1], 0xA5u);

    vm->Release(memory);
}

} // namespace Engine::Memory
