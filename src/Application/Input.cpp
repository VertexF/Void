#include "Input.hpp"

#include "Window.hpp"

#include "Foundation/Assert.hpp"
#include "Foundation/Numerics.hpp"

#include <cmath>

#include <SDL3/SDL.h>

static bool initGamepad(SDL_JoystickID joystickID, Gamepad& gamepad)
{
    SDL_Gamepad* pad = SDL_OpenGamepad(joystickID);
    SDL_Joystick* joy = SDL_OpenJoystick(joystickID);

    if (pad) 
    {
        vprint("Open joystick\n");
        vprint("Name: %s\n", SDL_GetGamepadNameForID(joystickID));
        vprint("Numers of Axes: %d\n", SDL_GetNumJoystickAxes(joy));
        vprint("Number of Buttons: %d\n", SDL_GetNumJoystickButtons(joy));

        gamepad.id = joystickID;
        gamepad.name = SDL_GetGamepadNameForID(joystickID);
        gamepad.handle = pad;

        return true;
    }
    else 
    {
        vprint("Couldn't open Joystick %u\n", joystickID);
        gamepad.id = UINT32_MAX;

        return false;
    }
}

//static void terminateGamepad(Gamepad& gamepad) 
//{
//    SDL_CloseJoystick((SDL_Joystick*)gamepad.handle);
//    gamepad.name = nullptr;
//    gamepad.handle = nullptr;
//    gamepad.id = UINT32_MAX;
//}

static uint32_t toSDLMouseButton(MouseButtons button) 
{
    switch (button) 
    {
    case MOUSE_BUTTON_LEFT:
        return SDL_BUTTON_LEFT;
    case MOUSE_BUTTON_MIDDLE:
        return SDL_BUTTON_MIDDLE;
    case MOUSE_BUTTON_RIGHT:
        return SDL_BUTTON_RIGHT;
    case MOUSE_BUTTON_COUNT:
    case MOUSE_BUTTON_NONE:
    default:
        return UINT32_MAX;
    }
}

//static const char** gamepadAxisNames() 
//{
//    static const char* names[] = {"left_x", "left_y", "right_x", "right_y", 
//                                    "trigger_left", "trigger_right", "gamepad_axis_count"};
//    return names;
//}
//
//static const char** gamepadButtonNames() 
//{
//    static const char* names[] = {"a", "b", "x", "y", "back", "guide", "start", 
//                                    "left_stick", "right_stick", "left_shoulder", "right_shoulder", 
//                                    "dpad_up", "dpad_down", "dpad_left", "dpad_right", "gamepad_button_count"};
//    return names;
//}
//
//static const char** mouseButtonNames() 
//{
//    static const char* names[] = {"left", "right", "middle", "mouse_button_count"};
//    return names;
//}

constexpr float MOUSE_DRAG_MIN_DISTANCE = 4.f;


struct InputBackend 
{
    void init(Gamepad* gamepads, uint32_t& maxGamepads);

    void getMouseState(InputVector2& position, uint8_t* buttons, uint32_t numButtons);
    void onEvent(uint8_t* keys, uint32_t numKeys, 
                 Gamepad* gamepads, uint32_t numGamepads, bool& hasFocus);
};

void InputBackend::init(Gamepad* gamepads, uint32_t &numGamepads)
{
    if (SDL_WasInit(SDL_INIT_GAMEPAD) != 1) 
    {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    }

    int32_t numJoysticks;
    SDL_JoystickID* joystickArray = SDL_GetJoysticks(&numJoysticks);
    numGamepads = numJoysticks;
    if (numJoysticks > 0) 
    {
        vprint("Detected joysticks!\n");

        for (int32_t i = 0; i < numJoysticks; ++i) 
        {
            if (SDL_IsGamepad(joystickArray[i]))
            {
                initGamepad(joystickArray[i], gamepads[i]);
            }
        }
    }
}

void InputBackend::getMouseState(InputVector2& position, uint8_t* buttons, uint32_t numButtons) 
{
    float mouseX = 0.f;
    float mouseY = 0.f;
    uint32_t mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

    for (uint32_t i = 0; i < numButtons; ++i) 
    {
        uint32_t sdlButton = toSDLMouseButton(static_cast<MouseButtons>(i));
        buttons[i] = mouseButtons & SDL_BUTTON_MASK(sdlButton);
    }

    position.x = mouseX;
    position.y = mouseY;
}

void InputBackend::onEvent(uint8_t* keys, uint32_t numKeys,
                            Gamepad* gamepads, uint32_t /*numGamepads*/, bool& hasFocus) 
{
    SDL_Event events;
    while (SDL_PollEvent(&events))
    {
        switch (events.type)
        {
        case SDL_EVENT_QUIT:
        {
            Window::instance()->exitRequested = true;
            break;
        }
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: 
        {
            Window::instance()->exitRequested = true;
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            int32_t key = events.key.scancode;
            if (key >= 0 && key < static_cast<int32_t>(numKeys))
            {
                keys[key] = (events.type == SDL_EVENT_KEY_DOWN);
            }
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
        {
            int newWidth{0};
            int newHeight{0};
            SDL_GetWindowSizeInPixels(Window::instance()->platformHandle, &newWidth, &newHeight);

            //Update only if needed.
            if (uint32_t(newWidth) != Window::instance()->width || uint32_t(newHeight) != Window::instance()->height)
            {
                Window::instance()->resizeRequested = true;
                Window::instance()->width = uint32_t(newWidth);
                Window::instance()->height = uint32_t(newHeight);

                vprint("Resize to %u, %u\n", Window::instance()->width, Window::instance()->height);
            }
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            vprint("Focus gained\n");
            hasFocus = true;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            vprint("Focus Lost\n");
            hasFocus = false;
            break;
        case SDL_EVENT_WINDOW_MAXIMIZED:
            vprint("Maximised\n");
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            vprint("Minimised\n");
            Window::instance()->minimisedRequested = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            vprint("Restored\n");
            Window::instance()->minimisedRequested = false;
            break;
        case SDL_EVENT_WINDOW_EXPOSED:
            vprint("Exposed\n");
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
        {
            vprint("Gamepad Added\n");
            int32_t index = events.cdevice.which;

            initGamepad(index, gamepads[index]);

            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
#if defined (INPUT_DEBUG_OUTPUT)
            vprint("Axis %u - %f\n", events.jaxis.axis, events.jaxis.value / 32768.f);
#endif //INPUT_DEBUG_OUTPUT

            for (size_t i = 0; i < MAX_GAMEPADS; ++i)
            {
                if (gamepads[i].id == events.gaxis.which)
                {
                    gamepads[i].axis[events.gaxis.axis] = events.gaxis.value / 32768.f;
                    break;
                }
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        {
#if defined(INPUT_DEBUG_OUTPUT)
            vprint("Button\n");
#endif //INPUT_DEBUG_OUTPUT

            for (size_t i = 0; i < MAX_GAMEPADS; ++i)
            {
                if (gamepads[i].id == events.gbutton.which)
                {
                    gamepads[i].buttons[events.gbutton.button] = events.gbutton.down == true ? 1 : 0;
                    break;
                }
            }
            break;
        }
        }
    }
}

InputBinding& InputBinding::set(BindingType inType, Device inDevice, DevicePart inDevicePart, uint16_t inButton,
                                uint8_t inIsComposite, uint8_t inIsPartOfComposite, uint8_t inRepeat) 
{
    type = inType;
    device = inDevice;
    devicePart = inDevicePart;
    button = inButton;
    isComposite = inIsComposite;
    isPartOfComposite = inIsPartOfComposite;
    repeat = inRepeat;

    return *this;
}

InputBinding& InputBinding::setDeadzones(float min, float max) 
{
    minDeadzone = min;
    maxDeadzone = max;

    return *this;
}

InputBinding& InputBinding::setHandles(uint32_t actionMap, uint32_t action)
{
    VOID_ASSERTM(actionMap < 256, "Action map need to be greater than 256");
    VOID_ASSERTM(action < 16636, "Action need to be greater than 16636");

    actionMapIndex = static_cast<uint8_t>(actionMap);
    actionIndex = static_cast<uint16_t>(action);

    return *this;
}

bool InputAction::triggered() const 
{
    return value.x != 0.f;
}

float InputAction::readValue1D() const 
{
    return value.x;
}

InputVector2 InputAction::readValue2D() const 
{
    return value;
}

//Device utility functions
Device deviceFromPart(DevicePart part) 
{
    switch (part) 
    {
    case DEVICE_PART_MOUSE:
    {
        return DEVICE_MOUSE;
    }

    case DEVICE_PART_GAMEPAD_AXIS:
    case DEVICE_PART_GAMEPAD_BUTTONS:
    {
        return DEVICE_GAMEPAD;
    }

    case DEVICE_PART_KEYBOARD:
    default: 
    {
        return DEVICE_KEYBOARD;
    }
    }
}

bool Gamepad::isAttached() const 
{
    return id >= 0; 
}

bool Gamepad::isButtonDown(GamepadButtons button) const 
{ 
    return buttons[button]; 
}

bool Gamepad::isButtonJustPressed(GamepadButtons button) const 
{ 
    return (buttons[button] && !previousButtons[button]); 
}

bool Gamepad::isButtonJustReleased(GamepadButtons key) const
{
    return !buttons[key] && previousButtons[key];
}

static InputBackend INPUT_BACKEND;

void InputHandler::init(Allocator* allocator)
{
    vprint("InputHandler init\n");

    stringBuffer.init(1000, allocator);
    actionMaps.init(allocator, 16);
    actions.init(allocator, 64);
    bindings.init(allocator, 256);

    INPUT_BACKEND.init(gamepads, numOfConnectedGamepads);
}

void InputHandler::shutdown() 
{
    actionMaps.shutdown();
    actions.shutdown();
    bindings.shutdown();

    stringBuffer.shutdown();

    vprint("Input Service shutting down\n");
}

bool InputHandler::isButtonDown(GamepadButtons button) const
{
    for (uint32_t i = 0; i < numOfConnectedGamepads; ++i)
    {
        return gamepads[i].isButtonDown(button) && hasFocus;
    }
    return false;
}

bool InputHandler::isButtonJustDown(GamepadButtons button) const
{
    for (uint32_t i = 0; i < numOfConnectedGamepads; ++i)
    {
        return gamepads[i].isButtonJustPressed(button) && hasFocus;
    }
    return false;
}

bool InputHandler::isButtonJustReleased(GamepadButtons button) const
{
    for (uint32_t i = 0; i < numOfConnectedGamepads; ++i)
    {
        return gamepads[i].isButtonJustReleased(button) && hasFocus;
    }
    return false;
}

bool InputHandler::isKeyDown(Keys key) const 
{
    return keys[key] && hasFocus;
}

bool InputHandler::isKeyJustPressed(Keys key, bool /*repeat*//* = false*/) const
{
    return keys[key] && !previousKeys[key] && hasFocus;
}

bool InputHandler::isKeyJustReleased(Keys key) const
{
    return !keys[key] && previousKeys[key] && hasFocus;
}

bool InputHandler::isMouseDown(MouseButtons button) const 
{
    return mouseButton[button];
}

bool InputHandler::isMouseClicked(MouseButtons button) const
{
    return mouseButton[button] && !previousMouseButton[button];
}

bool InputHandler::isMouseReleased(MouseButtons button) const
{
    return !mouseButton[button];
}

bool InputHandler::isMouseDragging(MouseButtons button) const
{
    if (!mouseButton[button])
    {
        return false;
    }

    return mouseDragDistance[button] > MOUSE_DRAG_MIN_DISTANCE;
}

void InputHandler::update() 
{
    //Update mouse
    previousMousePosition = mousePosition;
    //Update current mouse state
    INPUT_BACKEND.getMouseState(mousePosition, mouseButton, MOUSE_BUTTON_COUNT);

    for (uint32_t i = 0; i < MOUSE_BUTTON_COUNT; ++i) 
    {
        //Just clicked something save that position.
        if (isMouseClicked(static_cast<MouseButtons>(i))) 
        {
            mouseClickedPosition[i] = mousePosition;
        }
        else if (isMouseDown(static_cast<MouseButtons>(i))) 
        {
            float deltaX = mousePosition.x - mouseClickedPosition[i].x;
            float deltaY = mousePosition.y - mouseClickedPosition[i].y;
            mouseDragDistance[i] = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
        }
    }

    //Everything below here handles bindings for non-standard key set ups.
    //Update all the action maps. Update all the actions. Scan each action of the map
    for (uint32_t j = 0; j < actions.size; ++j) 
    {
        actions[j].value = {0, 0};
    }

    //Read all input values for each binding
    //First get all the button or composite parts. Composite input will be calculated after.
    for (uint32_t k = 0; k < bindings.size; ++k)
    {
        //Skip composite binding. Their value will be calculated after.
        if (bindings[k].isComposite)
        {
            continue;
        }

        bindings[k].value = 0.f;

        switch (bindings[k].device)
        {
            case DEVICE_KEYBOARD:
            {
                bool keyValue = bindings[k].repeat ? isKeyDown(static_cast<Keys>(bindings[k].button)) :
                                                      isKeyJustPressed(static_cast<Keys>(bindings[k].button, false));
                bindings[k].value = keyValue ? 1.f : 0.f;
                break;
            }

            case DEVICE_GAMEPAD:
            {
                Gamepad gamepad = gamepads[0];
                if (gamepad.handle == nullptr)
                {
                    break;
                }

                const float minDeadzone = bindings[k].minDeadzone;
                const float maxDeadzone = bindings[k].maxDeadzone;

                switch (bindings[k].devicePart)
                {
                case DEVICE_PART_GAMEPAD_AXIS:
                    bindings[k].value = gamepad.axis[bindings[k].button];
                    bindings[k].value = std::fabs(bindings[k].value) < minDeadzone ? 0.f : bindings[k].value;
                    bindings[k].value = std::fabs(bindings[k].value) > maxDeadzone ? (bindings[k].value < 0 ? -1.f : 1.f) : bindings[k].value;
                    break;

                case DEVICE_PART_GAMEPAD_BUTTONS:
                    bindings[k].value = bindings[k].repeat ? gamepad.isButtonDown(static_cast<GamepadButtons>(bindings[k].button)) :
                                                             gamepad.isButtonJustPressed(static_cast<GamepadButtons>(bindings[k].button));
                    break;
                case DEVICE_PART_KEYBOARD:
                    VOID_ERROR("Not implemented!\n");
                    break;
                case DEVICE_PART_MOUSE:
                    VOID_ERROR("Not implemented!\n");
                    break;
                }
            }
        }
    }

    for (uint32_t k = 0; k < bindings.size; ++k) 
    {
        if (bindings[k].isPartOfComposite)
        {
            continue;
        }

        InputAction inputAction = actions[bindings[k].actionIndex];

        switch (bindings[k].type)
        {
        case BINDING_TYPE_BUTTON:
            inputAction.value.x = max(inputAction.value.x, bindings[k].value ? 1.f : 0.f);
            break;
        case BINDING_TYPE_AXIS_1D:
            inputAction.value.x = bindings[k].value != 0.f ? bindings[k].value : inputAction.value.x;
            break;
        case BINDING_TYPE_AXIS_2D:
        {
            //Notice we are using a non-const reference to the next 2 key values.
            InputBinding& inputBindingX = bindings[++k];
            InputBinding& inputBindingY = bindings[++k];

            //The actually key gets updated here.
            inputAction.value.x = inputBindingX.value != 0.f ? inputBindingX.value : inputAction.value.x;
            inputAction.value.y = inputBindingY.value != 0.f ? inputBindingY.value : inputAction.value.y;
            break;
        }
        case BINDING_TYPE_VECTOR_1D:
        {
            //Notice we are using a non-const reference to the next 2 key value
            InputBinding& inputBindingPos = bindings[++k];
            InputBinding& inputBindingNeg = bindings[++k];

            //The actually key gets updated here.
            inputAction.value.x = inputBindingPos.value ?  inputBindingPos.value : 
                                  inputBindingNeg.value ? -inputBindingNeg.value : 
                                  inputAction.value.x;

            break;
        }
        case BINDING_TYPE_VECTOR_2D: 
        {
            //Notice we are using a non-const reference to the next 4 key values.
            InputBinding& inputBindingUp = bindings[++k];
            InputBinding& inputBindingDown = bindings[++k];
            InputBinding& inputBindingLeft = bindings[++k];
            InputBinding& inputBindingRight = bindings[++k];

            //The actually key gets updated here.
            inputAction.value.x = inputBindingRight.value ? 1.f : inputBindingLeft.value ? -1.f : inputAction.value.x;
            inputAction.value.y = inputBindingUp.value ? 1.f : inputBindingDown.value ? -1.f : inputAction.value.y;

            break;
        }
        case BINDING_TYPE_BUTTON_ONE_MOD:
            VOID_ERROR("Not implemented!\n");
            break;
        case BINDING_TYPE_BUTTON_TWO_MOD:
            VOID_ERROR("Not implemented!\n");
            break;
        }
    }
}

#if defined(VOID_IMGUI)
void InputHandler::debugUI() 
{
    if (ImGui::Begin("Inputs"))
    {
        ImGui::Text("Has focus %u", hasFocus ? 1 : 0);

        if (ImGui::TreeNode("Device"))
        {
            ImGui::Separator();
            if (ImGui::TreeNode("Gamepads"))
            {
                for (uint32_t i = 0; i < MAX_GAMEPADS; ++i) 
                {
                    const Gamepad& pad = gamepads[i];
                    ImGui::Text("Name: %s, id %d index %u", pad.name, pad.id, pad.index);

                    //Attach gamepad
                    if (pad.isAttached()) 
                    {
                        //Axes
                        ImGui::NewLine();
                        ImGui::Columns(GAMEPAD_AXIS_COUNT);
                        for (uint32_t gamepadIndex = 0; gamepadIndex < GAMEPAD_AXIS_COUNT; ++gamepadIndex)
                        {
                            ImGui::Text("%s", gamepadAxisNames()[gamepadIndex]);
                            ImGui::NextColumn();
                        }

                        for (uint32_t gamepadIndex = 0; gamepadIndex < GAMEPAD_AXIS_COUNT; ++gamepadIndex)
                        {
                            ImGui::Text("%f", pad.axis[gamepadIndex]);
                            ImGui::NextColumn();
                        }

                        //Buttons
                        ImGui::NewLine();
                        ImGui::Columns(GAMEPAD_BUTTON_COUNT);
                        for (uint32_t gamepadIndex = 0; gamepadIndex < GAMEPAD_BUTTON_COUNT; ++gamepadIndex) 
                        {
                            ImGui::Text("%s", gamepadButtonNames()[gamepadIndex]);
                            ImGui::NextColumn();
                        }

                        ImGui::Columns(GAMEPAD_BUTTON_COUNT);

                        for (uint32_t gamepadIndex = 0; gamepadIndex < GAMEPAD_BUTTON_COUNT; ++gamepadIndex)
                        {
                            ImGui::Text("%u", pad.buttons[gamepadIndex]);
                            ImGui::NextColumn();
                        }

                        ImGui::Columns(1);
                    }
                    ImGui::Separator();
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            if (ImGui::TreeNode("Mouse"))
            {
                ImGui::Text("Position      %f, %f", mousePosition.x, mousePosition.y);
                ImGui::Text("Previous pos  %f, %f", previousMousePosition.x, previousMousePosition.y);

                ImGui::Separator();

                for (uint32_t i = 0; i < MOUSE_BUTTON_COUNT; ++i) 
                {
                    ImGui::Text("Button %u", i);
                    ImGui::SameLine();
                    ImGui::Text("Click Position      %4.1f, %4.1f", mouseClickedPosition[i].x, mouseClickedPosition[i].y);
                    ImGui::SameLine();
                    ImGui::Text("Button %u, Previous %u", mouseButton[i], previousMouseButton[i]);
                    ImGui::SameLine();
                    ImGui::Text("Drag %f", mouseDragDistance[i]);

                    ImGui::Separator();
                }
                ImGui::TreePop();
            }

            ImGui::Separator();
            if (ImGui::TreeNode("Keyboard"))
            {
                //TODO: Add the keys in if you need to test keyboard key presses.
                //for (uint32_t i = 0; i < KEY_LAST; ++i) 
                //{
                //    
                //}
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Actions"))
        {
            for (uint32_t j = 0; j < actions.size; ++j) 
            {
                const InputAction& inputAction = actions[j];
                ImGui::Text("Action %s, x %2.3f y %2.3f", inputAction.name, inputAction.value.x, inputAction.value.y);
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Bindings"))
        {
            for (uint32_t k = 0; k < bindings.size; ++k) 
            {
                const InputBinding& binding = bindings[k];
                const InputAction& parentAction = actions[binding.actionIndex];

                const char* buttonName = "";
                switch (binding.devicePart) 
                {
                case DEVICE_PART_KEYBOARD:
                    buttonName = keyNames()[binding.button];
                    break;
                case DEVICE_PART_MOUSE:
                    break;
                case DEVICE_PART_GAMEPAD_AXIS:
                    break;
                case DEVICE_PART_GAMEPAD_BUTTONS:
                    break;
                }

                switch (binding.type)
                {
                case BINDING_TYPE_VECTOR_1D:
                    ImGui::Text("Binding action %s, type %s, value %f, composite %u, part of the composite %u, button %s",
                                parentAction.name, "Vector 1D", binding.value, binding.isComposite, binding.isPartOfComposite, buttonName);
                    break;
                case BINDING_TYPE_VECTOR_2D:
                    ImGui::Text("Binding action %s, type %s, value %f, composite %u, part of composite %u",
                                parentAction.name, "Vector 2D", binding.value, binding.isComposite, binding.isPartOfComposite);
                    break;
                case BINDING_TYPE_AXIS_1D:
                    ImGui::Text("Binding action %s, type %s, value %f, composite %u, part of composite %u",
                                parentAction.name, "Axis 1D", binding.value, binding.isComposite, binding.isPartOfComposite);
                    break;
                case BINDING_TYPE_AXIS_2D:
                    ImGui::Text("Binding action %s, type %s, value %f, composite %u, part of composite %u",
                                parentAction.name, "Axis 2D", binding.value, binding.isComposite, binding.isPartOfComposite);
                    break;
                case BINDING_TYPE_BUTTON:
                    ImGui::Text("Binding action %s, type %s, value %f, composite %u, part of the composite %u, button %s",
                                parentAction.name, "Vector 1D", binding.value, binding.isComposite, binding.isPartOfComposite, buttonName);
                    break;
                }
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
}
#endif //VOID_IMGUI

void InputHandler::newFrame() 
{
    //Cache preview frame keys.
    //Resetting previous frame breaks key pressing - there can be more frames between key presses event if continuously pressed.
    for (uint32_t i = 0; i < KEY_COUNT; ++i)
    {
        previousKeys[i] = keys[i];
    }

    for (uint32_t i = 0; i < MOUSE_BUTTON_COUNT; ++i) 
    {
        previousMouseButton[i] = mouseButton[i];
    }

    for (uint32_t i = 0; i < numOfConnectedGamepads; ++i) 
    {
        if (gamepads[i].isAttached())
        {
            for (uint32_t k = 0; k < GAMEPAD_BUTTON_COUNT; k++) 
            {
                gamepads[i].previousButtons[k] = gamepads[i].buttons[k];
            }
        }
    }
}

void InputHandler::onEvent() 
{
    INPUT_BACKEND.onEvent(keys, KEY_COUNT, gamepads, MAX_GAMEPADS, hasFocus);
}

bool InputHandler::isTriggered(uint32_t action) const 
{
    VOID_ASSERT(action < actions.size);

    return actions[action].triggered();
}

float InputHandler::isReadValue1D(uint32_t action) const 
{
    VOID_ASSERT(action < actions.size);

    return actions[action].readValue1D();
}

InputVector2 InputHandler::isReadValue2D(uint32_t action) const 
{
    VOID_ASSERT(action < actions.size);

    return actions[action].readValue2D();
}

//Create methods used to create the action input
uint32_t InputHandler::createActionMap(const InputActionMapCreation& creation) 
{
    InputActionMap newActionMap{};
    newActionMap.active = creation.active;
    newActionMap.name = creation.name;

    actionMaps.push(newActionMap);

    return actionMaps.size - 1;
}

uint32_t InputHandler::createAction(const InputActionCreation& creation) 
{
    InputAction action{};
    action.actionMap = creation.actionMap;
    action.name = creation.name;

    actions.push(action);

    return actions.size - 1;
}

//Find methods using names
uint32_t InputHandler::findActionMap(const char* name) const 
{
    //TODO: We might need to move this to a hash map according to the book.
    for (uint32_t i = 0; i < actionMaps.size; ++i) 
    {
        if (strcmp(name, actionMaps[i].name) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

uint32_t InputHandler::findAction(const char* name) const 
{
    //TODO: We might need to move this to a hash map according to the book.
    for (uint32_t i = 0; i < actions.size; ++i)
    {
        if (strcmp(name, actions[i].name) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

void InputHandler::addButton(uint32_t action, DevicePart device, uint16_t button, bool repeat/* = false*/) 
{
    const InputAction& bindingAction = actions[action];

    InputBinding binding;
    binding.set(BINDING_TYPE_BUTTON, deviceFromPart(device), device, button, 0, 0, repeat)
           .setHandles(bindingAction.actionMap, action);

    bindings.push(binding);
}

void InputHandler::addAxis1D(uint32_t action, DevicePart device, uint16_t axis, float minDeadzone, float maxDeadzone) 
{
    const InputAction& bindingAction = actions[action];

    InputBinding binding;
    binding.set(BINDING_TYPE_AXIS_1D, deviceFromPart(device), device, axis, 0, 0, 0)
           .setDeadzones(minDeadzone, maxDeadzone).setHandles(bindingAction.actionMap, action);

    bindings.push(binding);
}

void InputHandler::addAxis2D(uint32_t action, DevicePart device, uint16_t xAxis, uint16_t yAxis, float minDeadzone, float maxDeadzone) 
{
    const InputAction& bindingAction = actions[action];

    InputBinding binding;
    InputBinding bindingX;
    InputBinding bindingY;

    binding.set(BINDING_TYPE_AXIS_2D, deviceFromPart(device), device, UINT16_MAX, 1, 0, 0)
           .setDeadzones(minDeadzone, maxDeadzone).setHandles(bindingAction.actionMap, action);
    bindingX.set(BINDING_TYPE_AXIS_2D, deviceFromPart(device), device, xAxis, 0, 1, 0)
           .setDeadzones(minDeadzone, maxDeadzone).setHandles(bindingAction.actionMap, action);
    bindingY.set(BINDING_TYPE_AXIS_2D, deviceFromPart(device), device, yAxis, 0, 1, 0)
           .setDeadzones(minDeadzone, maxDeadzone).setHandles(bindingAction.actionMap, action);

    bindings.push(binding);
    bindings.push(bindingX);
    bindings.push(bindingY);
}

void InputHandler::addVector1D(uint32_t action, DevicePart devicePos, uint16_t buttonPos,
                                DevicePart deviceNeg, uint16_t buttonNeg,
                                bool repeat/* = true*/) 
{
    const InputAction& bindingAction = actions[action];

    InputBinding binding;
    InputBinding bindingPositive;
    InputBinding bindingNegative;

    binding.set(BINDING_TYPE_VECTOR_1D, deviceFromPart(devicePos), devicePos, UINT16_MAX, 1, 0, repeat)
           .setHandles(bindingAction.actionMap, action);
    bindingPositive.set(BINDING_TYPE_VECTOR_1D, deviceFromPart(devicePos), devicePos, buttonPos, 0, 1, repeat)
                   .setHandles(bindingAction.actionMap, action);
    bindingNegative.set(BINDING_TYPE_VECTOR_1D, deviceFromPart(deviceNeg), deviceNeg, buttonNeg, 0, 1, repeat)
                   .setHandles(bindingAction.actionMap, action);

    bindings.push(binding);
    bindings.push(bindingPositive);
    bindings.push(bindingNegative);
}

void InputHandler::addVector2D(uint32_t action, 
                                DevicePart deviceUp, uint16_t buttonUp,
                                DevicePart deviceDown, uint16_t buttonDown,
                                DevicePart deviceLeft, uint16_t buttonLeft,
                                DevicePart deviceRight, uint16_t buttonRight,
                                bool repeat/* = true*/) 
{
    const InputAction& bindingAction = actions[action];

    InputBinding binding;
    InputBinding bindingUp;
    InputBinding bindingDown;
    InputBinding bindingLeft;
    InputBinding bindingRight;

    binding.set(BINDING_TYPE_VECTOR_2D, deviceFromPart(deviceUp), deviceUp, UINT16_MAX, 1, 0, repeat)
           .setHandles(bindingAction.actionMap, action);
    bindingUp.set(BINDING_TYPE_VECTOR_2D, deviceFromPart(deviceUp), deviceUp, buttonUp, 0, 1, repeat)
               .setHandles(bindingAction.actionMap, action);
    bindingDown.set(BINDING_TYPE_VECTOR_2D, deviceFromPart(deviceDown), deviceDown, buttonDown, 0, 1, repeat)
               .setHandles(bindingAction.actionMap, action);
    bindingLeft.set(BINDING_TYPE_VECTOR_2D, deviceFromPart(deviceLeft), deviceLeft, buttonLeft, 0, 1, repeat)
               .setHandles(bindingAction.actionMap, action);
    bindingRight.set(BINDING_TYPE_VECTOR_2D, deviceFromPart(deviceRight), deviceRight, buttonRight, 0, 1, repeat)
               .setHandles(bindingAction.actionMap, action);

    bindings.push(binding);
    bindings.push(bindingUp);
    bindings.push(bindingDown);
    bindings.push(bindingLeft);
    bindings.push(bindingRight);
}