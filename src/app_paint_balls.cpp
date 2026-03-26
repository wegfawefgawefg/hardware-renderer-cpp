#include "app.h"

#include <cmath>

#include <SDL3/SDL_keyboard.h>

#include "imgui.h"

namespace
{
Vec3 CameraRayDirection(const Camera& camera)
{
    return CameraForward(camera);
}
}

bool App::TryFirePaintBall()
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
    float fireRate = std::max(m_paintBallSettings.fireRate, 0.01f);
    m_paintBallFireCooldown = 1.0f / fireRate;
    return true;
}

void App::UpdatePaintBalls(float dtSeconds)
{
    m_paintBallFireCooldown = std::max(0.0f, m_paintBallFireCooldown - dtSeconds);
    if (m_paintBallFireHeld &&
        m_mouseCaptured &&
        (ImGui::GetCurrentContext() == nullptr || !ImGui::GetIO().WantCaptureMouse))
    {
        while (m_paintBallFireCooldown <= 0.0f)
        {
            if (!TryFirePaintBall())
            {
                break;
            }
        }
    }
    m_paintBalls.Update(m_worldCollider, dtSeconds, m_paintBallSettings);
}
