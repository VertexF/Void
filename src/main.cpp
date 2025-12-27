#include "Foundation/Memory.hpp"
#include "Graphics/VulkanRenderer.hpp"

int main() 
{
	MemoryService::instance()->init(void_giga(1ull), void_mega(8));

	VulkanRenderer* renderer = (VulkanRenderer*)void_alloca(sizeof(VulkanRenderer), &MemoryService::instance()->systemAllocator);

	runGame(renderer);

	void_free(renderer, &MemoryService::instance()->systemAllocator);

	MemoryService::instance()->shutdown();
}