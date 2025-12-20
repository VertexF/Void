#ifndef GAME_CAMERA_HDR
#define GAME_CAMERA_HDR

#include "Foundation/Camera.hpp"
#include "Application/Keys.hpp"
#include "Application/Input.hpp"

#include <cglm/types-struct.h>

struct GameCamera 
{
    void init(bool enabled = true, float rotation = 10.f, float movementSpeed = 10.f, float movementDelta = 0.f);
    void reset();

    void update(InputHandler* input, uint32_t windowWidth, uint32_t windowHeight, float deltaTime);
    void applyJittering(float x, float y);

    Camera internal3DCamera{};
    vec3s targetMovement{};

    float targetYaw{0.f};
    float targetPitch{0.f};

    float mouseSensitivity{1.f};
    float movementDelta{0.f};
    uint32_t ignoreDraggingFrames{0};

    float rotationSpeed{10.f};
    float movementSpeed{10.f};

    bool enabled{true};
    bool mouseDragging{false};
};

#endif // !GAME_CAMERA_HDR
