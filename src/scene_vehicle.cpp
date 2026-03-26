#include "scene.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>

#include "assets/fbx_loader.h"
#include "assets/texture_loader.h"

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

void AppendFace(ModelData& model, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal)
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
}

SceneData BuildVehicleLightTestScene(const AssetRegistry& assetRegistry)
{
    SceneData scene{};
    scene.models.push_back(MakeBoxModel(Vec3Make(80.0f, 0.25f, 16.0f), 225, 225, 225));
    scene.models.push_back(MakeBoxModel(Vec3Make(1.0f, 1.4f, 0.4f), 220, 120, 80));
    scene.models.push_back(MakeBoxModel(Vec3Make(0.5f, 0.7f, 0.5f), 70, 120, 220));
    scene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = Mat4Translate(Vec3Make(0.0f, -0.25f, 0.0f)),
        .collidable = true,
    });

    static constexpr std::array<std::string_view, 8> kVehicles = {{
        "kenney/kenney_car-kit/Models/FBX format/taxi.fbx",
        "kenney/kenney_car-kit/Models/FBX format/police.fbx",
        "kenney/kenney_car-kit/Models/FBX format/van.fbx",
        "kenney/kenney_car-kit/Models/FBX format/firetruck.fbx",
        "kenney/kenney_car-kit/Models/FBX format/sedan.fbx",
        "kenney/kenney_car-kit/Models/FBX format/suv.fbx",
        "kenney/kenney_car-kit/Models/FBX format/hatchback-sports.fbx",
        "kenney/kenney_car-kit/Models/FBX format/ambulance.fbx",
    }};

    float cursorX = -56.0f;
    for (std::size_t i = 0; i < kVehicles.size(); ++i)
    {
        const std::filesystem::path* path = assetRegistry.FindByRelativePath(kVehicles[i]);
        if (path == nullptr)
        {
            continue;
        }

        ModelData vehicle = LoadFbxModel(path->string());
        if (vehicle.mesh.vertices.empty() || vehicle.mesh.indices.empty())
        {
            continue;
        }

        float footprint = ComputeModelFootprint(vehicle);
        float scale = 2.25f / footprint;
        std::uint32_t modelIndex = static_cast<std::uint32_t>(scene.models.size());
        scene.models.push_back(std::move(vehicle));

        Vec3 origin = Vec3Make(cursorX, 0.0f, 0.0f);
        scene.entities.push_back(EntityData{
            .modelIndex = modelIndex,
            .transform = Mat4Mul(Mat4Translate(origin), Mat4Scale(scale)),
            .collidable = true,
        });
        std::uint32_t entityIndex = static_cast<std::uint32_t>(scene.entities.size() - 1);
        scene.vehicleLightTestItems.push_back(SceneData::VehicleLightTestItem{
            .assetPath = std::string(kVehicles[i]),
            .entityIndex = entityIndex,
            .origin = origin,
            .scale = scale,
            .selectionRadius = 5.5f,
        });

        scene.entities.push_back(EntityData{
            .modelIndex = 1,
            .transform = Mat4Translate(Vec3Add(origin, Vec3Make(0.0f, 1.4f, 6.0f))),
            .collidable = true,
        });
        scene.entities.push_back(EntityData{
            .modelIndex = 2,
            .transform = Mat4Translate(Vec3Add(origin, Vec3Make(2.2f, 0.7f, 3.5f))),
            .collidable = true,
        });

        cursorX += 16.0f;
    }

    return scene;
}
