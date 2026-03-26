#include "scene_city_internal.h"

#include <cmath>

namespace city_internal
{
void AddGroundTile(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int tx,
    int tz
)
{
    if (IsRoadTile(tx, tz))
    {
        return;
    }

    AddModelInstanceWithFootprint(
        scene,
        assetRegistry,
        cache,
        "kenney/kenney_city-kit-roads/Models/OBJ format/tile-low.obj",
        Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz)),
        0.0f,
        kRoadTileSize
    );
}

static void AddRoadLights(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int tx,
    int tz
)
{
    const Vec3 center = Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz));
    if (tx % kRoadStrideTiles == 0)
    {
        Vec3 leftPosition = Vec3Add(center, Vec3Make(-kRoadLightOffset, 0.0f, 0.0f));
        Vec3 rightPosition = Vec3Add(center, Vec3Make(kRoadLightOffset, 0.0f, 0.0f));
        float leftYaw = 90.0f;
        float rightYaw = -90.0f;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", leftPosition, leftYaw, 1.6f);
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", rightPosition, rightYaw, 1.6f);
        scene.spotLights.push_back(SpotLightData{
            .position = Vec3Add(leftPosition, Vec3Make(0.0f, kRoadLightHeight, 0.0f)),
            .range = 10.0f,
            .direction = Vec3Normalize(RotateYOffset(kRoadLightAimOffset, leftYaw)),
            .innerCos = std::cos(DegreesToRadians(18.0f)),
            .color = Vec3Make(1.0f, 0.86f, 0.62f),
            .outerCos = std::cos(DegreesToRadians(34.0f)),
            .intensity = 2.4f,
            .yawDegrees = leftYaw,
        });
        scene.spotLights.push_back(SpotLightData{
            .position = Vec3Add(rightPosition, Vec3Make(0.0f, kRoadLightHeight, 0.0f)),
            .range = 10.0f,
            .direction = Vec3Normalize(RotateYOffset(kRoadLightAimOffset, rightYaw)),
            .innerCos = std::cos(DegreesToRadians(18.0f)),
            .color = Vec3Make(1.0f, 0.86f, 0.62f),
            .outerCos = std::cos(DegreesToRadians(34.0f)),
            .intensity = 2.4f,
            .yawDegrees = rightYaw,
        });
        return;
    }

    Vec3 nearPosition = Vec3Add(center, Vec3Make(0.0f, 0.0f, -kRoadLightOffset));
    Vec3 farPosition = Vec3Add(center, Vec3Make(0.0f, 0.0f, kRoadLightOffset));
    float nearYaw = 0.0f;
    float farYaw = 180.0f;
    AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", nearPosition, nearYaw, 1.6f);
    AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", farPosition, farYaw, 1.6f);
    scene.spotLights.push_back(SpotLightData{
        .position = Vec3Add(nearPosition, Vec3Make(0.0f, kRoadLightHeight, 0.0f)),
        .range = 10.0f,
        .direction = Vec3Normalize(RotateYOffset(kRoadLightAimOffset, nearYaw)),
        .innerCos = std::cos(DegreesToRadians(18.0f)),
        .color = Vec3Make(1.0f, 0.86f, 0.62f),
        .outerCos = std::cos(DegreesToRadians(34.0f)),
        .intensity = 2.4f,
        .yawDegrees = nearYaw,
    });
    scene.spotLights.push_back(SpotLightData{
        .position = Vec3Add(farPosition, Vec3Make(0.0f, kRoadLightHeight, 0.0f)),
        .range = 10.0f,
        .direction = Vec3Normalize(RotateYOffset(kRoadLightAimOffset, farYaw)),
        .innerCos = std::cos(DegreesToRadians(18.0f)),
        .color = Vec3Make(1.0f, 0.86f, 0.62f),
        .outerCos = std::cos(DegreesToRadians(34.0f)),
        .intensity = 2.4f,
        .yawDegrees = farYaw,
    });
}

void AddRoadTile(SceneData& scene, const AssetRegistry& assetRegistry, ModelCache& cache, int tx, int tz)
{
    if (!IsRoadTile(tx, tz))
    {
        return;
    }

    const Vec3 p = Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz));
    if (IsIntersectionTile(tx, tz))
    {
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            "kenney/kenney_city-kit-roads/Models/FBX format/road-crossroad.fbx",
            p,
            0.0f,
            kRoadTileSize
        );
        return;
    }

    AddModelInstanceWithFootprint(
        scene,
        assetRegistry,
        cache,
        "kenney/kenney_city-kit-roads/Models/FBX format/road-straight.fbx",
        p,
        tx % kRoadStrideTiles == 0 ? 90.0f : 0.0f,
        kRoadTileSize
    );
    AddRoadLights(scene, assetRegistry, cache, tx, tz);
}

void AddBlockPerimeterBuildings(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int blockX,
    int blockZ
)
{
    const int baseTileX = blockX * kRoadStrideTiles;
    const int baseTileZ = blockZ * kRoadStrideTiles;

    for (int localX = 1; localX <= kLotsPerBlockSide; ++localX)
    {
        const int txNorth = baseTileX + localX;
        const int tzNorth = baseTileZ + 1;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, ChooseBuildingAsset(blockX, blockZ, localX), Vec3Make(TileCenter(txNorth), 0.0f, TileCenter(tzNorth)), 180.0f, kBuildingFootprint);

        const int txSouth = baseTileX + localX;
        const int tzSouth = baseTileZ + kLotsPerBlockSide;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, ChooseBuildingAsset(blockX, blockZ, 10 + localX), Vec3Make(TileCenter(txSouth), 0.0f, TileCenter(tzSouth)), 0.0f, kBuildingFootprint);
    }

    for (int localZ = 2; localZ <= kLotsPerBlockSide - 1; ++localZ)
    {
        const int txWest = baseTileX + 1;
        const int tzWest = baseTileZ + localZ;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, ChooseBuildingAsset(blockX, blockZ, 20 + localZ), Vec3Make(TileCenter(txWest), 0.0f, TileCenter(tzWest)), 90.0f, kBuildingFootprint);

        const int txEast = baseTileX + kLotsPerBlockSide;
        const int tzEast = baseTileZ + localZ;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, ChooseBuildingAsset(blockX, blockZ, 30 + localZ), Vec3Make(TileCenter(txEast), 0.0f, TileCenter(tzEast)), -90.0f, kBuildingFootprint);
    }
}

void AddTrafficVehicles(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int minTile,
    int maxTile
)
{
    static constexpr std::string_view kVehicles[] = {
        "kenney/kenney_car-kit/Models/FBX format/taxi.fbx",
        "kenney/kenney_car-kit/Models/FBX format/police.fbx",
        "kenney/kenney_car-kit/Models/FBX format/van.fbx",
        "kenney/kenney_car-kit/Models/FBX format/firetruck.fbx",
        "kenney/kenney_car-kit/Models/FBX format/sedan.fbx",
        "kenney/kenney_car-kit/Models/FBX format/suv.fbx",
        "kenney/kenney_car-kit/Models/FBX format/hatchback-sports.fbx",
        "kenney/kenney_car-kit/Models/FBX format/ambulance.fbx",
    };
    constexpr std::size_t kVehicleCount = sizeof(kVehicles) / sizeof(kVehicles[0]);

    int vehicleIndex = 0;
    for (int tx = minTile + 2; tx <= maxTile - 2; tx += 4)
    {
        if (tx % kRoadStrideTiles == 0)
        {
            continue;
        }
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, kVehicles[vehicleIndex % kVehicleCount], Vec3Make(TileCenter(tx), 0.0f, TileCenter(-4)), TrafficYawDegrees(1), kVehicleFootprint, false, true, 1);
        ++vehicleIndex;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, kVehicles[vehicleIndex % kVehicleCount], Vec3Make(TileCenter(tx), 0.0f, TileCenter(4)), TrafficYawDegrees(3), kVehicleFootprint, false, true, 3);
        ++vehicleIndex;
    }

    for (int tz = minTile + 2; tz <= maxTile - 2; tz += 4)
    {
        if (tz % kRoadStrideTiles == 0)
        {
            continue;
        }
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, kVehicles[vehicleIndex % kVehicleCount], Vec3Make(TileCenter(-4), 0.0f, TileCenter(tz)), TrafficYawDegrees(0), kVehicleFootprint, false, true, 0);
        ++vehicleIndex;
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, kVehicles[vehicleIndex % kVehicleCount], Vec3Make(TileCenter(4), 0.0f, TileCenter(tz)), TrafficYawDegrees(2), kVehicleFootprint, false, true, 2);
        ++vehicleIndex;
    }
}

void AddStreetProps(SceneData& scene, const AssetRegistry& assetRegistry, ModelCache& cache)
{
    for (int tx = -6; tx <= 6; tx += 2)
    {
        if (tx == 0)
        {
            continue;
        }
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            "kenney/kenney_city-kit-roads/Models/FBX format/construction-cone.fbx",
            Vec3Make(TileCenter(tx), 0.0f, TileCenter(-1)),
            0.0f,
            0.45f
        );
    }
}

void AddStreetWorld(SceneData& scene, const AssetRegistry& assetRegistry)
{
    ModelCache cache{};
    const int minTile = -kHalfCityTiles;
    const int maxTile = minTile + kCityTilesPerSide - 1;
    const int minBlock = minTile / kRoadStrideTiles;
    const int maxBlockExclusive = minBlock + (kCityTilesPerSide / kRoadStrideTiles);

    for (int tz = minTile; tz <= maxTile; ++tz)
    {
        for (int tx = minTile; tx <= maxTile; ++tx)
        {
            AddGroundTile(scene, assetRegistry, cache, tx, tz);
            AddRoadTile(scene, assetRegistry, cache, tx, tz);
        }
    }

    for (int bz = minBlock; bz < maxBlockExclusive; ++bz)
    {
        for (int bx = minBlock; bx < maxBlockExclusive; ++bx)
        {
            AddBlockPerimeterBuildings(scene, assetRegistry, cache, bx, bz);
        }
    }

    AddStreetProps(scene, assetRegistry, cache);
    AddTrafficVehicles(scene, assetRegistry, cache, minTile, maxTile);
}
}
