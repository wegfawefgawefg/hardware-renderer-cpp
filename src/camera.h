#pragma once

#include "math_types.h"

struct Camera
{
    Vec3 position = Vec3Make(0.0f, 1.1f, 3.6f);
    float yawRadians = DegreesToRadians(180.0f);
    float pitchRadians = DegreesToRadians(-8.0f);
    float moveSpeed = 3.5f;
    float lookSpeed = 0.0025f;
};

Vec3 CameraForward(const Camera& camera);
Vec3 CameraRight(const Camera& camera);
Mat4 CameraViewMatrix(const Camera& camera);
bool CameraUpdateFromInput(Camera& camera, float dtSeconds, bool mouseCaptured);
void CameraAddMouseLook(Camera& camera, float deltaX, float deltaY);
