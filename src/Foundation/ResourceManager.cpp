#include "ResourceManager.hpp"

void ResourceManager::init(Allocator* alloc, ResourceFilenameResolver* resolver)
{
    allocator = alloc;
    filenameResolver = resolver;

    loaders.init(allocator, 8);
    compilers.init(allocator, 8);
}

void ResourceManager::shutdown() 
{
    loaders.shutdown();
    compilers.shutdown();
}

void ResourceManager::setLoader(const char* resourceType, ResourceLoader* loader) 
{
    const uint64_t hashedName = hashCalculate(resourceType);
    loaders.insert(hashedName, loader);
}

void ResourceManager::setCompiler(const char* resourceType, ResourceCompiler* compiler) 
{
    const uint64_t hashedName = hashCalculate(resourceType);
    compilers.insert(hashedName, compiler);
}