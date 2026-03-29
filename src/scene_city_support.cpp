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

std::uint32_t AddGeneratedModelInstance(
    SceneData& scene,
    ModelCache& cache,
    std::string key,
    ModelData model,
    Vec3 position,
    float yawDegrees,
    bool collidable,
    bool traffic,
    std::int32_t trafficDirection
)
{
    auto found = cache.find(key);
    std::uint32_t modelIndex = 0;
    if (found == cache.end())
    {
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

    scene.entities.push_back(EntityData{
        .modelIndex = modelIndex,
        .transform = PlacementTransform(position, yawDegrees, 1.0f),
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
}
