#include "camera.h"

#include <SDL3/SDL_keyboard.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPitchLimit = 1.55334306f;
}

Vec3 CameraForward(const Camera& camera)
{
    float cosPitch = std::cos(camera.pitchRadians);
    return Vec3Normalize(
        Vec3Make(
            std::sin(camera.yawRadians) * cosPitch,
            std::sin(camera.pitchRadians),
            std::cos(camera.yawRadians) * cosPitch
        )
    );
}

Vec3 CameraRight(const Camera& camera)
{
    return Vec3Normalize(Vec3Cross(CameraForward(camera), Vec3Make(0.0f, 1.0f, 0.0f)));
}

Mat4 CameraViewMatrix(const Camera& camera)
{
    Vec3 forward = CameraForward(camera);
    return Mat4LookAt(camera.position, Vec3Add(camera.position, forward), Vec3Make(0.0f, 1.0f, 0.0f));
}

bool CameraUpdateFromInput(Camera& camera, float dtSeconds, bool mouseCaptured)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    Vec3 move = Vec3Make(0.0f, 0.0f, 0.0f);
    Vec3 forward = CameraForward(camera);
    Vec3 right = CameraRight(camera);
    Vec3 flatForward = Vec3Normalize(Vec3Make(forward.x, 0.0f, forward.z));

    if (keys[SDL_SCANCODE_W])
    {
        move = Vec3Add(move, flatForward);
    }
    if (keys[SDL_SCANCODE_S])
    {
        move = Vec3Sub(move, flatForward);
    }
    if (keys[SDL_SCANCODE_D])
    {
        move = Vec3Add(move, right);
    }
    if (keys[SDL_SCANCODE_A])
    {
        move = Vec3Sub(move, right);
    }
    if (keys[SDL_SCANCODE_SPACE])
    {
        move = Vec3Add(move, Vec3Make(0.0f, 1.0f, 0.0f));
    }
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
    {
        move = Vec3Sub(move, Vec3Make(0.0f, 1.0f, 0.0f));
    }

    bool moved = Vec3Length(move) > 0.0001f;
    if (moved)
    {
        camera.position = Vec3Add(
            camera.position,
            Vec3Scale(Vec3Normalize(move), camera.moveSpeed * dtSeconds)
        );
    }

    return moved || mouseCaptured;
}

void CameraAddMouseLook(Camera& camera, float deltaX, float deltaY)
{
    camera.yawRadians -= deltaX * camera.lookSpeed;
    camera.pitchRadians -= deltaY * camera.lookSpeed;
    camera.pitchRadians = std::clamp(camera.pitchRadians, -kPitchLimit, kPitchLimit);
}
