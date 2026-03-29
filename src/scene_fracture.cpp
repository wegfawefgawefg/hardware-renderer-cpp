#include "scene.h"

#include <algorithm>

#include "assets/texture_loader.h"

namespace
{
constexpr std::string_view kFracturePrismTexture = "rivet_plate.png";
constexpr std::string_view kFracturePrismNormalTexture = "rivet_plate_normal.png";

TextureData MakeSolidTextureData(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    TextureData texture{};
    texture.width = 1;
    texture.height = 1;
    texture.pixels = {r, g, b, 255};
    return texture;
}

void AppendQuadGrid(
    ModelData& model,
    Vec3 origin,
    Vec3 axisU,
    Vec3 axisV,
    Vec3 normal,
    int segU,
    int segV)
{
    std::uint32_t baseVertex = static_cast<std::uint32_t>(model.mesh.vertices.size());
    for (int y = 0; y <= segV; ++y)
    {
        float fy = static_cast<float>(y) / static_cast<float>(std::max(segV, 1));
        for (int x = 0; x <= segU; ++x)
        {
            float fx = static_cast<float>(x) / static_cast<float>(std::max(segU, 1));
            model.mesh.vertices.push_back(Vertex{
                .position = Vec3Add(origin, Vec3Add(Vec3Scale(axisU, fx), Vec3Scale(axisV, fy))),
                .normal = normal,
                .uv = Vec2Make(static_cast<float>(x), static_cast<float>(segV - y)),
            });
        }
    }

    auto vertexAt = [&](int x, int y)
    {
        return baseVertex + static_cast<std::uint32_t>(y * (segU + 1) + x);
    };

    std::uint32_t firstIndex = static_cast<std::uint32_t>(model.mesh.indices.size());
    for (int y = 0; y < segV; ++y)
    {
        for (int x = 0; x < segU; ++x)
        {
            std::uint32_t i00 = vertexAt(x, y);
            std::uint32_t i10 = vertexAt(x + 1, y);
            std::uint32_t i01 = vertexAt(x, y + 1);
            std::uint32_t i11 = vertexAt(x + 1, y + 1);
            bool flipped = Vec3Dot(Vec3Cross(axisU, axisV), normal) < 0.0f;
            if (!flipped)
            {
                model.mesh.indices.push_back(i00);
                model.mesh.indices.push_back(i10);
                model.mesh.indices.push_back(i11);
                model.mesh.indices.push_back(i00);
                model.mesh.indices.push_back(i11);
                model.mesh.indices.push_back(i01);
            }
            else
            {
                model.mesh.indices.push_back(i00);
                model.mesh.indices.push_back(i11);
                model.mesh.indices.push_back(i10);
                model.mesh.indices.push_back(i00);
                model.mesh.indices.push_back(i01);
                model.mesh.indices.push_back(i11);
            }
        }
    }

    model.primitives.push_back(PrimitiveData{
        .firstIndex = firstIndex,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()) - firstIndex,
        .materialIndex = 0,
    });
}

ModelData MakeFracturePrismModel(const AssetRegistry& assetRegistry, Vec3 halfExtents, int segX, int segY, int segZ)
{
    ModelData model{};
    if (const std::filesystem::path* texturePath = assetRegistry.FindByRelativePath(kFracturePrismTexture))
    {
        model.textures.push_back(LoadTexture(texturePath->string()));
    }
    else
    {
        model.textures.push_back(MakeSolidTextureData(196, 201, 210));
    }
    std::int32_t normalTextureIndex = -1;
    if (const std::filesystem::path* normalTexturePath = assetRegistry.FindByRelativePath(kFracturePrismNormalTexture))
    {
        normalTextureIndex = static_cast<std::int32_t>(model.textures.size());
        model.textures.push_back(LoadTexture(normalTexturePath->string()));
    }
    model.materials.push_back(MaterialData{
        .name = "fracture_prism",
        .textureIndex = 0,
        .normalTextureIndex = normalTextureIndex,
        .flipNormalY = true,
    });

    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, 1.0f),
        segX,
        segY);
    AppendQuadGrid(
        model,
        Vec3Make(halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(-halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, -1.0f),
        segX,
        segY);
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(0.0f, 0.0f, halfExtents.z * 2.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(-1.0f, 0.0f, 0.0f),
        segZ,
        segY);
    AppendQuadGrid(
        model,
        Vec3Make(halfExtents.x, -halfExtents.y, halfExtents.z),
        Vec3Make(0.0f, 0.0f, -halfExtents.z * 2.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(1.0f, 0.0f, 0.0f),
        segZ,
        segY);
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, halfExtents.y, halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, -halfExtents.z * 2.0f),
        Vec3Make(0.0f, 1.0f, 0.0f),
        segX,
        segZ);
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, halfExtents.z * 2.0f),
        Vec3Make(0.0f, -1.0f, 0.0f),
        segX,
        segZ);

    return model;
}

}

SceneData BuildFractureTestScene(const AssetRegistry& assetRegistry, const FractureSceneConfig& config)
{
    (void)assetRegistry;
    SceneData scene{};
    ModelData floor{};
    floor.textures.push_back(MakeSolidTextureData(210, 206, 196));
    floor.materials.push_back(MaterialData{.name = "ground", .textureIndex = 0});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(-24.0f, 0.0f, -24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(0.0f, 0.0f)});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(24.0f, 0.0f, -24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(1.0f, 0.0f)});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(24.0f, 0.0f, 24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(1.0f, 1.0f)});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(-24.0f, 0.0f, 24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(0.0f, 1.0f)});
    floor.mesh.indices = {0, 2, 1, 0, 3, 2};
    floor.primitives.push_back(PrimitiveData{.firstIndex = 0, .indexCount = static_cast<std::uint32_t>(floor.mesh.indices.size()), .materialIndex = 0});
    scene.models.push_back(std::move(floor));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Identity(),
        .collidable = false,
    });

    std::uint32_t prismModelIndex = static_cast<std::uint32_t>(scene.models.size());
    scene.models.push_back(MakeFracturePrismModel(
        assetRegistry,
        config.prismHalfExtents,
        static_cast<int>(std::max(config.prismSegX, 1u)),
        static_cast<int>(std::max(config.prismSegY, 1u)),
        static_cast<int>(std::max(config.prismSegZ, 1u))));
    scene.entities.push_back(EntityData{
        .modelIndex = prismModelIndex,
        .transform = Mat4Translate(Vec3Make(0.0f, config.prismHalfExtents.y, 0.0f)),
        .assetPath = "generated/fracture_prism",
        .collidable = true,
    });

    return scene;
}
