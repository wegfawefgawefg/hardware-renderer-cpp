#include "scene.h"

#include "generated_prism.h"

namespace
{
TextureData MakeSolidTextureData(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    TextureData texture{};
    texture.width = 1;
    texture.height = 1;
    texture.pixels = {r, g, b, 255};
    return texture;
}
}

SceneData BuildFractureTestScene(const AssetRegistry& assetRegistry, const FractureSceneConfig& config)
{
    SceneData scene{};

    ModelData floor{};
    floor.textures.push_back(MakeSolidTextureData(210, 206, 196));
    floor.materials.push_back(MaterialData{.name = "ground", .textureIndex = 0});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(-24.0f, 0.0f, -24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(0.0f, 0.0f)});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(24.0f, 0.0f, -24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(1.0f, 0.0f)});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(24.0f, 0.0f, 24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(1.0f, 1.0f)});
    floor.mesh.vertices.push_back(Vertex{.position = Vec3Make(-24.0f, 0.0f, 24.0f), .normal = Vec3Make(0.0f, 1.0f, 0.0f), .uv = Vec2Make(0.0f, 1.0f)});
    floor.mesh.indices = {0, 2, 1, 0, 3, 2};
    floor.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(floor.mesh.indices.size()),
        .materialIndex = 0,
    });
    scene.models.push_back(std::move(floor));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Identity(),
        .collidable = false,
    });

    std::uint32_t prismModelIndex = static_cast<std::uint32_t>(scene.models.size());
    scene.models.push_back(MakeGeneratedPrismModel(
        assetRegistry,
        config.prismHalfExtents,
        config.prismQuadSize));
    scene.entities.push_back(EntityData{
        .modelIndex = prismModelIndex,
        .transform = Mat4Translate(Vec3Make(0.0f, config.prismHalfExtents.y, 0.0f)),
        .assetPath = "generated/fracture_prism",
        .collidable = true,
    });

    return scene;
}
