#include "damage/mesh_damage.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace damage
{
namespace
{
struct LocalTransform
{
    Vec3 translation = {};
    float scale = 1.0f;
};

LocalTransform ExtractLocalTransform(Mat4 m)
{
    LocalTransform result{};
    result.translation = Vec3Make(m.m[12], m.m[13], m.m[14]);
    Vec3 xAxis = Vec3Make(m.m[0], m.m[1], m.m[2]);
    result.scale = std::max(Vec3Length(xAxis), 1e-5f);
    return result;
}

void EnsureEntityOwnsModel(SceneData& scene, std::uint32_t entityIndex)
{
    if (entityIndex >= scene.entities.size())
    {
        return;
    }

    EntityData& entity = scene.entities[entityIndex];
    if (entity.modelIndex >= scene.models.size())
    {
        return;
    }

    std::uint32_t sharedCount = 0;
    for (const EntityData& candidate : scene.entities)
    {
        if (candidate.modelIndex == entity.modelIndex)
        {
            ++sharedCount;
            if (sharedCount > 1)
            {
                break;
            }
        }
    }

    if (sharedCount <= 1)
    {
        return;
    }

    scene.models.push_back(scene.models[entity.modelIndex]);
    entity.modelIndex = static_cast<std::uint32_t>(scene.models.size() - 1);
}

Vec3 LocalToWorld(const LocalTransform& transform, Vec3 p)
{
    return Vec3Add(transform.translation, Vec3Scale(p, transform.scale));
}

Vec3 ClosestPointOnTriangle(Vec3 p, Vec3 a, Vec3 b, Vec3 c)
{
    Vec3 ab = Vec3Sub(b, a);
    Vec3 ac = Vec3Sub(c, a);
    Vec3 ap = Vec3Sub(p, a);
    float d1 = Vec3Dot(ab, ap);
    float d2 = Vec3Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
    {
        return a;
    }

    Vec3 bp = Vec3Sub(p, b);
    float d3 = Vec3Dot(ab, bp);
    float d4 = Vec3Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
    {
        return b;
    }

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return Vec3Add(a, Vec3Scale(ab, v));
    }

    Vec3 cp = Vec3Sub(p, c);
    float d5 = Vec3Dot(ab, cp);
    float d6 = Vec3Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
    {
        return c;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return Vec3Add(a, Vec3Scale(ac, w));
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        Vec3 bc = Vec3Sub(c, b);
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return Vec3Add(b, Vec3Scale(bc, w));
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return Vec3Add(a, Vec3Add(Vec3Scale(ab, v), Vec3Scale(ac, w)));
}

bool TriangleHitsSphere(Vec3 a, Vec3 b, Vec3 c, Vec3 center, float radius)
{
    Vec3 closest = ClosestPointOnTriangle(center, a, b, c);
    Vec3 delta = Vec3Sub(closest, center);
    return Vec3Dot(delta, delta) <= radius * radius;
}

float TriangleArea(Vec3 a, Vec3 b, Vec3 c)
{
    return 0.5f * Vec3Length(Vec3Cross(Vec3Sub(b, a), Vec3Sub(c, a)));
}

void RecomputeNormals(ModelData& model)
{
    for (Vertex& vertex : model.mesh.vertices)
    {
        vertex.normal = Vec3Make(0.0f, 0.0f, 0.0f);
    }

    for (std::size_t i = 0; i + 2 < model.mesh.indices.size(); i += 3)
    {
        std::uint32_t i0 = model.mesh.indices[i + 0];
        std::uint32_t i1 = model.mesh.indices[i + 1];
        std::uint32_t i2 = model.mesh.indices[i + 2];
        if (i0 >= model.mesh.vertices.size() || i1 >= model.mesh.vertices.size() || i2 >= model.mesh.vertices.size())
        {
            continue;
        }

        const Vec3 a = model.mesh.vertices[i0].position;
        const Vec3 b = model.mesh.vertices[i1].position;
        const Vec3 c = model.mesh.vertices[i2].position;
        Vec3 n = Vec3Cross(Vec3Sub(b, a), Vec3Sub(c, a));
        if (Vec3Length(n) <= 1e-6f)
        {
            continue;
        }
        model.mesh.vertices[i0].normal = Vec3Add(model.mesh.vertices[i0].normal, n);
        model.mesh.vertices[i1].normal = Vec3Add(model.mesh.vertices[i1].normal, n);
        model.mesh.vertices[i2].normal = Vec3Add(model.mesh.vertices[i2].normal, n);
    }

    for (Vertex& vertex : model.mesh.vertices)
    {
        if (Vec3Length(vertex.normal) <= 1e-6f)
        {
            vertex.normal = Vec3Make(0.0f, 1.0f, 0.0f);
        }
        else
        {
            vertex.normal = Vec3Normalize(vertex.normal);
        }
    }
}

void RebuildPrimitiveRanges(ModelData& model, const std::vector<bool>& deleteTri)
{
    std::vector<std::uint32_t> rebuiltIndices;
    rebuiltIndices.reserve(model.mesh.indices.size());
    std::size_t triIndex = 0;
    for (PrimitiveData& primitive : model.primitives)
    {
        std::uint32_t firstIndex = static_cast<std::uint32_t>(rebuiltIndices.size());
        for (std::uint32_t local = 0; local + 2 < primitive.indexCount; local += 3, ++triIndex)
        {
            std::uint32_t base = primitive.firstIndex + local;
            if (base + 2 >= model.mesh.indices.size() || triIndex >= deleteTri.size())
            {
                continue;
            }
            if (deleteTri[triIndex])
            {
                continue;
            }

            std::uint32_t i0 = model.mesh.indices[base + 0];
            std::uint32_t i1 = model.mesh.indices[base + 1];
            std::uint32_t i2 = model.mesh.indices[base + 2];
            if (i0 >= model.mesh.vertices.size() || i1 >= model.mesh.vertices.size() || i2 >= model.mesh.vertices.size())
            {
                continue;
            }

            const Vec3 a = model.mesh.vertices[i0].position;
            const Vec3 b = model.mesh.vertices[i1].position;
            const Vec3 c = model.mesh.vertices[i2].position;
            if (TriangleArea(a, b, c) < 0.0005f)
            {
                continue;
            }

            rebuiltIndices.push_back(i0);
            rebuiltIndices.push_back(i1);
            rebuiltIndices.push_back(i2);
        }
        primitive.firstIndex = firstIndex;
        primitive.indexCount = static_cast<std::uint32_t>(rebuiltIndices.size()) - firstIndex;
    }
    model.mesh.indices = std::move(rebuiltIndices);
}

bool ApplyDent(
    ModelData& model,
    const LocalTransform& transform,
    Vec3 hitPoint,
    Vec3 shotDirection,
    float radius,
    float depth)
{
    bool changed = false;
    Vec3 shotDir = Vec3Normalize(shotDirection);
    float invScale = 1.0f / transform.scale;
    Vec3 dentCenter = Vec3Sub(hitPoint, Vec3Scale(shotDir, radius * 0.35f));
    for (Vertex& vertex : model.mesh.vertices)
    {
        Vec3 worldPos = LocalToWorld(transform, vertex.position);
        Vec3 fromCenter = Vec3Sub(worldPos, dentCenter);
        float dist = Vec3Length(fromCenter);
        if (dist > radius)
        {
            continue;
        }

        if (dist <= 1e-5f)
        {
            dist = 1e-5f;
        }

        float falloff = 1.0f - std::clamp(dist / std::max(radius, 1e-5f), 0.0f, 1.0f);
        float displacement = depth * falloff;
        Vec3 pushDir = Vec3Scale(fromCenter, 1.0f / dist);
        vertex.position = Vec3Add(vertex.position, Vec3Scale(pushDir, displacement * invScale));
        changed = true;
    }

    if (!changed)
    {
        return false;
    }
    RecomputeNormals(model);
    return true;
}

bool ApplyPunch(
    ModelData& model,
    const LocalTransform& transform,
    Vec3 hitPoint,
    Vec3 shotDirection,
    float radius,
    float depth,
    float innerRadiusScale,
    float coreRadiusScale)
{
    const float clampedInnerScale = std::clamp(innerRadiusScale, 0.05f, 0.95f);
    const float clampedCoreScale = std::clamp(coreRadiusScale, 0.02f, clampedInnerScale - 0.01f);
    const float innerRadius = std::max(radius * clampedInnerScale, 1e-4f);
    const float coreRadius = std::max(radius * clampedCoreScale, 1e-4f);
    bool changed = false;

    changed |= ApplyDent(model, transform, hitPoint, shotDirection, radius, depth);
    changed |= ApplyDent(model, transform, hitPoint, shotDirection, innerRadius, innerRadius);

    std::vector<bool> deleteTri(model.mesh.indices.size() / 3, false);
    std::size_t triIndex = 0;
    bool deletedAnyTriangles = false;
    for (const PrimitiveData& primitive : model.primitives)
    {
        for (std::uint32_t local = 0; local + 2 < primitive.indexCount; local += 3, ++triIndex)
        {
            std::uint32_t base = primitive.firstIndex + local;
            if (base + 2 >= model.mesh.indices.size())
            {
                continue;
            }

            const Vertex& v0 = model.mesh.vertices[model.mesh.indices[base + 0]];
            const Vertex& v1 = model.mesh.vertices[model.mesh.indices[base + 1]];
            const Vertex& v2 = model.mesh.vertices[model.mesh.indices[base + 2]];
            Vec3 a = LocalToWorld(transform, v0.position);
            Vec3 b = LocalToWorld(transform, v1.position);
            Vec3 c = LocalToWorld(transform, v2.position);
            if (!TriangleHitsSphere(a, b, c, hitPoint, coreRadius))
            {
                continue;
            }

            deleteTri[triIndex] = true;
            deletedAnyTriangles = true;
        }
    }

    if (deletedAnyTriangles)
    {
        RebuildPrimitiveRanges(model, deleteTri);
        changed = true;
    }

    if (!changed)
    {
        return false;
    }

    RecomputeNormals(model);
    return true;
}
}

bool ApplyMeshDamage(
    SceneData& scene,
    const TriangleMeshCollider::RayHit& hit,
    Vec3 shotDirection,
    const MeshSettings& settings)
{
    if (!hit.hit || hit.entityIndex >= scene.entities.size())
    {
        return false;
    }

    EntityData& entity = scene.entities[hit.entityIndex];
    if (entity.modelIndex >= scene.models.size())
    {
        return false;
    }

    EnsureEntityOwnsModel(scene, hit.entityIndex);
    ModelData& model = scene.models[entity.modelIndex];
    LocalTransform transform = ExtractLocalTransform(entity.transform);
    Vec3 shotDir = Vec3Normalize(shotDirection);
    Vec3 inwardFromSurface = Vec3Normalize(Vec3Scale(hit.normal, -1.0f));
    Vec3 punchDirection = shotDir;
    if (Vec3Dot(punchDirection, inwardFromSurface) < 0.0f)
    {
        punchDirection = Vec3Scale(punchDirection, -1.0f);
    }
    float dentDepth = settings.punchDepth <= 0.0f ? settings.radius : settings.punchDepth;

    switch (settings.mode)
    {
    case Mode::Dent:
        return ApplyDent(model, transform, hit.position, punchDirection, settings.radius, dentDepth);
    case Mode::Punch:
        return ApplyPunch(
            model,
            transform,
            hit.position,
            punchDirection,
            settings.radius,
            settings.radius,
            settings.punchInnerRadiusScale,
            settings.punchCoreRadiusScale);
    case Mode::DamageDecal:
        return false;
    }

    return false;
}
}
