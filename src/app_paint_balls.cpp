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

void App::AppendPaintSplat(const PaintSplatSpawn& splat)
{
    auto& paint = m_state.paint;
    PaintSplat& dst = paint.splats[paint.nextSplatIndex];
    dst.position = splat.position;
    dst.radius = splat.radius;
    dst.normal = Vec3Normalize(splat.normal);
    dst.color = splat.color;
    paint.nextSplatIndex = (paint.nextSplatIndex + 1u) % kMaxPaintSplats;
    paint.splatCount = std::min(paint.splatCount + 1u, kMaxPaintSplats);
}

bool App::TryFirePaintBall()
{
    auto& core = m_state.core;
    auto& paint = m_state.paint;
    Vec3 forward = CameraRayDirection(core.camera);
    Vec3 spawn = Vec3Add(
        core.camera.position,
        Vec3Add(
            Vec3Scale(forward, 0.45f),
            Vec3Make(0.0f, -0.08f, 0.0f)
        )
    );
    core.paintBalls.Fire(spawn, forward, paint.ballSettings);
    float fireRate = std::max(paint.ballSettings.fireRate, 0.01f);
    paint.fireCooldown = 1.0f / fireRate;
    return true;
}

void App::UpdatePaintBalls(float dtSeconds)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& paint = m_state.paint;
    std::vector<PaintSplatSpawn> newSplats;
    newSplats.reserve(16);
    paint.fireCooldown = std::max(0.0f, paint.fireCooldown - dtSeconds);
    if (paint.fireHeld &&
        runtime.mouseCaptured &&
        (ImGui::GetCurrentContext() == nullptr || !ImGui::GetIO().WantCaptureMouse))
    {
        while (paint.fireCooldown <= 0.0f)
        {
            if (!TryFirePaintBall())
            {
                break;
            }
        }
    }
    core.paintBalls.Update(core.worldCollider, dtSeconds, paint.ballSettings, newSplats);
    for (const PaintSplatSpawn& splat : newSplats)
    {
        AppendPaintSplat(splat);
    }
}
