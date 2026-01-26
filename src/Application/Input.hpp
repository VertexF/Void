#ifndef INPUT_HDR
#define INPUT_HDR

#include "Foundation/Array.hpp"
#include "Foundation/String.hpp"

#include "Application/Keys.hpp"


static constexpr uint32_t MAX_GAMEPADS = 4;

struct SDL_Gamepad;
struct GPUDevice;
struct Allocator;

enum GamepadAxis : uint8_t 
{
    GAMEPAD_AXIS_LEFTX = 0,
    GAMEPAD_AXIS_LEFTY,
    GAMEPAD_AXIS_RIGHTX,
    GAMEPAD_AXIS_RIGHTY,
    GAMEPAD_AXIS_TRIGGERLEFT,
    GAMEPAD_AXIS_TRIGGERRIGHT,
    GAMEPAD_AXIS_COUNT
};

enum GamepadButtons : uint8_t
{
    GAMEPAD_BUTTON_A = 0,
    GAMEPAD_BUTTON_B,
    GAMEPAD_BUTTON_X,
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_GUIDE,
    GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_LEFTSTICK,
    GAMEPAD_BUTTON_RIGHTSTICK,
    GAMEPAD_BUTTON_LEFTSHOULDER,
    GAMEPAD_BUTTON_RIGHTSHOULDER,
    GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_COUNT
};

enum MouseButtons : int8_t
{
    MOUSE_BUTTON_NONE = -1,
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT, 
    MOUSE_BUTTON_MIDDLE, 
    MOUSE_BUTTON_COUNT
};

enum Device : uint8_t 
{
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    DEVICE_GAMEPAD
};

enum DevicePart : uint8_t 
{
    DEVICE_PART_KEYBOARD,
    DEVICE_PART_MOUSE,
    DEVICE_PART_GAMEPAD_AXIS,
    DEVICE_PART_GAMEPAD_BUTTONS
};

enum BindingType : uint8_t 
{
    BINDING_TYPE_BUTTON,
    BINDING_TYPE_AXIS_1D,
    BINDING_TYPE_AXIS_2D,
    BINDING_TYPE_VECTOR_1D,
    BINDING_TYPE_VECTOR_2D,
    BINDING_TYPE_BUTTON_ONE_MOD,
    BINDING_TYPE_BUTTON_TWO_MOD,
};

//Device utility functions
Device deviceFromPart(DevicePart part);

struct InputVector2 
{
    float x = 0.f;
    float y = 0.f;
};

struct Gamepad 
{
    float axis[GAMEPAD_AXIS_COUNT]{0.f};

    SDL_Gamepad* handle = nullptr;
    const char* name = nullptr;

    uint32_t id = UINT32_MAX;
    uint8_t buttons[GAMEPAD_BUTTON_COUNT]{ 0 };
    uint8_t previousButtons[GAMEPAD_BUTTON_COUNT]{ 0 };

    bool isAttached() const;
    bool isButtonDown(GamepadButtons button) const;
    bool isButtonJustPressed(GamepadButtons button) const;
    bool isButtonJustReleased(GamepadButtons key) const;
};

struct InputBinding 
{
    float value = 0.0f;

    float minDeadzone = 0.1f;
    float maxDeadzone = 0.95f;

    uint16_t actionIndex;
    uint16_t button;

    uint8_t actionMapIndex;
    uint8_t isComposite;
    uint8_t isPartOfComposite;
    uint8_t repeat;

    BindingType type;
    Device device;
    DevicePart devicePart;

    InputBinding& set(BindingType inType, Device inDevice, DevicePart inDevicePart, uint16_t inButton,
                      uint8_t inIsComposite, uint8_t inIsPartOfComposite, uint8_t inRepeat);
    InputBinding& setDeadzones(float min, float max);
    InputBinding& setHandles(uint32_t actionMap, uint32_t action);
};

struct InputAction 
{
    bool triggered() const;
    float readValue1D() const;
    InputVector2 readValue2D() const;

    InputVector2 value;
    uint32_t actionMap;
    const char* name;
};

struct InputActionMap 
{
    const char* name;
    bool active;
};

struct InputActionMapCreation 
{
    const char* name;
    bool active;
};

struct InputActionCreation 
{
    const char* name;
    uint32_t actionMap;
};

struct InputHandler
{
    void init(Allocator* allocator);
    void shutdown();

    bool isButtonDown(GamepadButtons button) const;
    bool isButtonJustDown(GamepadButtons button) const;
    bool isButtonJustReleased(GamepadButtons button) const;

    bool isKeyDown(Keys key) const;
    bool isKeyJustPressed(Keys key, bool repeat = false) const;
    bool isKeyJustReleased(Keys key) const;

    bool isMouseDown(MouseButtons button) const;
    bool isMouseClicked(MouseButtons button) const;
    bool isMouseReleased(MouseButtons button) const;
    bool isMouseDragging(MouseButtons button) const;

    void update();

#if defined(VOID_IMGUI)
    void debugUI();
#endif //VOID_IMGUI

    void newFrame();
    void onEvent(GPUDevice* gpu);

    bool isTriggered(uint32_t action) const;
    float isReadValue1D(uint32_t action) const;
    InputVector2 isReadValue2D(uint32_t action) const;

    //Create methods used to create the action input
    uint32_t createActionMap(const InputActionMapCreation& creation);
    uint32_t createAction(const InputActionCreation& creation);

    //Find methods using names
    uint32_t findActionMap(const char* name) const;
    uint32_t findAction(const char* name) const;

    void addButton(uint32_t action, DevicePart device, uint16_t button, bool repeat = false);
    void addAxis1D(uint32_t action, DevicePart device, uint16_t axis, float minDeadzone, float maxDeadzone);
    void addAxis2D(uint32_t action, DevicePart device, uint16_t xAxis, uint16_t yAxis, float minDeadzone, float maxDeadzone);
    void addVector1D(uint32_t action, DevicePart devicePos, uint16_t buttonPos,
                                      DevicePart deviceNeg, uint16_t buttonNeg, 
                                      bool repeat = true);
    void addVector2D(uint32_t action, DevicePart deviceUp, uint16_t buttonUp,
                                      DevicePart deviceDown, uint16_t buttonDown,
                                      DevicePart deviceLeft, uint16_t buttonLeft,
                                      DevicePart deviceRight, uint16_t buttonRight,
                                      bool repeat = true);

    uint8_t keys[KEY_COUNT]{ 0 };
    uint8_t previousKeys[KEY_COUNT]{ 0 };

    Gamepad gamepads[MAX_GAMEPADS]{};

    InputVector2 mouseClickedPosition[MOUSE_BUTTON_COUNT]{};
    float mouseDragDistance[MOUSE_BUTTON_COUNT]{ 0.f };
    uint8_t mouseButton[MOUSE_BUTTON_COUNT]{ 0 };
    uint8_t previousMouseButton[MOUSE_BUTTON_COUNT]{ 0 };

    StringBuffer stringBuffer;
    Array<InputActionMap> actionMaps;
    Array<InputAction> actions;
    Array<InputBinding> bindings;

    InputVector2 mousePosition{};
    InputVector2 previousMousePosition{};

    uint32_t numOfConnectedGamepads{0};

    bool hasFocus{false};
};

#endif // !INPUT_HDR
