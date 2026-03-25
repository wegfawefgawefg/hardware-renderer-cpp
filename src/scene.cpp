#include "scene.h"

#include <algorithm>

#include "scene_city.h"

namespace
{
Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 out = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    if (out.w != 0.0f && out.w != 1.0f)
    {
        float invW = 1.0f / out.w;
        return Vec3Make(out.x * invW, out.y * invW, out.z * invW);
    }
    return Vec3Make(out.x, out.y, out.z);
}
}

SceneBounds ComputeSceneBounds(const SceneData& scene)
{
    SceneBounds bounds{};

    for (const EntityData& entity : scene.entities)
    {
        if (entity.modelIndex >= scene.models.size())
        {
            continue;
        }

        const ModelData& model = scene.models[entity.modelIndex];
        for (const Vertex& vertex : model.mesh.vertices)
        {
            Vec3 p = TransformPoint(entity.transform, vertex.position);
            if (!bounds.valid)
            {
                bounds.valid = true;
                bounds.min = p;
                bounds.max = p;
                continue;
            }

            bounds.min.x = std::min(bounds.min.x, p.x);
            bounds.min.y = std::min(bounds.min.y, p.y);
            bounds.min.z = std::min(bounds.min.z, p.z);
            bounds.max.x = std::max(bounds.max.x, p.x);
            bounds.max.y = std::max(bounds.max.y, p.y);
            bounds.max.z = std::max(bounds.max.z, p.z);
        }
    }

    if (!bounds.valid)
    {
        return bounds;
    }

    bounds.center = Vec3Scale(Vec3Add(bounds.min, bounds.max), 0.5f);
    Vec3 extents = Vec3Scale(Vec3Sub(bounds.max, bounds.min), 0.5f);
    bounds.radius = std::max(Vec3Length(extents), 1.0f);
    return bounds;
}

std::uint32_t CountSceneTriangles(const SceneData& scene)
{
    std::uint32_t triangleCount = 0;
    for (const EntityData& entity : scene.entities)
    {
        if (entity.modelIndex >= scene.models.size())
        {
            continue;
        }

        const ModelData& model = scene.models[entity.modelIndex];
        triangleCount += static_cast<std::uint32_t>(model.mesh.indices.size() / 3);
    }
    return triangleCount;
}

SceneData LoadSampleScene(const AssetRegistry& assetRegistry)
{
    return BuildSampleCity(assetRegistry);
}
