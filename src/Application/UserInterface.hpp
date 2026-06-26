#ifndef USER_INTERFACE_HDR
#define USER_INTERFACE_HDR

#include "Graphics/2DRenderer.hpp"

enum UI
{
	EXIT_BUTTON,
	START_BUTTON,

	UI_COUNT
};

//Note these are stored in screen space.
struct Rect 
{
	float x;
	float y;
	float width;
	float height;
};

inline Rect sUIRectangles[50];

static bool isUIPressed(UI button, const InputHandler& handler)
{
	float pointX;
	float pointY;
	SDL_GetMouseState(&pointX, &pointY);

	if (pointX > sUIRectangles[button].x && pointX < sUIRectangles[button].x + sUIRectangles[button].width
		&& pointY > sUIRectangles[button].y && pointY < sUIRectangles[button].y + sUIRectangles[button].height)
	{
		if (handler.isMouseClicked(MOUSE_BUTTON_LEFT))
		{
			return true;
		}

		return false;
	}

	return false;
}

struct GUI
{
	void init(Renderer2D& inRenderer2D);

	void buildMainMenu();
	void buildGameUI();

	void buildUI();

	void addButton(vec3s position, vec2s size, vec2s spriteSize, vec2s rowAndColumn, vec2s offset, TextureAtlas atlas, UI uiElement);

	void resizeUI(float ratioIncreaseWidth, float ratioIncreaseHeight);

	Renderer2D* renderer2D;

	float width[ATLAS_COUNT];
	float height[ATLAS_COUNT];

	float windowHieght = 0;
	float windowWidth = 0;
};

#endif // !USER_INTERFACE_HDR