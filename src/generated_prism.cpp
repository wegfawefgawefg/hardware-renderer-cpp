#include "generated_prism.h"

#include <algorithm>
#include <cmath>

#include "assets/texture_loader.h"

namespace
{
constexpr std::string_view kPrismTexture = "rivet_plate.png";
constexpr std::string_view kPrismNormalTexture = "rivet_plate_normal.png";

TextureData MakeSolidTextureData(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    TextureData texture{};
    texture.width = 1;
    texture.height = 1;
    texture.pixels = {r, g, b, 255};
    return texture;
}

std::uint32_t ComputeSegments(float fullExtent, float targetQuadSize)
{
    float clampedQuadSize = std::max(targetQuadSize, 0.05f);
    return static_cast<std::uint32_t>(std::max(1.0f, std::round(fullExtent / clampedQuadSize)));
}

void AddQuad(
    ModelData& model,
    Vec3 a,
    Vec3 b,
    Vec3 c,
    Vec3 d,
    Vec3 normal,
    Vec2 uvMin = Vec2Make(0.0f, 0.0f),
    Vec2 uvMax = Vec2Make(1.0f, 1.0f))
{
    std::uint32_t baseVertex = static_cast<std::uint32_t>(model.mesh.vertices.size());
    model.mesh.vertices.push_back(Vertex{.position = a, .normal = normal, .uv = Vec2Make(uvMin.x, uvMin.y)});
    model.mesh.vertices.push_back(Vertex{.position = b, .normal = normal, .uv = Vec2Make(uvMax.x, uvMin.y)});
    model.mesh.vertices.push_back(Vertex{.position = c, .normal = normal, .uv = Vec2Make(uvMax.x, uvMax.y)});
    model.mesh.vertices.push_back(Vertex{.position = d, .normal = normal, .uv = Vec2Make(uvMin.x, uvMax.y)});
    model.mesh.indices.push_back(baseVertex + 0);
    model.mesh.indices.push_back(baseVertex + 1);
    model.mesh.indices.push_back(baseVertex + 2);
    model.mesh.indices.push_back(baseVertex + 0);
    model.mesh.indices.push_back(baseVertex + 2);
    model.mesh.indices.push_back(baseVertex + 3);
}

void AppendBox(ModelData& model, Vec3 halfExtents, Vec3 center, std::uint32_t materialIndex)
{
    std::uint32_t firstIndex = static_cast<std::uint32_t>(model.mesh.indices.size());
    Vec3 min = Vec3Sub(center, halfExtents);
    Vec3 max = Vec3Add(center, halfExtents);
    AddQuad(
        model,
        Vec3Make(min.x, min.y, max.z),
        Vec3Make(max.x, min.y, max.z),
        Vec3Make(max.x, max.y, max.z),
        Vec3Make(min.x, max.y, max.z),
        Vec3Make(0.0f, 0.0f, 1.0f));
    AddQuad(
        model,
        Vec3Make(max.x, min.y, min.z),
        Vec3Make(min.x, min.y, min.z),
        Vec3Make(min.x, max.y, min.z),
        Vec3Make(max.x, max.y, min.z),
        Vec3Make(0.0f, 0.0f, -1.0f));
    AddQuad(
        model,
        Vec3Make(min.x, min.y, min.z),
        Vec3Make(min.x, min.y, max.z),
        Vec3Make(min.x, max.y, max.z),
        Vec3Make(min.x, max.y, min.z),
        Vec3Make(-1.0f, 0.0f, 0.0f));
    AddQuad(
        model,
        Vec3Make(max.x, min.y, max.z),
        Vec3Make(max.x, min.y, min.z),
        Vec3Make(max.x, max.y, min.z),
        Vec3Make(max.x, max.y, max.z),
        Vec3Make(1.0f, 0.0f, 0.0f));
    AddQuad(
        model,
        Vec3Make(min.x, max.y, max.z),
        Vec3Make(max.x, max.y, max.z),
        Vec3Make(max.x, max.y, min.z),
        Vec3Make(min.x, max.y, min.z),
        Vec3Make(0.0f, 1.0f, 0.0f));
    AddQuad(
        model,
        Vec3Make(min.x, min.y, min.z),
        Vec3Make(max.x, min.y, min.z),
        Vec3Make(max.x, min.y, max.z),
        Vec3Make(min.x, min.y, max.z),
        Vec3Make(0.0f, -1.0f, 0.0f));

    model.primitives.push_back(PrimitiveData{
        .firstIndex = firstIndex,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()) - firstIndex,
        .materialIndex = materialIndex,
    });
}

ModelData MakeSolidColorModelData(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::string_view name)
{
    ModelData model{};
    model.textures.push_back(MakeSolidTextureData(r, g, b));
    model.materials.push_back(MaterialData{
        .name = std::string(name),
        .textureIndex = 0,
        .normalTextureIndex = -1,
        .castsShadows = true,
        .flipNormalY = true,
        .generatedQuadMaterialUv = false,
        .leanShading = true,
    });
    return model;
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
                .uv = Vec2Make(fx, 1.0f - fy),
            });
        }
    }

    auto vertexAt = [&](int x, int y)
    {
        return baseVertex + static_cast<std::uint32_t>(y * (segU + 1) + x);
    };

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
}
}

GeneratedPrismLayout ComputeGeneratedPrismLayout(Vec3 halfExtents, float targetQuadSize)
{
    return GeneratedPrismLayout{
        .segX = ComputeSegments(halfExtents.x * 2.0f, targetQuadSize),
        .segY = ComputeSegments(halfExtents.y * 2.0f, targetQuadSize),
        .segZ = ComputeSegments(halfExtents.z * 2.0f, targetQuadSize),
    };
}

ModelData MakeGeneratedPrismModel(
    const AssetRegistry& assetRegistry,
    Vec3 halfExtents,
    float targetQuadSize
)
{
    GeneratedPrismLayout layout = ComputeGeneratedPrismLayout(halfExtents, targetQuadSize);

    ModelData model{};
    if (const std::filesystem::path* texturePath = assetRegistry.FindByRelativePath(kPrismTexture))
    {
        model.textures.push_back(LoadTexture(texturePath->string()));
    }
    else
    {
        model.textures.push_back(MakeSolidTextureData(196, 201, 210));
    }

    std::int32_t normalTextureIndex = -1;
    if (const std::filesystem::path* normalTexturePath = assetRegistry.FindByRelativePath(kPrismNormalTexture))
    {
        normalTextureIndex = static_cast<std::int32_t>(model.textures.size());
        model.textures.push_back(LoadTexture(normalTexturePath->string()));
    }

    model.materials.push_back(MaterialData{
        .name = "generated_prism",
        .textureIndex = 0,
        .normalTextureIndex = normalTextureIndex,
        .flipNormalY = true,
        .generatedQuadMaterialUv = true,
    });

    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, 1.0f),
        static_cast<int>(layout.segX),
        static_cast<int>(layout.segY));
    AppendQuadGrid(
        model,
        Vec3Make(halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(-halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, -1.0f),
        static_cast<int>(layout.segX),
        static_cast<int>(layout.segY));
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(0.0f, 0.0f, halfExtents.z * 2.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(-1.0f, 0.0f, 0.0f),
        static_cast<int>(layout.segZ),
        static_cast<int>(layout.segY));
    AppendQuadGrid(
        model,
        Vec3Make(halfExtents.x, -halfExtents.y, halfExtents.z),
        Vec3Make(0.0f, 0.0f, -halfExtents.z * 2.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(1.0f, 0.0f, 0.0f),
        static_cast<int>(layout.segZ),
        static_cast<int>(layout.segY));
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, halfExtents.y, halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, -halfExtents.z * 2.0f),
        Vec3Make(0.0f, 1.0f, 0.0f),
        static_cast<int>(layout.segX),
        static_cast<int>(layout.segZ));
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, halfExtents.z * 2.0f),
        Vec3Make(0.0f, -1.0f, 0.0f),
        static_cast<int>(layout.segX),
        static_cast<int>(layout.segZ));

    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });

    return model;
}

ModelData MakeGeneratedProcCityModel(
    const AssetRegistry& assetRegistry,
    Vec3 halfExtents,
    float targetQuadSize
)
{
    (void)targetQuadSize;
    ModelData model{};
    if (const std::filesystem::path* texturePath = assetRegistry.FindByRelativePath(kPrismTexture))
    {
        model.textures.push_back(LoadTexture(texturePath->string()));
    }
    else
    {
        model.textures.push_back(MakeSolidTextureData(196, 201, 210));
    }

    std::int32_t normalTextureIndex = -1;
    if (const std::filesystem::path* normalTexturePath = assetRegistry.FindByRelativePath(kPrismNormalTexture))
    {
        normalTextureIndex = static_cast<std::int32_t>(model.textures.size());
        model.textures.push_back(LoadTexture(normalTexturePath->string()));
    }

    model.materials.push_back(MaterialData{
        .name = "generated_proc_city",
        .textureIndex = 0,
        .normalTextureIndex = normalTextureIndex,
        .flipNormalY = true,
        .generatedQuadMaterialUv = true,
        .leanShading = true,
    });

    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, 1.0f),
        1,
        1);
    AppendQuadGrid(
        model,
        Vec3Make(halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(-halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, -1.0f),
        1,
        1);
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(0.0f, 0.0f, halfExtents.z * 2.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(-1.0f, 0.0f, 0.0f),
        1,
        1);
    AppendQuadGrid(
        model,
        Vec3Make(halfExtents.x, -halfExtents.y, halfExtents.z),
        Vec3Make(0.0f, 0.0f, -halfExtents.z * 2.0f),
        Vec3Make(0.0f, halfExtents.y * 2.0f, 0.0f),
        Vec3Make(1.0f, 0.0f, 0.0f),
        1,
        1);
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, halfExtents.y, halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, -halfExtents.z * 2.0f),
        Vec3Make(0.0f, 1.0f, 0.0f),
        1,
        1);
    AppendQuadGrid(
        model,
        Vec3Make(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        Vec3Make(halfExtents.x * 2.0f, 0.0f, 0.0f),
        Vec3Make(0.0f, 0.0f, halfExtents.z * 2.0f),
        Vec3Make(0.0f, -1.0f, 0.0f),
        1,
        1);

    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });

    return model;
}

ModelData MakeGeneratedProcCityGroundTileModel(float tileSize)
{
    ModelData model = MakeSolidColorModelData(194, 194, 188, "generated_proc_city_ground");
    AddQuad(
        model,
        Vec3Make(-tileSize * 0.5f, 0.0f, -tileSize * 0.5f),
        Vec3Make(-tileSize * 0.5f, 0.0f, tileSize * 0.5f),
        Vec3Make(tileSize * 0.5f, 0.0f, tileSize * 0.5f),
        Vec3Make(tileSize * 0.5f, 0.0f, -tileSize * 0.5f),
        Vec3Make(0.0f, 1.0f, 0.0f));
    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

ModelData MakeGeneratedProcCityRoadTileModel(float tileSize, bool intersection)
{
    (void)intersection;
    ModelData model = MakeSolidColorModelData(
        58,
        60,
        64,
        intersection ? "generated_proc_city_crossroad" : "generated_proc_city_road");
    AddQuad(
        model,
        Vec3Make(-tileSize * 0.5f, 0.0f, -tileSize * 0.5f),
        Vec3Make(-tileSize * 0.5f, 0.0f, tileSize * 0.5f),
        Vec3Make(tileSize * 0.5f, 0.0f, tileSize * 0.5f),
        Vec3Make(tileSize * 0.5f, 0.0f, -tileSize * 0.5f),
        Vec3Make(0.0f, 1.0f, 0.0f));
    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

ModelData MakeGeneratedProcCityStreetLightModel()
{
    ModelData model = MakeSolidColorModelData(212, 214, 218, "generated_proc_city_street_light");
    AppendBox(model, Vec3Make(0.08f, 2.65f, 0.08f), Vec3Make(0.0f, 2.65f, 0.0f), 0);
    AppendBox(model, Vec3Make(0.06f, 0.06f, 0.55f), Vec3Make(0.0f, 5.05f, 0.48f), 0);
    AppendBox(model, Vec3Make(0.10f, 0.12f, 0.16f), Vec3Make(0.0f, 4.95f, 0.98f), 0);
    AppendBox(model, Vec3Make(0.16f, 0.04f, 0.16f), Vec3Make(0.0f, 0.04f, 0.0f), 0);
    return model;
}
