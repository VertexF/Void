#include "ResourcePool.hpp"

#include <string.h>

static constexpr uint32_t INVALID_INDEX = ~0u;

void ResourcePool::init(Allocator* alloc, uint32_t inNewPoolSize, uint32_t inResourceSize)
{
    allocator = alloc;
    poolSize = inNewPoolSize;
    resourceSize = inResourceSize;

    //Lets groups these together in a sizes of resource + uint32_t
    size_t allocationSize = poolSize * (resourceSize + sizeof(uint32_t));
    memory = (uint8_t*)allocator->allocate(allocationSize, 1);
    memset(memory, 0, allocationSize);

    //Allocate and add free indices
    freeIndices = (uint32_t*)(memory + poolSize * resourceSize);
    freeIndicesHead = 0;

    for (uint32_t i = 0; i < poolSize; ++i) 
    {
        freeIndices[i] = i;
    }

    usedIndices = 0;
}

void ResourcePool::shutdown() const
{
    if (freeIndicesHead != 0) 
    {
        vprint("Resource pool has unfreed resources.\n");

        for (uint32_t i = 0; i < freeIndicesHead; ++i) 
        {
            vprint("\tResource %u\n", freeIndices[i]);
        }
    }

    VOID_ASSERT(usedIndices == 0);

    allocator->deallocate(memory);
}

uint32_t ResourcePool::obtainResource() 
{
    //TODO: Try to add bits for checking if resources are alice and use bitmasking to figure that out.
    if (freeIndicesHead < poolSize) 
    {
        const uint32_t freeIndex = freeIndices[freeIndicesHead++];
        ++usedIndices;
        return freeIndex;
    }

    VOID_ASSERTM(false, "No more resources");
    return INVALID_INDEX;
}

void ResourcePool::releaseResource(uint32_t index) 
{
    freeIndices[--freeIndicesHead] = index;
    --usedIndices;
}

void ResourcePool::freeAllResources() 
{
    freeIndicesHead = 0;
    usedIndices = 0;

    for (uint32_t i = 0; i < poolSize; ++i) 
    {
        freeIndices[i] = i;
    }
}

void* ResourcePool::accessResource(uint32_t index) 
{
    if (index != INVALID_INDEX)
    {
        return &memory[index * resourceSize];
    }

    return nullptr;
}

const void* ResourcePool::accessResource(uint32_t index) const 
{
    if (index != INVALID_INDEX)
    {
        return &memory[index * resourceSize];
    }

    return nullptr;
}