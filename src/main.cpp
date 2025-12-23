#include "Foundation/Memory.hpp"
#include "Graphics/VulkanRenderer.hpp"

int main() 
{
	MemoryServiceConfiguration memoryConfiguration;
	memoryConfiguration.maximumDynamicSize = void_giga(1ull);

	MemoryService::instance()->init(&memoryConfiguration);
	MemoryService::instance()->scratchAllocator.init(void_mega(8));
	HeapAllocator* allocator = &MemoryService::instance()->systemAllocator;

	VulkanRenderer* renderer = (VulkanRenderer*)void_alloca(sizeof(VulkanRenderer), allocator);

	runGame(renderer);

	void_free(renderer, allocator);

	MemoryService::instance()->shutdown();
}