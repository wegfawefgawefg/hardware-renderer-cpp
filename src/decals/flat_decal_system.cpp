#include "decals/flat_decal_system.h"

#include <cmath>

namespace decals
{
namespace
{
Vec3 RotateAroundAxis(Vec3 v, Vec3 axis, float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Add(
        Vec3Add(Vec3Scale(v, c), Vec3Scale(Vec3Cross(axis, v), s)),
        Vec3Scale(axis, Vec3Dot(axis, v) * (1.0f - c)));
}

Mat4 BuildFlatDecalTransform(Vec3 position, Vec3 normal, float size, float rollRadians)
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
}

void ResetFlatDecalTemplates(FlatDecalSystem& system)
{
    system.templates = {};
    system.templateCount = 0;
}

void ClearFlatDecals(FlatDecalSystem& system)
{
    system.instances = {};
    system.nextInstance = 0;
}

FlatDecalTemplateId RegisterFlatDecalTemplate(FlatDecalSystem& system, FlatDecalTemplate decalTemplate)
{
    if (system.templateCount >= system.kMaxTemplates)
    {
        return kInvalidFlatDecalTemplateId;
    }

    FlatDecalTemplateId id = static_cast<FlatDecalTemplateId>(system.templateCount);
    system.templates[id] = std::move(decalTemplate);
    ++system.templateCount;
    return id;
}

bool SpawnFlatDecal(
    FlatDecalSystem& system,
    FlatDecalTemplateId templateId,
    Vec3 hitPosition,
    Vec3 hitNormal,
    float size,
    float rollRadians)
{
    if (templateId >= system.templateCount)
    {
        return false;
    }

    std::uint32_t slot = system.nextInstance % system.kMaxInstances;
    system.nextInstance = (slot + 1u) % system.kMaxInstances;
    FlatDecalInstance& instance = system.instances[slot];
    Vec3 decalPosition = Vec3Add(hitPosition, Vec3Scale(hitNormal, std::max(0.01f, size * 0.06f)));
    instance.transform = BuildFlatDecalTransform(decalPosition, hitNormal, size * 2.0f, rollRadians);
    instance.templateId = templateId;
    instance.active = true;
    return true;
}
}
