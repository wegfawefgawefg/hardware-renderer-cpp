#include "app.h"

#include <cmath>

#include <SDL3/SDL_keyboard.h>

namespace
{
Vec3 CameraRayDirection(const Camera& camera)
{
    return CameraForward(camera);
}
}

void App::TryFirePaintBall()
{
    Vec3 forward = CameraRayDirection(m_camera);
    Vec3 spawn = Vec3Add(
        m_camera.position,
        Vec3Add(
            Vec3Scale(forward, 0.45f),
            Vec3Make(0.0f, -0.08f, 0.0f)
        )
    );
    m_paintBalls.Fire(spawn, forward, m_paintBallSettings);
}

void App::UpdatePaintBalls(float dtSeconds)
{
    m_paintBalls.Update(m_worldCollider, dtSeconds, m_paintBallSettings);
}
