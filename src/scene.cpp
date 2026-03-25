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

TextureData MakeSolidTextureData(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    TextureData texture{};
    texture.width = 1;
    texture.height = 1;
    texture.pixels = {r, g, b, 255};
    return texture;
}

void AppendFace(
    ModelData& model,
    Vec3 a,
    Vec3 b,
    Vec3 c,
    Vec3 d,
    Vec3 normal
)
{
    std::uint32_t base = static_cast<std::uint32_t>(model.mesh.vertices.size());
    model.mesh.vertices.push_back(Vertex{.position = a, .normal = normal, .uv = Vec2Make(0.0f, 0.0f)});
    model.mesh.vertices.push_back(Vertex{.position = b, .normal = normal, .uv = Vec2Make(1.0f, 0.0f)});
    model.mesh.vertices.push_back(Vertex{.position = c, .normal = normal, .uv = Vec2Make(1.0f, 1.0f)});
    model.mesh.vertices.push_back(Vertex{.position = d, .normal = normal, .uv = Vec2Make(0.0f, 1.0f)});
    model.mesh.indices.insert(model.mesh.indices.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
}

ModelData MakeBoxModel(Vec3 halfExtents, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    ModelData model{};
    model.textures.push_back(MakeSolidTextureData(r, g, b));
    model.materials.push_back(MaterialData{.name = "solid", .textureIndex = 0});

    float x = halfExtents.x;
    float y = halfExtents.y;
    float z = halfExtents.z;
    AppendFace(model, Vec3Make(-x, -y, z), Vec3Make(x, -y, z), Vec3Make(x, y, z), Vec3Make(-x, y, z), Vec3Make(0.0f, 0.0f, 1.0f));
    AppendFace(model, Vec3Make(x, -y, -z), Vec3Make(-x, -y, -z), Vec3Make(-x, y, -z), Vec3Make(x, y, -z), Vec3Make(0.0f, 0.0f, -1.0f));
    AppendFace(model, Vec3Make(-x, -y, -z), Vec3Make(-x, -y, z), Vec3Make(-x, y, z), Vec3Make(-x, y, -z), Vec3Make(-1.0f, 0.0f, 0.0f));
    AppendFace(model, Vec3Make(x, -y, z), Vec3Make(x, -y, -z), Vec3Make(x, y, -z), Vec3Make(x, y, z), Vec3Make(1.0f, 0.0f, 0.0f));
    AppendFace(model, Vec3Make(-x, y, z), Vec3Make(x, y, z), Vec3Make(x, y, -z), Vec3Make(-x, y, -z), Vec3Make(0.0f, 1.0f, 0.0f));
    AppendFace(model, Vec3Make(-x, -y, -z), Vec3Make(x, -y, -z), Vec3Make(x, -y, z), Vec3Make(-x, -y, z), Vec3Make(0.0f, -1.0f, 0.0f));

    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

SceneData BuildShadowTestScene()
{
    SceneData scene{};
    scene.models.push_back(MakeBoxModel(Vec3Make(24.0f, 0.25f, 24.0f), 235, 235, 235));
    scene.models.push_back(MakeBoxModel(Vec3Make(1.25f, 3.0f, 1.25f), 220, 120, 80));
    scene.models.push_back(MakeBoxModel(Vec3Make(1.0f, 1.0f, 1.0f), 80, 140, 220));

    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Translate(Vec3Make(0.0f, -0.25f, 0.0f)),
        .collidable = true,
    });
    scene.entities.push_back(EntityData{
        .modelIndex = 1,
        .transform = Mat4Translate(Vec3Make(-3.5f, 3.0f, 0.0f)),
        .collidable = true,
    });
    scene.entities.push_back(EntityData{
        .modelIndex = 2,
        .transform = Mat4Translate(Vec3Make(2.5f, 1.0f, 2.5f)),
        .collidable = true,
    });

    return scene;
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

SceneData LoadSampleScene(const AssetRegistry& assetRegistry, SceneKind kind)
{
    if (kind == SceneKind::ShadowTest)
    {
        (void)assetRegistry;
        return BuildShadowTestScene();
    }
    return BuildSampleCity(assetRegistry);
}
