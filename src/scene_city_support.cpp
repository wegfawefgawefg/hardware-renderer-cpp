#include "scene_city_internal.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "assets/fbx_loader.h"
#include "assets/obj_loader.h"

namespace city_internal
{
float TrafficYawDegrees(int direction)
{
    switch (direction)
    {
    case 0: return 180.0f;
    case 1: return -90.0f;
    case 2: return 0.0f;
    default: return 90.0f;
    }
}

Mat4 PlacementTransform(Vec3 position, float yawDegrees, float scale)
{
    return Mat4Mul(
        Mat4Translate(position),
        Mat4Mul(Mat4RotateY(DegreesToRadians(yawDegrees)), Mat4Scale(scale))
    );
}

Vec3 RotateYOffset(Vec3 v, float yawDegrees)
{
    float radians = DegreesToRadians(yawDegrees);
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Make(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
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

ModelData LoadSceneModelByExtension(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    if (extension == ".fbx" || extension == ".FBX")
    {
        return LoadFbxModel(path.string());
    }
    if (extension == ".obj" || extension == ".OBJ")
    {
        return LoadObjModel(path.string());
    }
    return {};
}

std::uint32_t AddModelInstanceWithFootprint(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    std::string_view relativePath,
    Vec3 position,
    float yawDegrees,
    float targetFootprint,
    bool collidable,
    bool traffic,
    std::int32_t trafficDirection
)
{
    const std::string key(relativePath);
    auto found = cache.find(key);
    std::uint32_t modelIndex = 0;
    if (found == cache.end())
    {
        const std::filesystem::path* path = assetRegistry.FindByRelativePath(relativePath);
        if (path == nullptr)
        {
            return static_cast<std::uint32_t>(-1);
        }

        ModelData model = LoadSceneModelByExtension(*path);
        if (model.mesh.indices.empty() || model.mesh.vertices.empty())
        {
            return static_cast<std::uint32_t>(-1);
        }

        modelIndex = static_cast<std::uint32_t>(scene.models.size());
        scene.models.push_back(std::move(model));
        cache.emplace(key, modelIndex);
    }
    else
    {
        modelIndex = found->second;
    }

    float footprint = ComputeModelFootprint(scene.models[modelIndex]);
    float scale = targetFootprint / footprint;
    scene.entities.push_back(EntityData{
        .modelIndex = modelIndex,
        .transform = PlacementTransform(position, yawDegrees, scale),
        .assetPath = key,
        .collidable = collidable,
        .traffic = traffic,
        .trafficDirection = trafficDirection,
    });
    return static_cast<std::uint32_t>(scene.entities.size() - 1);
}

float TileCenter(int tile)
{
    return static_cast<float>(tile) * kRoadTileSize;
}

bool IsRoadTile(int tx, int tz)
{
    return tx % kRoadStrideTiles == 0 || tz % kRoadStrideTiles == 0;
}

bool IsIntersectionTile(int tx, int tz)
{
    return tx % kRoadStrideTiles == 0 && tz % kRoadStrideTiles == 0;
}

const char* ChooseBuildingAsset(int bx, int bz, int edgeIndex)
{
    static constexpr const char* kCommercial[] = {
        "kenney/kenney_city-kit-commercial_2.1/Models/FBX format/low-detail-building-a.fbx",
        "kenney/kenney_city-kit-commercial_2.1/Models/FBX format/low-detail-building-f.fbx",
        "kenney/kenney_city-kit-commercial_2.1/Models/FBX format/low-detail-building-j.fbx",
        "kenney/kenney_city-kit-commercial_2.1/Models/FBX format/building-skyscraper-b.fbx",
    };
    static constexpr const char* kIndustrial[] = {
        "kenney/kenney_city-kit-industrial_1.0/Models/FBX format/building-h.fbx",
        "kenney/kenney_city-kit-industrial_1.0/Models/FBX format/building-r.fbx",
        "kenney/kenney_city-kit-industrial_1.0/Models/FBX format/building-t.fbx",
        "kenney/kenney_city-kit-industrial_1.0/Models/FBX format/chimney-large.fbx",
    };
    static constexpr const char* kSuburban[] = {
        "kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-c.fbx",
        "kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-h.fbx",
        "kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-q.fbx",
        "kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-u.fbx",
    };

    const int selector = std::abs(bx * 31 + bz * 17 + edgeIndex * 13) % 3;
    const int item = std::abs(bx * 7 + bz * 5 + edgeIndex * 3) % 4;
    if (selector == 0)
    {
        return kCommercial[item];
    }
    if (selector == 1)
    {
        return kIndustrial[item];
    }
    return kSuburban[item];
}
}
