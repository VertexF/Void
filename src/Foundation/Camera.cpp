#include "Camera.hpp"

#include "cglm/struct/cam.h"
#include "cglm/struct/affine.h"
#include "cglm/struct/quat.h"
#include "cglm/struct/project.h"

void Camera::initPerspective(float nearPlane, float farPlane, float fov, float aspectRatio) 
{
    perspective = true;

    this->nearPlane = nearPlane;
    this->farPlane = farPlane;
    this->fieldOfView = fov;
    this->aspectRatio = aspectRatio;

    reset();
}

void Camera::initOrthographic(float nearPlane, float farPlane, float viewportWidth, float viewportHeight, float zoom) 
{
    perspective = false;

    this->nearPlane = nearPlane;
    this->farPlane = farPlane;
    this->zoom = zoom;
    this->viewportWidth = viewportWidth;
    this->viewportHeight = viewportHeight;

    reset();
}

void Camera::reset() 
{
    position = { 2.f, 2.f, 2.f };
    yaw = 0.f;
    pitch = 0.f;
    view = glms_mat4_identity();
    projection = glms_mat4_identity();

    updateProjection = true;
}

void Camera::resetPoistion() 
{
    position = { 0.f, 1.f, 0.f };
}

void Camera::setViewportSize(float width, float height) 
{
    viewportWidth = width;
    viewportHeight = height;

    updateProjection = true;
}

void Camera::setZoom(float zoom) 
{
    this->zoom = zoom;
    updateProjection = true;
}

void Camera::setAspectRatio(float aspectRatio) 
{
    this->aspectRatio = aspectRatio;
    updateProjection = true;
}

void Camera::setFov(float fov) 
{
    fieldOfView = fov;
    updateProjection = true;
}

void Camera::update() 
{
    //mat4s rym = glms_rotate_make(glm_rad(45.0f), vec3s{ 0.0f, 1.0f, 0.0f });

    //mat4s sm = glms_scale_make(vec3s{ model_scale, model_scale, model_scale });
    //global_model = glms_mat4_mul(rym, sm);

    //Quaternion based rotation
    const versors pitchRotation = glms_quat(pitch, 1, 0, 0);
    const versors yawRotation = glms_quat(yaw, 0, 1, 0);
    const versors rotation = glms_quat_normalize(glms_quat_mul(pitchRotation, yawRotation));

    const mat4s translation = glms_translate_make(glms_vec3_scale(position, 1.f));
    view = glms_mat4_mul(glms_quat_mat4(rotation), translation);

    //Update the vector for movement
    right = { view.m00, view.m10, view.m20 };
    up = { view.m01, view.m11, view.m21 };
    direction = { view.m02, view.m12, view.m22 };

    if (updateProjection) 
    {
        updateProjection = false;
        calculateProjectionMatrix();
    }

    //Calculate final view projection matrix
    calculateViewProjection();
}

void Camera::rotate(float deltaPitch, float deltaYaw) 
{
    pitch += deltaPitch;
    yaw += deltaYaw;
}

void Camera::calculateProjectionMatrix() 
{
    if (perspective) 
    {
        projection = glms_perspective(glm_rad(fieldOfView), aspectRatio, 100.f, 1.f);
    }
    else 
    {
        projection = glms_ortho(zoom * -viewportWidth / 2.f, zoom * viewportWidth / 2.f,
                                zoom * -viewportHeight / 2.f, zoom * viewportHeight / 2.f, nearPlane, farPlane);
    }
}

void Camera::calculateViewProjection() 
{
    viewProjection = glms_mat4_mul(projection, view);
}

//Project/unproject
vec3s Camera::unproject(const vec3s& screenCoordinates)
{
    return glms_unproject(screenCoordinates, viewProjection, { 0, 0, viewportWidth, viewportHeight });
}

//Unproject by inverting the y of the screen coordinate.
vec3s Camera::unprojectInverted(const vec3s& screenCooridates)
{
    const vec3s screenCooridatesInvertedY{ screenCooridates.x, viewportHeight - screenCooridates.y, screenCooridates.z };
    return unproject(screenCooridatesInvertedY);
}

void Camera::getProjectionOrtho2D(mat4s& outMatrix) 
{
    outMatrix = glms_ortho(0, viewportWidth * zoom, 0, viewportHeight * zoom, -1.f, 1.f);
}

void Camera::yawPitchFromDirection(const vec3s& direction, float& yaw, float& pitch)
{
    yaw = glm_deg(atan2f(direction.z, direction.x));
    pitch = glm_deg(asinf(direction.y));
}
