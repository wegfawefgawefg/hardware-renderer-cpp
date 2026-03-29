#include "app.h"

#include <algorithm>
#include <cmath>

#include "damage/mesh_damage.h"

namespace
{
constexpr std::string_view kFractureDecalAssetPath = "generated/fracture_decal";
constexpr float kPi = 3.14159265358979323846f;

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

Vec3 RotateAroundAxis(Vec3 v, Vec3 axis, float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Add(
        Vec3Add(Vec3Scale(v, c), Vec3Scale(Vec3Cross(axis, v), s)),
        Vec3Scale(axis, Vec3Dot(axis, v) * (1.0f - c))
    );
}

Mat4 BuildDecalTransform(Vec3 position, Vec3 normal, float size, float rollRadians)
{
    Vec3 forward = Vec3Normalize(normal);
    Vec3 referenceUp = std::fabs(forward.y) < 0.95f ? Vec3Make(0.0f, 1.0f, 0.0f) : Vec3Make(1.0f, 0.0f, 0.0f);
    Vec3 right = Vec3Normalize(Vec3Cross(referenceUp, forward));
    if (Vec3Length(right) <= 1e-4f)
    {
        referenceUp = Vec3Make(0.0f, 0.0f, 1.0f);
        right = Vec3Normalize(Vec3Cross(referenceUp, forward));
    }
    Vec3 up = Vec3Normalize(Vec3Cross(forward, right));
    if (std::fabs(rollRadians) > 1e-6f)
    {
        right = RotateAroundAxis(right, forward, rollRadians);
        up = RotateAroundAxis(up, forward, rollRadians);
    }

    Mat4 result = Mat4Identity();
    result.m[0] = right.x * size;
    result.m[1] = right.y * size;
    result.m[2] = right.z * size;
    result.m[4] = up.x * size;
    result.m[5] = up.y * size;
    result.m[6] = up.z * size;
    result.m[8] = forward.x;
    result.m[9] = forward.y;
    result.m[10] = forward.z;
    result.m[12] = position.x;
    result.m[13] = position.y;
    result.m[14] = position.z;
    return result;
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

bool PlaceFractureDecal(SceneData& scene, FractureState& fracture, Vec3 hitPosition, Vec3 hitNormal, float size, float rollRadians)
{
    std::vector<std::uint32_t> decalEntities;
    decalEntities.reserve(scene.entities.size());
    for (std::uint32_t entityIndex = 0; entityIndex < scene.entities.size(); ++entityIndex)
    {
        if (scene.entities[entityIndex].assetPath == kFractureDecalAssetPath)
        {
            decalEntities.push_back(entityIndex);
        }
    }
    if (decalEntities.empty())
    {
        return false;
    }

    std::uint32_t slot = fracture.nextDecalSlot % static_cast<std::uint32_t>(decalEntities.size());
    fracture.nextDecalSlot = (slot + 1u) % static_cast<std::uint32_t>(decalEntities.size());
    EntityData& entity = scene.entities[decalEntities[slot]];
    Vec3 decalPosition = Vec3Add(hitPosition, Vec3Scale(hitNormal, std::max(0.01f, size * 0.06f)));
    entity.transform = BuildDecalTransform(decalPosition, hitNormal, size * 2.0f, rollRadians);
    return true;
}
}

bool App::TryFireFractureShot()
{
    auto& core = m_state.core;
    auto& fracture = m_state.fracture;
    Vec3 rayDirection = FractureRayDirection(core.camera);
    TriangleMeshCollider::RayHit hit = core.worldCollider.Raycast(core.camera.position, rayDirection, 200.0f);
    fracture.hitValid = hit.hit;
    if (!hit.hit)
    {
        return false;
    }

    fracture.hitPosition = hit.position;
    fracture.hitNormal = hit.normal;
    if (fracture.settings.mesh.mode == damage::Mode::DamageDecal)
    {
        bool placed = false;
        std::uint32_t burstCount = std::max(1u, fracture.settings.mesh.decalBurstCount);
        for (std::uint32_t pellet = 0; pellet < burstCount; ++pellet)
        {
            std::uint32_t seed = fracture.decalShotCounter++;
            Vec3 pelletDirection = BuildSpreadDirection(rayDirection, seed + pellet * 97u, fracture.settings.mesh.decalSpreadDegrees);
            TriangleMeshCollider::RayHit pelletHit = core.worldCollider.Raycast(core.camera.position, pelletDirection, 200.0f);
            if (!pelletHit.hit)
            {
                continue;
            }
            float rollRange = DegreesToRadians(fracture.settings.mesh.decalRollVarianceDegrees);
            float roll = (Hash01(seed * 53u + 7u) * 2.0f - 1.0f) * rollRange;
            bool pelletPlaced = PlaceFractureDecal(core.scene, fracture, pelletHit.position, pelletHit.normal, fracture.settings.mesh.radius, roll);
            placed |= pelletPlaced;
            if (pelletPlaced)
            {
                PlayRicochetSound();
            }
            fracture.hitPosition = pelletHit.position;
            fracture.hitNormal = pelletHit.normal;
            fracture.hitValid = true;
        }
        if (placed)
        {
            fracture.fireCooldown = 1.0f / std::max(fracture.settings.fireRate, 0.01f);
        }
        return placed;
    }

    if (!damage::ApplyMeshDamage(core.scene, hit, rayDirection, fracture.settings.mesh))
    {
        return false;
    }
    if (fracture.settings.mesh.mode == damage::Mode::Dent)
    {
        RefreshCurrentSceneGeometry();
    }
    else
    {
        RebuildCurrentSceneResources();
    }
    fracture.fireCooldown = 1.0f / std::max(fracture.settings.fireRate, 0.01f);
    return true;
}

void App::UpdateFractureSandbox(float dtSeconds)
{
    auto& core = m_state.core;
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

    TriangleMeshCollider::RayHit hoverHit = core.worldCollider.Raycast(core.camera.position, FractureRayDirection(core.camera), 200.0f);
    fracture.hitValid = hoverHit.hit;
    if (hoverHit.hit)
    {
        fracture.hitPosition = hoverHit.position;
        fracture.hitNormal = hoverHit.normal;
    }
    if (fracture.settings.mesh.mode == damage::Mode::DamageDecal &&
        m_state.runtime.mouseCaptured &&
        fracture.fireHeld &&
        fracture.fireCooldown <= 0.0f)
    {
        TryFireFractureShot();
    }
}
