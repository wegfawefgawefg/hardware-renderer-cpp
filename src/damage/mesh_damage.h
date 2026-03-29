#pragma once

#include "collision/triangle_collider.h"
#include "scene.h"

namespace damage
{
enum class Mode
{
    Dent = 0,
    Punch,
    DamageDecal,
};

struct MeshSettings
{
    Mode mode = Mode::Dent;
    float radius = 1.6f;
    float punchDepth = 2.4f;
    float punchInnerRadiusScale = 0.58f;
    float punchCoreRadiusScale = 0.42f;
    float decalRollVarianceDegrees = 18.0f;
    float decalSpreadDegrees = 3.5f;
    std::uint32_t decalBurstCount = 1;
};

bool ApplyMeshDamage(
    SceneData& scene,
    const TriangleMeshCollider::RayHit& hit,
    Vec3 shotDirection,
    const MeshSettings& settings
);
}
