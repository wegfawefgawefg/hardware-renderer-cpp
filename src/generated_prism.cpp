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
