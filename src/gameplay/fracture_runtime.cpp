#include "app.h"

#include <algorithm>

namespace
{
Vec3 FractureRayDirection(const Camera& camera)
{
    return CameraForward(camera);
}
}

bool App::TryFireFractureShot()
{
    auto& core = m_state.core;
    auto& fracture = m_state.fracture;
    FractureHit hit = core.fracture.Raycast(core.camera.position, FractureRayDirection(core.camera), 200.0f);
    fracture.hitValid = hit.hit;
    if (!hit.hit)
    {
        return false;
    }

    fracture.hitPosition = hit.position;
    fracture.hitNormal = hit.normal;
    core.fracture.FractureAt(hit, fracture.settings);
    fracture.fireCooldown = 1.0f / std::max(fracture.settings.fireRate, 0.01f);
    return true;
}

void App::UpdateFractureSandbox(float dtSeconds)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    auto& fracture = m_state.fracture;
    if (lighting.sceneKind != SceneKind::FractureTest)
    {
        fracture.hitValid = false;
        fracture.fireHeld = false;
        fracture.fireCooldown = 0.0f;
        return;
    }

    fracture.fireCooldown = std::max(0.0f, fracture.fireCooldown - dtSeconds);
    core.fracture.Update(dtSeconds, fracture.settings);

    FractureHit hoverHit = core.fracture.Raycast(core.camera.position, FractureRayDirection(core.camera), 200.0f);
    fracture.hitValid = hoverHit.hit;
    if (hoverHit.hit)
    {
        fracture.hitPosition = hoverHit.position;
        fracture.hitNormal = hoverHit.normal;
    }

    if (fracture.fireHeld && runtime.mouseCaptured)
    {
        while (fracture.fireCooldown <= 0.0f)
        {
            if (!TryFireFractureShot())
            {
                break;
            }
        }
    }
}
