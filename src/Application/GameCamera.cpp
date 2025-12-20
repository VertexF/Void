#include "GameCamera.hpp"

#include "Foundation/Platform.hpp"
#include "Foundation/Numerics.hpp"

#include <cglm/struct/affine.h>
#include <cglm/struct/cam.h>

//We are doing this to get frame indepenent lerp.
//http://www.rorydriscoll.com/2016/03/07/frame-rate-independent-damping-using-lerp/
static float lerp(float a, float b, float t, float deltaTime) 
{
    return glm_lerp(a, b, 1.f - powf(1 - t, deltaTime));
}

static vec3s lerp3(const vec3s& from, const vec3s& to, float t, float deltaTime) 
{
    return vec3s{ lerp(from.x, to.x, t, deltaTime), lerp(from.y, to.y, t, deltaTime), lerp(from.z, to.z, t, deltaTime) };
}

void GameCamera::init(bool inEnabled /*= true*/, float inRotationSpeed /*= 10.f*/,
                        float inMovementSpeed /*= 10.f*/, float inMovementDelta /*= 0.f*/) 
{
    reset();

    enabled = inEnabled;
    rotationSpeed = inRotationSpeed;
    movementSpeed = inMovementSpeed;
    movementDelta = inMovementDelta;
}

void GameCamera::reset() 
{
    targetYaw = 0.f;
    targetPitch = 0.f;

    targetMovement = internal3DCamera.position;

    mouseDragging = false;
    ignoreDraggingFrames = 3;
    mouseSensitivity = 2.f;
}

void GameCamera::update(InputHandler* input, uint32_t windowWidth, uint32_t windowHeight, float deltaTime) 
{
    if (enabled == false)
    {
        return;
    }

    internal3DCamera.update();

    //Ignore first dragging frames for mouse movement waiting the cursor to be placed at the center of the screen.
    if (input->isMouseDragging(MOUSE_BUTTON_RIGHT) /*&& !ImGui::IsAnyItemHovered()*/)
    {
        if (ignoreDraggingFrames == 0) 
        {
            targetYaw -= (input->mousePosition.x - (windowWidth / 2.f)) * mouseSensitivity * deltaTime;
            targetPitch -= (input->mousePosition.y - (windowHeight / 2.f)) * mouseSensitivity * deltaTime;
        }
        else 
        {
            --ignoreDraggingFrames;
        }
        mouseDragging = true;
    }
    else 
    {
        mouseDragging = false;
        ignoreDraggingFrames = 3;
    }

    vec3s cameraMovement{ 0.f, 0.f, 0.f };
    float cameraMovementDelta = movementDelta/* * 0.06f*/;

    //Change movemenet
    if (input->isKeyDown(KEY_RSHIFT) || input->isKeyDown(KEY_LSHIFT))
    {
        cameraMovementDelta *= 10.f;
    }

    if (input->isKeyDown(KEY_RALT) || input->isKeyDown(KEY_LALT))
    {
        cameraMovementDelta *= 100.f;
    }

    if (input->isKeyDown(KEY_RCTRL) || input->isKeyDown(KEY_LCTRL))
    {
        cameraMovementDelta *= 0.1f;
    }

    //Actual 3D movememnt
    if (input->isKeyDown(KEY_LEFT) || input->isKeyDown(KEY_A))
    {
        cameraMovement = glms_vec3_add(cameraMovement, glms_vec3_scale(internal3DCamera.right, -cameraMovementDelta));
    }
    else if (input->isKeyDown(KEY_RIGHT) || input->isKeyDown(KEY_D))
    {
        cameraMovement = glms_vec3_add(cameraMovement, glms_vec3_scale(internal3DCamera.right, cameraMovementDelta));
    }

    if (input->isKeyDown(KEY_PAGEDOWN) || input->isKeyDown(KEY_E))
    {
        cameraMovement = glms_vec3_add(cameraMovement, glms_vec3_scale(internal3DCamera.up, -cameraMovementDelta));
    }
    else if (input->isKeyDown(KEY_PAGEUP) || input->isKeyDown(KEY_Q))
    {
        cameraMovement = glms_vec3_add(cameraMovement, glms_vec3_scale(internal3DCamera.up, cameraMovementDelta));
    }

    if (input->isKeyDown(KEY_UP) || input->isKeyDown(KEY_W))
    {
        cameraMovement = glms_vec3_add(cameraMovement, glms_vec3_scale(internal3DCamera.direction, cameraMovementDelta));
    }
    else if (input->isKeyDown(KEY_DOWN) || input->isKeyDown(KEY_S))
    {
        cameraMovement = glms_vec3_add(cameraMovement, glms_vec3_scale(internal3DCamera.direction, -cameraMovementDelta));
    }

    targetMovement = glms_vec3_add(targetMovement, cameraMovement);

    {
        const float tweenSpeed = rotationSpeed * deltaTime;
        internal3DCamera.rotate((targetPitch - internal3DCamera.pitch) * tweenSpeed, (targetYaw - internal3DCamera.yaw) * tweenSpeed);

        const float tweenPositionSpeed = movementSpeed * deltaTime;
        internal3DCamera.position = lerp3(internal3DCamera.position, targetMovement, 0.9f, tweenPositionSpeed);
    }
}

void GameCamera::applyJittering(float x, float y) 
{
    //Reset camera projection
    internal3DCamera.calculateProjectionMatrix();

    mat4s jitteringMatix = glms_translate_make({x, y, 0.f});
    internal3DCamera.projection = glms_mat4_mul(jitteringMatix, internal3DCamera.projection);
    internal3DCamera.calculateViewProjection();
}