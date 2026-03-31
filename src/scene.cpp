#include "scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "assets/fbx_loader.h"
#include "assets/gltf_loader.h"
#include "assets/texture_loader.h"
#include "scene_city.h"

namespace
{
constexpr std::string_view kCharacterModelAsset = "kenney/animated-characters-1/Model/characterMedium.fbx";
constexpr std::string_view kCharacterTextureAsset = "kenney/animated-characters-1/Skins/survivorMaleB.png";
constexpr std::string_view kSponzaGltfAsset = "sponza_optimized/Sponza.gltf";
constexpr std::string_view kDragonGltfAsset = "dragon_attenuation/DragonAttenuation.gltf";

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

ModelData MakePlaneModel(float halfExtent, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    ModelData model{};
    model.textures.push_back(MakeSolidTextureData(r, g, b));
    model.materials.push_back(MaterialData{.name = "solid", .textureIndex = 0});

    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(-halfExtent, 0.0f, -halfExtent),
        .normal = Vec3Make(0.0f, 1.0f, 0.0f),
        .uv = Vec2Make(0.0f, 0.0f),
    });
    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(halfExtent, 0.0f, -halfExtent),
        .normal = Vec3Make(0.0f, 1.0f, 0.0f),
        .uv = Vec2Make(1.0f, 0.0f),
    });
    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(halfExtent, 0.0f, halfExtent),
        .normal = Vec3Make(0.0f, 1.0f, 0.0f),
        .uv = Vec2Make(1.0f, 1.0f),
    });
    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(-halfExtent, 0.0f, halfExtent),
        .normal = Vec3Make(0.0f, 1.0f, 0.0f),
        .uv = Vec2Make(0.0f, 1.0f),
    });
    model.mesh.indices.insert(model.mesh.indices.end(), {0, 2, 1, 0, 3, 2});
    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

ModelData MakeVerticalQuadModel(float halfWidth, float halfHeight, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    ModelData model{};
    model.textures.push_back(MakeSolidTextureData(r, g, b));
    model.materials.push_back(MaterialData{.name = "solid", .textureIndex = 0, .flipNormalY = false});

    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(-halfWidth, -halfHeight, 0.0f),
        .normal = Vec3Make(0.0f, 0.0f, -1.0f),
        .uv = Vec2Make(0.0f, 0.0f),
    });
    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(halfWidth, -halfHeight, 0.0f),
        .normal = Vec3Make(0.0f, 0.0f, -1.0f),
        .uv = Vec2Make(1.0f, 0.0f),
    });
    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(halfWidth, halfHeight, 0.0f),
        .normal = Vec3Make(0.0f, 0.0f, -1.0f),
        .uv = Vec2Make(1.0f, 1.0f),
    });
    model.mesh.vertices.push_back(Vertex{
        .position = Vec3Make(-halfWidth, halfHeight, 0.0f),
        .normal = Vec3Make(0.0f, 0.0f, -1.0f),
        .uv = Vec2Make(0.0f, 1.0f),
    });
    model.mesh.indices.insert(model.mesh.indices.end(), {0, 2, 1, 0, 3, 2});
    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

ModelData MakeUvSphereModel(
    float radius,
    std::uint32_t longitudeSegments,
    std::uint32_t latitudeSegments,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b)
{
    ModelData model{};
    model.textures.push_back(MakeSolidTextureData(r, g, b));
    model.materials.push_back(MaterialData{.name = "solid", .textureIndex = 0, .flipNormalY = false});

    longitudeSegments = std::max<std::uint32_t>(longitudeSegments, 3u);
    latitudeSegments = std::max<std::uint32_t>(latitudeSegments, 2u);

    for (std::uint32_t lat = 0; lat <= latitudeSegments; ++lat)
    {
        float v = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
        float theta = v * 3.14159265359f;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);
        for (std::uint32_t lon = 0; lon <= longitudeSegments; ++lon)
        {
            float u = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
            float phi = u * 6.28318530718f;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            Vec3 normal = Vec3Make(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
            model.mesh.vertices.push_back(Vertex{
                .position = Vec3Scale(normal, radius),
                .normal = normal,
                .uv = Vec2Make(u, v),
            });
        }
    }

    const std::uint32_t stride = longitudeSegments + 1u;
    for (std::uint32_t lat = 0; lat < latitudeSegments; ++lat)
    {
        for (std::uint32_t lon = 0; lon < longitudeSegments; ++lon)
        {
            std::uint32_t i0 = lat * stride + lon;
            std::uint32_t i1 = i0 + 1u;
            std::uint32_t i2 = i0 + stride;
            std::uint32_t i3 = i2 + 1u;
            if (lat != 0u)
            {
                model.mesh.indices.insert(model.mesh.indices.end(), {i0, i1, i2});
            }
            if (lat + 1u != latitudeSegments)
            {
                model.mesh.indices.insert(model.mesh.indices.end(), {i1, i3, i2});
            }
        }
    }

    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

ModelData MakeVirtualGeomCubeModel(float halfExtent, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    ModelData model = MakeBoxModel(Vec3Make(halfExtent, halfExtent, halfExtent), r, g, b);
    model.materials[0].flipNormalY = false;
    return model;
}

float ComputeModelMinY(const ModelData& model, Mat4 transform)
{
    if (model.mesh.vertices.empty())
    {
        return 0.0f;
    }
    float minY = 1e30f;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        minY = std::min(minY, TransformPoint(transform, vertex.position).y);
    }
    return minY;
}

float ComputeModelFootprintTransformed(const ModelData& model, Mat4 transform)
{
    if (model.mesh.vertices.empty())
    {
        return 1.0f;
    }

    Vec3 mn = TransformPoint(transform, model.mesh.vertices.front().position);
    Vec3 mx = mn;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        Vec3 p = TransformPoint(transform, vertex.position);
        mn.x = std::min(mn.x, p.x);
        mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x);
        mx.z = std::max(mx.z, p.z);
    }
    return std::max(std::max(mx.x - mn.x, mx.z - mn.z), 0.001f);
}

struct VirtualGeomSource
{
    ModelData model;
    Mat4 baseTransform = Mat4Identity();
    std::string assetPath;
};

ModelData MakeVirtualGeomAggregateModel(const ModelData& sourceModel, std::uint32_t instanceCount)
{
    ModelData model{};
    TextureData palette{};
    palette.width = 256;
    palette.height = 1;
    palette.pixels.resize(static_cast<std::size_t>(palette.width) * 4u);
    for (std::uint32_t i = 0; i < palette.width; ++i)
    {
        std::uint8_t r = 214;
        std::uint8_t g = 176;
        std::uint8_t b = 104;
        if (i > 0u)
        {
            float t = std::fmod(static_cast<float>(i - 1u) * 0.6180339f, 1.0f);
            float a = t * 6.28318530718f;
            auto channel = [a](float phase) -> std::uint8_t {
                float v = 0.35f + 0.55f * (0.5f + 0.5f * std::sin(a + phase));
                return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
            };
            r = channel(0.0f);
            g = channel(2.0943951f);
            b = channel(4.1887902f);
        }
        const std::size_t base = static_cast<std::size_t>(i) * 4u;
        palette.pixels[base + 0u] = r;
        palette.pixels[base + 1u] = g;
        palette.pixels[base + 2u] = b;
        palette.pixels[base + 3u] = 255;
    }
    model.textures.push_back(std::move(palette));
    model.materials.push_back(MaterialData{.name = "virtual_geom_palette", .textureIndex = 0, .flipNormalY = false});

    const std::uint32_t sourceTriangleCount = static_cast<std::uint32_t>(sourceModel.mesh.indices.size() / 3u);
    const std::uint32_t maxTriangleCount = std::max(1u, sourceTriangleCount * std::max(1u, instanceCount));
    model.mesh.vertices.resize(static_cast<std::size_t>(maxTriangleCount) * 3u);
    model.mesh.indices.resize(static_cast<std::size_t>(maxTriangleCount) * 3u);
    for (std::uint32_t i = 0; i < maxTriangleCount * 3u; ++i)
    {
        model.mesh.indices[static_cast<std::size_t>(i)] = i;
    }
    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

float ComputeModelFootprint(const ModelData& model);
Vec3 ComputeModelMin(const ModelData& model);

void ForceSolidBenchmarkMaterial(ModelData& model, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    model.textures.clear();
    model.textures.push_back(MakeSolidTextureData(r, g, b));
    if (model.materials.empty())
    {
        model.materials.push_back(MaterialData{.name = "benchmark_solid", .textureIndex = 0, .flipNormalY = false});
    }
    for (MaterialData& material : model.materials)
    {
        material.textureIndex = 0;
        material.normalTextureIndex = -1;
        material.flipNormalY = false;
        material.leanShading = false;
    }
}

VirtualGeomSource MakeVirtualGeomSource(const AssetRegistry& assetRegistry, const VirtualGeomSceneConfig& config)
{
    if (config.meshKind == VirtualGeomMeshKind::Dragon)
    {
        const std::filesystem::path* dragonPath = assetRegistry.FindByRelativePath(kDragonGltfAsset);
        if (dragonPath != nullptr)
        {
            SceneData scene = LoadGltfScene(dragonPath->string());
            if (scene.models.size() >= 2 && scene.entities.size() >= 2)
            {
                VirtualGeomSource source{};
                source.model = scene.models[1];
                source.assetPath = std::string(kDragonGltfAsset);
                source.baseTransform = Mat4Identity();
                ForceSolidBenchmarkMaterial(source.model, 214, 176, 104);
                float footprint = ComputeModelFootprint(source.model);
                float scale = 8.0f / std::max(footprint, 0.001f);
                source.baseTransform = Mat4Scale(scale);
                return source;
            }
        }
    }

    VirtualGeomSource source{};
    source.model = config.meshKind == VirtualGeomMeshKind::Cube
        ? MakeVirtualGeomCubeModel(3.0f, 220, 220, 220)
        : MakeUvSphereModel(
            3.0f,
            config.sphereLongitudeSegments,
            config.sphereLatitudeSegments,
            220,
            220,
            220);
    source.assetPath = config.meshKind == VirtualGeomMeshKind::Cube
        ? "generated/virtual_geom_cube"
        : "generated/virtual_geom_subject";
    return source;
}

SceneData BuildLightTileTestScene(const AssetRegistry&)
{
    SceneData scene{};
    scene.models.push_back(MakeVerticalQuadModel(16.0f, 12.0f, 220, 220, 220));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Translate(Vec3Make(0.0f, 4.0f, 10.0f)),
        .assetPath = "generated/light_tile_test_quad",
        .collidable = false,
    });
    return scene;
}

float ComputeModelFootprint(const ModelData& model);
Vec3 ComputeModelMin(const ModelData& model);

SceneData BuildManyLightsScene(const AssetRegistry& assetRegistry, ManyLightsHeroModel heroModel)
{
    if (heroModel == ManyLightsHeroModel::Sponza)
    {
        const std::filesystem::path* sponzaPath = assetRegistry.FindByRelativePath(kSponzaGltfAsset);
        if (sponzaPath != nullptr)
        {
            SceneData scene = LoadGltfScene(sponzaPath->string());
            for (EntityData& entity : scene.entities)
            {
                entity.collidable = false;
                if (entity.assetPath.empty())
                {
                    entity.assetPath = std::string(kSponzaGltfAsset);
                }
            }
            return scene;
        }
    }

    SceneData scene{};
    scene.models.push_back(MakePlaneModel(32.0f, 48, 48, 48));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Identity(),
        .assetPath = "generated/many_lights_floor",
        .collidable = false,
    });

    const std::filesystem::path* characterPath = assetRegistry.FindByRelativePath(kCharacterModelAsset);
    if (characterPath != nullptr)
    {
        ModelData characterModel = LoadFbxModel(characterPath->string());
        if (!characterModel.mesh.vertices.empty() && !characterModel.mesh.indices.empty())
        {
            const std::filesystem::path* texturePath = assetRegistry.FindByRelativePath(kCharacterTextureAsset);
            if (texturePath != nullptr)
            {
                TextureData skinTexture = LoadTexture(texturePath->string());
                if (characterModel.textures.empty())
                {
                    characterModel.textures.push_back(skinTexture);
                    if (characterModel.materials.empty())
                    {
                        characterModel.materials.push_back(MaterialData{.name = "skin", .textureIndex = 0});
                    }
                    else
                    {
                        for (MaterialData& material : characterModel.materials)
                        {
                            material.textureIndex = 0;
                        }
                    }
                }
                else
                {
                    for (TextureData& texture : characterModel.textures)
                    {
                        texture = skinTexture;
                    }
                }
            }

            Vec3 modelMin = ComputeModelMin(characterModel);
            float footprint = ComputeModelFootprint(characterModel);
            float scale = 8.0f / std::max(footprint, 0.001f);
            std::uint32_t modelIndex = static_cast<std::uint32_t>(scene.models.size());
            scene.models.push_back(std::move(characterModel));
            scene.entities.push_back(EntityData{
                .modelIndex = modelIndex,
                .transform = Mat4Mul(
                    Mat4Translate(Vec3Make(0.0f, -modelMin.y * scale, 10.0f)),
                    Mat4Scale(scale)
                ),
                .assetPath = std::string(kCharacterModelAsset),
                .collidable = false,
            });
            return scene;
        }
    }

    scene.models.push_back(MakeBoxModel(Vec3Make(2.0f, 4.0f, 2.0f), 220, 220, 220));
    scene.entities.push_back(EntityData{
        .modelIndex = 1,
        .transform = Mat4Translate(Vec3Make(0.0f, 4.0f, 10.0f)),
        .assetPath = "generated/many_lights_hero_box",
        .collidable = false,
    });
    return scene;
}

SceneData MakeVirtualGeomTestScene(const AssetRegistry& assetRegistry, const VirtualGeomSceneConfig& config)
{
    SceneData scene{};
    VirtualGeomSource source = MakeVirtualGeomSource(assetRegistry, config);
    float gridWidth = std::max(1.0f, static_cast<float>(std::max(1u, config.gridCountX) - 1u) * config.gridSpacing);
    float gridDepth = std::max(1.0f, static_cast<float>(std::max(1u, config.gridCountZ) - 1u) * config.gridSpacing);
    float planeHalfExtent = std::max(12.0f, std::max(gridWidth, gridDepth) * 0.75f + 8.0f);
    scene.models.push_back(std::move(source.model));
    scene.models.push_back(MakePlaneModel(planeHalfExtent, 52, 52, 56));
    scene.models.push_back(MakeVirtualGeomAggregateModel(
        scene.models[0],
        std::max(1u, config.gridCountX) * std::max(1u, config.gridCountZ)));

    scene.entities.push_back(EntityData{
        .modelIndex = 1,
        .transform = Mat4Identity(),
        .assetPath = "generated/virtual_geom_floor",
        .collidable = false,
    });

    const std::uint32_t gridCountX = std::max(1u, config.gridCountX);
    const std::uint32_t gridCountZ = std::max(1u, config.gridCountZ);
    const float startX = -0.5f * static_cast<float>(gridCountX - 1u) * config.gridSpacing;
    const float startZ = -0.5f * static_cast<float>(gridCountZ - 1u) * config.gridSpacing;
    const float modelMinY = ComputeModelMinY(scene.models[0], source.baseTransform);
    for (std::uint32_t z = 0; z < gridCountZ; ++z)
    {
        for (std::uint32_t x = 0; x < gridCountX; ++x)
        {
            Vec3 pos = Vec3Make(
                startX + static_cast<float>(x) * config.gridSpacing,
                -modelMinY,
                startZ + static_cast<float>(z) * config.gridSpacing);
            scene.entities.push_back(EntityData{
                .modelIndex = 0,
                .transform = Mat4Mul(Mat4Translate(pos), source.baseTransform),
                .assetPath = source.assetPath,
                .collidable = false,
            });
        }
    }
    scene.entities.push_back(EntityData{
        .modelIndex = 2,
        .transform = Mat4Identity(),
        .assetPath = "generated/virtual_geom_aggregate",
        .collidable = false,
    });
    return scene;
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
    scene.models.push_back(MakePlaneModel(18.0f, 220, 220, 220));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Identity(),
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
    if (texturePath != nullptr)
    {
        TextureData skinTexture = LoadTexture(texturePath->string());
        if (characterModel.textures.empty())
        {
            characterModel.textures.push_back(skinTexture);
            if (characterModel.materials.empty())
            {
                characterModel.materials.push_back(MaterialData{.name = "skin", .textureIndex = 0});
            }
            else
            {
                for (MaterialData& material : characterModel.materials)
                {
                    material.textureIndex = 0;
                }
            }
        }
        else
        {
            for (TextureData& texture : characterModel.textures)
            {
                texture = skinTexture;
            }
        }
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

SceneData LoadSampleScene(const AssetRegistry& assetRegistry, SceneKind kind, ManyLightsHeroModel manyLightsHeroModel)
{
    if (kind == SceneKind::PlayerMaskTest)
    {
        return BuildPlayerMaskTestScene(assetRegistry);
    }
    if (kind == SceneKind::FractureTest)
    {
        return BuildFractureTestScene(assetRegistry, FractureSceneConfig{});
    }
    if (kind == SceneKind::ProcCity)
    {
        CitySceneConfig config{};
        config.buildingMode = CitySceneConfig::BuildingMode::Procedural;
        config.roadLightStride = 2;
        return BuildSampleCity(assetRegistry, config);
    }
    if (kind == SceneKind::LightTileTest)
    {
        return BuildLightTileTestScene(assetRegistry);
    }
    if (kind == SceneKind::ManyLights)
    {
        return BuildManyLightsScene(assetRegistry, manyLightsHeroModel);
    }
    if (kind == SceneKind::VirtualGeomTest)
    {
        return MakeVirtualGeomTestScene(assetRegistry, VirtualGeomSceneConfig{});
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
    CitySceneConfig config{};
    config.buildingMode = CitySceneConfig::BuildingMode::Kenney;
    return BuildSampleCity(assetRegistry, config);
}

SceneData BuildVirtualGeomTestScene(const AssetRegistry& assetRegistry, const VirtualGeomSceneConfig& config)
{
    return MakeVirtualGeomTestScene(assetRegistry, config);
}
