#include "gameplay/fracture_sandbox.h"

#include <algorithm>
#include <cmath>

#include "audio/audio_system.h"
#include "decals/flat_decal_system.h"
#include "damage/mesh_damage.h"

namespace
{
Vec3 FractureRayDirection(const Camera& camera)
{
    return CameraForward(camera);
}

float Hash01(std::uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    v ^= v >> 16;
    return static_cast<float>(v & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

Vec3 BuildSpreadDirection(Vec3 baseDirection, std::uint32_t seed, float spreadDegrees)
{
    if (spreadDegrees <= 0.01f)
    {
        return Vec3Normalize(baseDirection);
    }
    Vec3 forward = Vec3Normalize(baseDirection);
    Vec3 referenceUp = std::fabs(forward.y) < 0.95f ? Vec3Make(0.0f, 1.0f, 0.0f) : Vec3Make(1.0f, 0.0f, 0.0f);
    Vec3 right = Vec3Normalize(Vec3Cross(referenceUp, forward));
    Vec3 up = Vec3Normalize(Vec3Cross(forward, right));
    float spread = DegreesToRadians(spreadDegrees);
    float yaw = (Hash01(seed * 17u + 3u) * 2.0f - 1.0f) * spread;
    float pitch = (Hash01(seed * 31u + 11u) * 2.0f - 1.0f) * spread;
    Vec3 offset = Vec3Add(Vec3Scale(right, std::tan(yaw)), Vec3Scale(up, std::tan(pitch)));
    return Vec3Normalize(Vec3Add(forward, offset));
}

decals::FlatDecalTemplateId CurrentDamageDecalTemplate(const FractureSandboxState& state)
{
    if (state.damageDecalTemplateCount == 0)
    {
        return decals::kInvalidFlatDecalTemplateId;
    }
    std::uint32_t index = std::min(state.selectedDamageDecalTemplate, state.damageDecalTemplateCount - 1u);
    return state.damageDecalTemplates[index];
}
}

void UpdateFractureSandboxHover(
    const TriangleMeshCollider& worldCollider,
    const Camera& camera,
    FractureSandboxState& state,
    bool fractureSceneActive,
    float dtSeconds)
{
    if (!fractureSceneActive)
    {
        state.hitValid = false;
        state.fireHeld = false;
        state.fireCooldown = 0.0f;
        return;
    }

    state.fireCooldown = std::max(0.0f, state.fireCooldown - dtSeconds);

    TriangleMeshCollider::RayHit hoverHit = worldCollider.Raycast(camera.position, FractureRayDirection(camera), 200.0f);
    state.hitValid = hoverHit.hit;
    if (!hoverHit.hit)
    {
        return;
    }

    state.hitPosition = hoverHit.position;
    state.hitNormal = hoverHit.normal;
}

FractureShotResult FireFractureSandboxShot(
    SceneData& scene,
    const TriangleMeshCollider& worldCollider,
    const Camera& camera,
    FractureSandboxState& state,
    decals::FlatDecalSystem& flatDecals,
    audio::System& audio)
{
    Vec3 rayDirection = FractureRayDirection(camera);
    TriangleMeshCollider::RayHit hit = worldCollider.Raycast(camera.position, rayDirection, 200.0f);
    state.hitValid = hit.hit;
    if (!hit.hit)
    {
        return {};
    }

    state.hitPosition = hit.position;
    state.hitNormal = hit.normal;
    if (state.settings.mesh.mode == damage::Mode::DamageDecal)
    {
        decals::FlatDecalTemplateId templateId = CurrentDamageDecalTemplate(state);
        if (templateId == decals::kInvalidFlatDecalTemplateId)
        {
            return {};
        }

        bool placed = false;
        std::uint32_t burstCount = std::max(1u, state.settings.mesh.decalBurstCount);
        for (std::uint32_t pellet = 0; pellet < burstCount; ++pellet)
        {
            std::uint32_t seed = state.decalShotCounter++;
            Vec3 pelletDirection = BuildSpreadDirection(rayDirection, seed + pellet * 97u, state.settings.mesh.decalSpreadDegrees);
            TriangleMeshCollider::RayHit pelletHit = worldCollider.Raycast(camera.position, pelletDirection, 200.0f);
            if (!pelletHit.hit)
            {
                continue;
            }

            float rollRange = DegreesToRadians(state.settings.mesh.decalRollVarianceDegrees);
            float roll = (Hash01(seed * 53u + 7u) * 2.0f - 1.0f) * rollRange;
            bool pelletPlaced = decals::SpawnFlatDecal(
                flatDecals,
                templateId,
                pelletHit.position,
                pelletHit.normal,
                state.settings.mesh.radius,
                roll);
            placed |= pelletPlaced;
            if (pelletPlaced)
            {
                audio::PlayRicochet(audio);
            }
            state.hitPosition = pelletHit.position;
            state.hitNormal = pelletHit.normal;
            state.hitValid = true;
        }

        if (!placed)
        {
            return {};
        }

        state.fireCooldown = 1.0f / std::max(state.settings.fireRate, 0.01f);
        return {
            .applied = true,
            .refresh = FractureSceneRefresh::None,
        };
    }

    if (!damage::ApplyMeshDamage(scene, hit, rayDirection, state.settings.mesh))
    {
        return {};
    }

    state.fireCooldown = 1.0f / std::max(state.settings.fireRate, 0.01f);
    return {
        .applied = true,
        .refresh = state.settings.mesh.mode == damage::Mode::Dent
            ? FractureSceneRefresh::GeometryOnly
            : FractureSceneRefresh::FullRebuild,
    };
}
