#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "assets/asset_registry.h"
#include "scene.h"

namespace city_internal
{
using ModelCache = std::unordered_map<std::string, std::uint32_t>;

inline constexpr float kRoadTileSize = 6.0f;
inline constexpr int kLotsPerBlockSide = 3;
inline constexpr int kRoadStrideTiles = kLotsPerBlockSide + 1;
inline constexpr int kCityTilesPerSide = 32;
inline constexpr int kHalfCityTiles = kCityTilesPerSide / 2;
inline constexpr float kVehicleFootprint = 2.25f;
inline constexpr float kBuildingFootprint = 5.0f;
inline constexpr float kRoadLightOffset = kRoadTileSize * 0.48f;
inline constexpr float kRoadLightHeight = 5.35f;
inline constexpr Vec3 kRoadLightAimOffset = {-0.062f, -5.350f, 1.303f};

float TrafficYawDegrees(int direction);
Mat4 PlacementTransform(Vec3 position, float yawDegrees, float scale = 1.0f);
Vec3 RotateYOffset(Vec3 v, float yawDegrees);
float ComputeModelFootprint(const ModelData& model);
ModelData LoadSceneModelByExtension(const std::filesystem::path& path);
std::uint32_t AddModelInstanceWithFootprint(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    std::string_view relativePath,
    Vec3 position,
    float yawDegrees,
    float targetFootprint,
    bool collidable = true,
    bool traffic = false,
    std::int32_t trafficDirection = -1
);
float TileCenter(int tile);
bool IsRoadTile(int tx, int tz);
bool IsIntersectionTile(int tx, int tz);
void AddGroundTile(SceneData& scene, const AssetRegistry& assetRegistry, ModelCache& cache, int tx, int tz);
void AddRoadTile(SceneData& scene, const AssetRegistry& assetRegistry, ModelCache& cache, int tx, int tz);
const char* ChooseBuildingAsset(int bx, int bz, int edgeIndex);
void AddBlockPerimeterBuildings(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int blockX,
    int blockZ
);
void AddTrafficVehicles(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int minTile,
    int maxTile
);
void AddStreetProps(SceneData& scene, const AssetRegistry& assetRegistry, ModelCache& cache);
void AddStreetWorld(SceneData& scene, const AssetRegistry& assetRegistry);
}
