#ifndef CAMERA_HDR
#define CAMERA_HDR

#include "Platform.hpp"
#include <cglm/struct/mat4.h>

//Camera struct can be both perspective and orthographic.
struct Camera 
{
    void initPerspective(float nearPlane, float farPlane, float fov, float aspectRatio);
    void initOrthographic(float nearPlane, float farPlane, float viewportWidth, float viewportHeight, float zoom);

    void reset();
    void resetPoistion();

    void setViewportSize(float width, float height);
    void setZoom(float zoom);
    void setAspectRatio(float aspectRatio);
    void setFov(float fov);

    void update();
    void rotate(float deltaPitch, float deltaYaw);

    void calculateProjectionMatrix();
    void calculateViewProjection();

    //Project/unproject
    vec3s unproject(const vec3s& screenCoordinates);

    //Unproject by inverting the y of the screen coordinate.
    vec3s unprojectInverted(const vec3s& screenCooridates);

    void getProjectionOrtho2D(mat4s& outMatrix);

    static void yawPitchFromDirection(const vec3s& direction, float &yaw, float &pitch);

    mat4s view{};
    mat4s projection{};
    mat4s viewProjection{};

    vec3s position{};
    vec3s right{};
    vec3s direction{};
    vec3s up{};

    float yaw = 0.f;
    float pitch = 0.f;

    float nearPlane = 0.f;
    float farPlane = 0.f;

    float fieldOfView = 0.f;
    float aspectRatio = 0.f;

    float zoom = 0.f;
    float viewportWidth = 0.f;
    float viewportHeight = 0.f;

    bool perspective = false;
    bool updateProjection = false;
};

#endif // !CAMERA_HDR
