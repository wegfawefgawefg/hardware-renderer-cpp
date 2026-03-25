#include "scene.h"

#include <algorithm>
#include <filesystem>

#include "assets/gltf_loader.h"
#include "assets/obj_loader.h"
#include "assets/texture_loader.h"

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

void AddGroundPlane(SceneData& scene, Vec3 center, float y, float extent)
{
    ModelData ground{};
    ground.materials.push_back(MaterialData{
        .name = "ground",
        .textureIndex = 0,
    });
    ground.textures.push_back(MakeSolidTexture(110, 116, 128, 255));

    auto pushVertex = [&](float x, float z, float u, float v) {
        Vertex vertex{};
        vertex.position = Vec3Make(x, y, z);
        vertex.normal = Vec3Make(0.0f, 1.0f, 0.0f);
        vertex.uv = Vec2Make(u, v);
        ground.mesh.vertices.push_back(vertex);
    };

    float x0 = center.x - extent;
    float x1 = center.x + extent;
    float z0 = center.z - extent;
    float z1 = center.z + extent;
    pushVertex(x0, z0, 0.0f, 0.0f);
    pushVertex(x1, z0, 1.0f, 0.0f);
    pushVertex(x1, z1, 1.0f, 1.0f);
    pushVertex(x0, z1, 0.0f, 1.0f);

    ground.mesh.indices = {0, 1, 2, 0, 2, 3};
    ground.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = 6,
        .materialIndex = 0,
    });

    std::uint32_t modelIndex = static_cast<std::uint32_t>(scene.models.size());
    scene.models.push_back(std::move(ground));
    scene.entities.push_back(EntityData{
        .modelIndex = modelIndex,
        .transform = Mat4Identity(),
    });
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

SceneData LoadSampleScene()
{
    std::filesystem::path marioScenePath = HARDWARE_RENDERER_MARIO_SCENE_PATH;
    if (std::filesystem::exists(marioScenePath))
    {
        SceneData scene = LoadGltfScene(marioScenePath.string());
        SceneBounds bounds = ComputeSceneBounds(scene);
        Vec3 center = bounds.valid ? bounds.center : Vec3Make(0.0f, 0.0f, 0.0f);
        float extent = bounds.valid ? std::max(bounds.radius * 2.5f, 12.0f) : 12.0f;
        float groundY = bounds.valid ? bounds.min.y - 0.02f : -0.02f;
        AddGroundPlane(scene, center, groundY, extent);
        return scene;
    }

    SceneData scene{};
    scene.models.push_back(LoadObjModel(HARDWARE_RENDERER_SAMPLE_OBJ_PATH));

    EntityData entity{};
    entity.modelIndex = 0;
    entity.transform = Mat4Identity();
    scene.entities.push_back(entity);

    SceneBounds bounds = ComputeSceneBounds(scene);
    Vec3 center = bounds.valid ? bounds.center : Vec3Make(0.0f, 0.0f, 0.0f);
    float extent = bounds.valid ? std::max(bounds.radius * 2.5f, 8.0f) : 8.0f;
    float groundY = bounds.valid ? bounds.min.y - 0.02f : -0.02f;
    AddGroundPlane(scene, center, groundY, extent);

    return scene;
}
