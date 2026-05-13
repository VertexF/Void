#include <Foundation/Memory/MemoryTag.hpp>

#include <gtest/gtest.h>

#include <cstddef>

namespace Engine::Memory {

TEST(MemoryTag, NamesMatchEnumValues)
{
    EXPECT_STREQ(GetMemoryTagName(MemoryTag::Default), "Default");
    EXPECT_STREQ(GetMemoryTagName(MemoryTag::Render), "Render");
    EXPECT_STREQ(GetMemoryTagName(MemoryTag::Physics), "Physics");
    EXPECT_STREQ(GetMemoryTagName(MemoryTag::Texture), "Texture");
    EXPECT_STREQ(GetMemoryTagName(MemoryTag::Temporary), "Temporary");
    EXPECT_STREQ(GetMemoryTagName(MemoryTag::Debug), "Debug");
}

TEST(MemoryTag, UnknownValuesReturnUnknown)
{
    const auto invalidTag = static_cast<MemoryTag>(static_cast<size_t>(MemoryTag::Count) + 8u);
    EXPECT_STREQ(GetMemoryTagName(invalidTag), "Unknown");
}

TEST(MemoryTag, CountRemainsAfterConcreteTags)
{
    EXPECT_GT(static_cast<size_t>(MemoryTag::Count), static_cast<size_t>(MemoryTag::Debug));
}

} // namespace Engine::Memory
