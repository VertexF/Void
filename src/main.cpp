#include "Graphics/VulkanRenderer.hpp"

int main() 
{
	VulkanRenderer* renderer = new VulkanRenderer{};

	runGame(renderer);

	delete renderer;
}