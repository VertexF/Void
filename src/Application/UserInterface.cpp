#include "UserInterface.hpp"

void GUI::init(Renderer2D& inRenderer2D)
{
	renderer2D = &inRenderer2D;

	renderer2D->loadTexture(ATLAS_TEST);
	width[ATLAS_TEST] = (float)renderer2D->width;
	height[ATLAS_TEST] = (float)renderer2D->height;
}

void GUI::buildMainMenu()
{
	float xCentre = (((float)Window::instance()->width) - width[ATLAS_TEST]) / 2;
	float yCentre = (((float)Window::instance()->height) - height[ATLAS_TEST]) / 2;

	vec2s spriteOffset = { .x = 1, .y = 1 };
	vec2s buttonSize = { width[ATLAS_TEST], 128.f };
	vec2s subSpriteSize = { width[ATLAS_TEST], 128.f };

	renderer2D->addQuad({ xCentre, yCentre - 40.f, 0.5f }, buttonSize, subSpriteSize, { 0, 3 }, spriteOffset, ATLAS_TEST);
	addButton({ xCentre, yCentre + 100.f, 0.5f }, buttonSize, subSpriteSize, { 0, 2 }, spriteOffset, ATLAS_TEST, START_BUTTON);
	addButton({ xCentre, yCentre + 240.f, 0.5f }, buttonSize, subSpriteSize, { 0, 1 }, spriteOffset, ATLAS_TEST, EXIT_BUTTON);
}

void GUI::buildGameUI()
{
}

void GUI::buildUI()
{
	buildMainMenu();
	buildGameUI();
}

void GUI::addButton(vec3s position, vec2s size, vec2s spriteSize, vec2s rowAndColumn, vec2s offset, TextureAtlas atlas, UI uiElement)
{
	renderer2D->addQuad(position, size, spriteSize, rowAndColumn, offset, atlas);
	sUIRectangles[uiElement].x = position.x;
	sUIRectangles[uiElement].y = position.y;
	sUIRectangles[uiElement].width = size.x;
	sUIRectangles[uiElement].height = size.y;
}

void GUI::resizeUI(float ratioIncreaseWidth, float ratioIncreaseHeight)
{
	for (uint32_t i = 0; i < UI_COUNT; ++i)
	{
		sUIRectangles[(UI)i].x *= ratioIncreaseWidth;
		sUIRectangles[(UI)i].y *= ratioIncreaseHeight;
		sUIRectangles[(UI)i].width *= ratioIncreaseWidth;
		sUIRectangles[(UI)i].height *= ratioIncreaseHeight;
	}
}