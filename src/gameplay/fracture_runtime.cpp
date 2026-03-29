#include "app.h"

#include "gameplay/fracture_sandbox.h"

bool App::TryFireFractureShot()
{
    auto& core = m_state.core;
    auto& fracture = m_state.fracture;
    FractureShotResult result =
        FireFractureSandboxShot(core.scene, core.worldCollider, core.camera, fracture, core.flatDecals, m_audio);
    if (!result.applied)
    {
        return false;
    }

    if (result.refresh == FractureSceneRefresh::GeometryOnly)
    {
        RefreshCurrentSceneGeometry();
    }
    else if (result.refresh == FractureSceneRefresh::FullRebuild)
    {
        RebuildCurrentSceneResources();
    }

    return true;
}

void App::UpdateFractureSandbox(float dtSeconds)
{
    auto& lighting = m_state.lighting;
    auto& fracture = m_state.fracture;
    auto& paint = m_state.paint;
    UpdateFractureSandboxHover(
        m_state.core.worldCollider,
        m_state.core.camera,
        fracture,
        lighting.sceneKind == SceneKind::FractureTest ||
            (lighting.sceneKind == SceneKind::City &&
             paint.interactionMode == PaintInteractionMode::Damage),
        dtSeconds);
    if (fracture.settings.mesh.mode == damage::Mode::DamageDecal &&
        m_state.runtime.mouseCaptured &&
        fracture.fireHeld &&
        fracture.fireCooldown <= 0.0f)
    {
        TryFireFractureShot();
    }
}
