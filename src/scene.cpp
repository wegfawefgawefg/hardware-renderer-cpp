#include "scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "assets/fbx_loader.h"
#include "assets/texture_loader.h"
#include "scene_city.h"

namespace
{
constexpr std::string_view kCharacterModelAsset = "kenney/animated-characters-1/Model/characterMedium.fbx";
constexpr std::string_view kCharacterTextureAsset = "kenney/animated-characters-1/Skins/survivorMaleB.png";

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

float ComputeModelFootprint(const ModelData& model)
{
    if (model.mesh.vertices.empty())
    {
        return 1.0f;
    }

    Vec3 mn = model.mesh.vertices.front().position;
    Vec3 mx = model.mesh.vertices.front().position;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        mn.x = std::min(mn.x, vertex.position.x);
        mn.z = std::min(mn.z, vertex.position.z);
        mx.x = std::max(mx.x, vertex.position.x);
        mx.z = std::max(mx.z, vertex.position.z);
    }

    return std::max(std::max(mx.x - mn.x, mx.z - mn.z), 0.001f);
}

Vec3 ComputeModelMin(const ModelData& model)
{
    if (model.mesh.vertices.empty())
    {
        return {};
    }

    Vec3 mn = model.mesh.vertices.front().position;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        mn.x = std::min(mn.x, vertex.position.x);
        mn.y = std::min(mn.y, vertex.position.y);
        mn.z = std::min(mn.z, vertex.position.z);
    }
    return mn;
}

SceneData BuildPlayerMaskTestScene(const AssetRegistry& assetRegistry)
{
    SceneData scene{};
    scene.models.push_back(MakeBoxModel(Vec3Make(18.0f, 0.25f, 18.0f), 220, 220, 220));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Translate(Vec3Make(0.0f, -0.25f, 0.0f)),
        .collidable = true,
    });

    const std::filesystem::path* characterPath = assetRegistry.FindByRelativePath(kCharacterModelAsset);
    if (characterPath == nullptr)
    {
        return scene;
    }

    ModelData characterModel = LoadFbxModel(characterPath->string());
    if (characterModel.mesh.vertices.empty() || characterModel.mesh.indices.empty())
    {
        return scene;
    }

    const std::filesystem::path* texturePath = assetRegistry.FindByRelativePath(kCharacterTextureAsset);
    if (texturePath != nullptr && !characterModel.textures.empty())
    {
        characterModel.textures[0] = LoadTexture(texturePath->string());
    }

    Vec3 modelMin = ComputeModelMin(characterModel);
    float footprint = ComputeModelFootprint(characterModel);
    float scale = 7.5f / std::max(footprint, 0.001f);
    std::uint32_t modelIndex = static_cast<std::uint32_t>(scene.models.size());
    scene.models.push_back(std::move(characterModel));
    scene.entities.push_back(EntityData{
        .modelIndex = modelIndex,
        .transform = Mat4Mul(
            Mat4Translate(Vec3Make(0.0f, -modelMin.y * scale, 0.0f)),
            Mat4Scale(scale)
        ),
        .assetPath = std::string(kCharacterModelAsset),
        .collidable = true,
    });

    return scene;
}

SceneData BuildShadowTestScene(const AssetRegistry& assetRegistry)
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

    const std::filesystem::path* lampPath = assetRegistry.FindByRelativePath(
        "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx"
    );
    if (lampPath != nullptr)
    {
        ModelData lampModel = LoadFbxModel(lampPath->string());
        if (!lampModel.mesh.vertices.empty() && !lampModel.mesh.indices.empty())
        {
            float footprint = ComputeModelFootprint(lampModel);
            float scale = 1.6f / footprint;
            std::uint32_t lampModelIndex = static_cast<std::uint32_t>(scene.models.size());
            scene.models.push_back(std::move(lampModel));
            scene.entities.push_back(EntityData{
                .modelIndex = lampModelIndex,
                .transform = Mat4Mul(
                    Mat4Translate(Vec3Make(-1.0f, 0.0f, 2.0f)),
                    Mat4Mul(Mat4RotateY(DegreesToRadians(0.0f)), Mat4Scale(scale))
                ),
                .collidable = false,
            });
        }
    }

    scene.spotLights.push_back(SpotLightData{
        .position = Vec3Make(-1.0f, 5.35f, 2.0f),
        .range = 16.0f,
        .direction = Vec3Normalize(Vec3Make(0.15f, -1.0f, -0.10f)),
        .innerCos = std::cos(DegreesToRadians(18.0f)),
        .color = Vec3Make(1.0f, 0.86f, 0.68f),
        .outerCos = std::cos(DegreesToRadians(30.0f)),
        .intensity = 4.0f,
        .yawDegrees = 0.0f,
    });

    return scene;
}

SceneData BuildSpotShadowTestScene(const AssetRegistry& assetRegistry)
{
    SceneData scene{};
    scene.models.push_back(MakeBoxModel(Vec3Make(24.0f, 0.25f, 24.0f), 235, 235, 235));
    scene.models.push_back(MakeBoxModel(Vec3Make(1.2f, 2.4f, 1.2f), 220, 120, 80));
    scene.models.push_back(MakeBoxModel(Vec3Make(0.8f, 1.2f, 2.0f), 80, 140, 220));

    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Translate(Vec3Make(0.0f, -0.25f, 0.0f)),
        .collidable = true,
    });

    for (int z = -1; z <= 1; ++z)
    {
        for (int x = -1; x <= 1; ++x)
        {
            Vec3 lampBase = Vec3Make(static_cast<float>(x) * 10.0f, 0.0f, static_cast<float>(z) * 10.0f);
            scene.entities.push_back(EntityData{
                .modelIndex = ((x + z) & 1) == 0 ? 1u : 2u,
                .transform = Mat4Translate(Vec3Add(lampBase, Vec3Make(1.6f, (((x + z) & 1) == 0) ? 2.4f : 1.2f, -1.4f))),
                .collidable = true,
            });
        }
    }

    const std::filesystem::path* lampPath = assetRegistry.FindByRelativePath(
        "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx"
    );
    if (lampPath != nullptr)
    {
        ModelData lampModel = LoadFbxModel(lampPath->string());
        if (!lampModel.mesh.vertices.empty() && !lampModel.mesh.indices.empty())
        {
            float footprint = ComputeModelFootprint(lampModel);
            float scale = 1.6f / footprint;
            std::uint32_t lampModelIndex = static_cast<std::uint32_t>(scene.models.size());
            scene.models.push_back(std::move(lampModel));

            for (int z = -1; z <= 1; ++z)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    Vec3 lampBase = Vec3Make(static_cast<float>(x) * 10.0f, 0.0f, static_cast<float>(z) * 10.0f);
                    scene.entities.push_back(EntityData{
                        .modelIndex = lampModelIndex,
                        .transform = Mat4Mul(Mat4Translate(lampBase), Mat4Scale(scale)),
                        .collidable = false,
                    });
                    scene.spotLights.push_back(SpotLightData{
                        .position = Vec3Add(lampBase, Vec3Make(0.0f, 5.35f, 0.0f)),
                        .range = 14.0f,
                        .direction = Vec3Normalize(Vec3Make(-0.062f, -5.350f, 1.303f)),
                        .innerCos = std::cos(DegreesToRadians(18.0f)),
                        .color = Vec3Make(1.0f, 0.86f, 0.68f),
                        .outerCos = std::cos(DegreesToRadians(30.0f)),
                        .intensity = 4.0f,
                        .yawDegrees = 0.0f,
                    });
                }
            }
        }
    }

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
    if (kind == SceneKind::PlayerMaskTest)
    {
        return BuildPlayerMaskTestScene(assetRegistry);
    }
    if (kind == SceneKind::ShadowTest)
    {
        return BuildShadowTestScene(assetRegistry);
    }
    if (kind == SceneKind::SpotShadowTest)
    {
        return BuildSpotShadowTestScene(assetRegistry);
    }
    if (kind == SceneKind::VehicleLightTest)
    {
        return BuildVehicleLightTestScene(assetRegistry);
    }
    return BuildSampleCity(assetRegistry);
}
