#include "scene_city_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "generated_prism.h"

namespace city_internal
{
namespace
{
struct GeneratedBuildingSpec
{
    Vec3 halfExtents = {};
    float yawDegrees = 0.0f;
};

struct KenneyBuildingChoice
{
    std::string_view assetPath;
    float targetFootprint = kBuildingFootprint;
};

constexpr std::array<KenneyBuildingChoice, 12> kKenneyBuildingChoices = {{
    {"kenney/kenney_city-kit-commercial_2.1/Models/FBX format/low-detail-building-a.fbx", 5.0f},
    {"kenney/kenney_city-kit-commercial_2.1/Models/FBX format/low-detail-building-f.fbx", 5.0f},
    {"kenney/kenney_city-kit-commercial_2.1/Models/FBX format/low-detail-building-j.fbx", 5.0f},
    {"kenney/kenney_city-kit-commercial_2.1/Models/FBX format/building-skyscraper-b.fbx", 5.0f},
    {"kenney/kenney_city-kit-industrial_1.0/Models/FBX format/building-h.fbx", 5.0f},
    {"kenney/kenney_city-kit-industrial_1.0/Models/FBX format/building-r.fbx", 5.0f},
    {"kenney/kenney_city-kit-industrial_1.0/Models/FBX format/building-t.fbx", 5.0f},
    {"kenney/kenney_city-kit-industrial_1.0/Models/FBX format/chimney-large.fbx", 5.0f},
    {"kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-c.fbx", 5.0f},
    {"kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-h.fbx", 5.0f},
    {"kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-q.fbx", 5.0f},
    {"kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-u.fbx", 5.0f},
}};

constexpr std::array<float, 3> kBaseBuildingWidths = {3.75f, 4.375f, 5.0f};
constexpr std::array<float, 3> kBaseBuildingDepths = {3.75f, 4.375f, 5.0f};
constexpr std::array<float, 11> kBaseBuildingHeights = {
    5.0f, 5.625f, 6.25f, 6.875f, 7.5f, 8.125f, 8.75f, 9.375f, 10.0f, 10.625f, 11.25f,
};
constexpr std::array<Vec3, 3> kProcCityArchetypeHalfExtents = {
    Vec3{2.5f, 2.5f, 2.5f},
    Vec3{2.5f, 4.0f, 2.5f},
    Vec3{2.5f, 5.5f, 2.5f},
};

std::uint32_t HashBuildingSeed(int blockX, int blockZ, int edgeIndex)
{
    std::uint32_t x = static_cast<std::uint32_t>(blockX * 73856093);
    std::uint32_t z = static_cast<std::uint32_t>(blockZ * 19349663);
    std::uint32_t e = static_cast<std::uint32_t>(edgeIndex * 83492791);
    return x ^ z ^ e;
}

float QuantizeDimension(float value, float step)
{
    float clampedStep = std::max(step, 0.05f);
    return std::max(clampedStep, std::round(value / clampedStep) * clampedStep);
}

GeneratedBuildingSpec ChooseGeneratedBuildingSpec(
    int blockX,
    int blockZ,
    int edgeIndex,
    float yawDegrees,
    float quadSize,
    CitySceneConfig::BuildingMode buildingMode)
{
    std::uint32_t seed = HashBuildingSeed(blockX, blockZ, edgeIndex);
    if (buildingMode == CitySceneConfig::BuildingMode::Procedural)
    {
        GeneratedBuildingSpec spec{};
        spec.halfExtents = kProcCityArchetypeHalfExtents[seed % kProcCityArchetypeHalfExtents.size()];
        spec.yawDegrees = yawDegrees;
        return spec;
    }

    float fullWidth = kBaseBuildingWidths[seed % kBaseBuildingWidths.size()];
    float fullDepth = kBaseBuildingDepths[(seed / 3u) % kBaseBuildingDepths.size()];
    float fullHeight = kBaseBuildingHeights[(seed / 9u) % kBaseBuildingHeights.size()];

    GeneratedBuildingSpec spec{};
    spec.halfExtents.x = 0.5f * QuantizeDimension(fullWidth, quadSize);
    spec.halfExtents.y = 0.5f * QuantizeDimension(fullHeight, quadSize);
    spec.halfExtents.z = 0.5f * QuantizeDimension(fullDepth, quadSize);
    spec.yawDegrees = yawDegrees;
    return spec;
}

void AddGeneratedBuildingInstance(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    const CitySceneConfig& config,
    int blockX,
    int blockZ,
    int edgeIndex,
    Vec3 position,
    float yawDegrees,
    float quadSize
)
{
    float clampedQuadSize = std::max(quadSize, 0.05f);
    GeneratedBuildingSpec spec = ChooseGeneratedBuildingSpec(
        blockX,
        blockZ,
        edgeIndex,
        yawDegrees,
        clampedQuadSize,
        config.buildingMode);
    int widthSteps = static_cast<int>(std::round((spec.halfExtents.x * 2.0f) / clampedQuadSize));
    int heightSteps = static_cast<int>(std::round((spec.halfExtents.y * 2.0f) / clampedQuadSize));
    int depthSteps = static_cast<int>(std::round((spec.halfExtents.z * 2.0f) / clampedQuadSize));

    char key[128];
    if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
    {
        std::snprintf(
            key,
            sizeof(key),
            "generated/proc_city_building_a%d_q%03d",
            HashBuildingSeed(blockX, blockZ, edgeIndex) % static_cast<int>(kProcCityArchetypeHalfExtents.size()),
            static_cast<int>(std::round(clampedQuadSize * 100.0f)));
    }
    else
    {
        std::snprintf(
            key,
            sizeof(key),
            "generated/city_building_w%d_h%d_d%d_q%03d",
            widthSteps,
            heightSteps,
            depthSteps,
            static_cast<int>(std::round(clampedQuadSize * 100.0f)));
    }

    ModelData model = config.buildingMode == CitySceneConfig::BuildingMode::Procedural
        ? MakeGeneratedProcCityModel(assetRegistry, spec.halfExtents, clampedQuadSize)
        : MakeGeneratedPrismModel(assetRegistry, spec.halfExtents, clampedQuadSize);
    AddGeneratedModelInstance(
        scene,
        cache,
        key,
        std::move(model),
        Vec3Make(position.x, spec.halfExtents.y, position.z),
        spec.yawDegrees);
}

void AddKenneyBuildingInstance(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int blockX,
    int blockZ,
    int edgeIndex,
    Vec3 position,
    float yawDegrees)
{
    std::uint32_t seed = HashBuildingSeed(blockX, blockZ, edgeIndex);
    const KenneyBuildingChoice& choice = kKenneyBuildingChoices[seed % kKenneyBuildingChoices.size()];
    AddModelInstanceWithFootprint(
        scene,
        assetRegistry,
        cache,
        choice.assetPath,
        position,
        yawDegrees,
        choice.targetFootprint);
}

void AddCityBuildingInstance(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    const CitySceneConfig& config,
    int blockX,
    int blockZ,
    int edgeIndex,
    Vec3 position,
    float yawDegrees)
{
    if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
    {
        AddGeneratedBuildingInstance(
            scene,
            assetRegistry,
            cache,
            config,
            blockX,
            blockZ,
            edgeIndex,
            position,
            yawDegrees,
            config.buildingQuadSize);
        return;
    }

    AddKenneyBuildingInstance(scene, assetRegistry, cache, blockX, blockZ, edgeIndex, position, yawDegrees);
}
}

void AddGroundTile(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    const CitySceneConfig& config,
    ModelCache& cache,
    int tx,
    int tz
)
{
    if (IsRoadTile(tx, tz))
    {
        return;
    }

    if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
    {
        AddGeneratedModelInstance(
            scene,
            cache,
            "generated/proc_city_ground_tile",
            MakeGeneratedProcCityGroundTileModel(kRoadTileSize),
            Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz)),
            0.0f
        );
        return;
    }

    AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/OBJ format/tile-low.obj", Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz)), 0.0f, kRoadTileSize);
}

static void AddRoadLights(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    const CitySceneConfig& config,
    int tx,
    int tz
)
{
    int stride = std::max(config.roadLightStride, 1);
    if ((((tx + kHalfCityTiles) / stride) + ((tz + kHalfCityTiles) / stride)) % stride != 0)
    {
        return;
    }

    const Vec3 center = Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz));
    if (tx % kRoadStrideTiles == 0)
    {
        Vec3 leftPosition = Vec3Add(center, Vec3Make(-kRoadLightOffset, 0.0f, 0.0f));
        Vec3 rightPosition = Vec3Add(center, Vec3Make(kRoadLightOffset, 0.0f, 0.0f));
        float leftYaw = 90.0f;
        float rightYaw = -90.0f;
        if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
        {
            AddGeneratedModelInstance(scene, cache, "generated/proc_city_street_light", MakeGeneratedProcCityStreetLightModel(), leftPosition, leftYaw);
            AddGeneratedModelInstance(scene, cache, "generated/proc_city_street_light", MakeGeneratedProcCityStreetLightModel(), rightPosition, rightYaw);
        }
        else
        {
            AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", leftPosition, leftYaw, 1.6f);
            AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", rightPosition, rightYaw, 1.6f);
        }
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
    if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
    {
        AddGeneratedModelInstance(scene, cache, "generated/proc_city_street_light", MakeGeneratedProcCityStreetLightModel(), nearPosition, nearYaw);
        AddGeneratedModelInstance(scene, cache, "generated/proc_city_street_light", MakeGeneratedProcCityStreetLightModel(), farPosition, farYaw);
    }
    else
    {
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", nearPosition, nearYaw, 1.6f);
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx", farPosition, farYaw, 1.6f);
    }
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

void AddRoadTile(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    const CitySceneConfig& config,
    int tx,
    int tz)
{
    if (!IsRoadTile(tx, tz))
    {
        return;
    }

    const Vec3 p = Vec3Make(TileCenter(tx), 0.0f, TileCenter(tz));
    if (IsIntersectionTile(tx, tz))
    {
        if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
        {
            AddGeneratedModelInstance(scene, cache, "generated/proc_city_crossroad", MakeGeneratedProcCityRoadTileModel(kRoadTileSize, true), p, 0.0f);
        }
        else
        {
            AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/road-crossroad.fbx", p, 0.0f, kRoadTileSize);
        }
        return;
    }

    if (config.buildingMode == CitySceneConfig::BuildingMode::Procedural)
    {
        AddGeneratedModelInstance(
            scene,
            cache,
            "generated/proc_city_road_straight",
            MakeGeneratedProcCityRoadTileModel(kRoadTileSize, false),
            p,
            tx % kRoadStrideTiles == 0 ? 90.0f : 0.0f);
    }
    else
    {
        AddModelInstanceWithFootprint(scene, assetRegistry, cache, "kenney/kenney_city-kit-roads/Models/FBX format/road-straight.fbx", p, tx % kRoadStrideTiles == 0 ? 90.0f : 0.0f, kRoadTileSize);
    }
    AddRoadLights(scene, assetRegistry, cache, config, tx, tz);
}

void AddBlockPerimeterBuildings(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int blockX,
    int blockZ,
    const CitySceneConfig& config
)
{
    const int baseTileX = blockX * kRoadStrideTiles;
    const int baseTileZ = blockZ * kRoadStrideTiles;

    for (int localX = 1; localX <= kLotsPerBlockSide; ++localX)
    {
        const int txNorth = baseTileX + localX;
        const int tzNorth = baseTileZ + 1;
        AddCityBuildingInstance(
            scene,
            assetRegistry,
            cache,
            config,
            blockX,
            blockZ,
            localX,
            Vec3Make(TileCenter(txNorth), 0.0f, TileCenter(tzNorth)),
            180.0f);

        const int txSouth = baseTileX + localX;
        const int tzSouth = baseTileZ + kLotsPerBlockSide;
        AddCityBuildingInstance(
            scene,
            assetRegistry,
            cache,
            config,
            blockX,
            blockZ,
            10 + localX,
            Vec3Make(TileCenter(txSouth), 0.0f, TileCenter(tzSouth)),
            0.0f);
    }

    for (int localZ = 2; localZ <= kLotsPerBlockSide - 1; ++localZ)
    {
        const int txWest = baseTileX + 1;
        const int tzWest = baseTileZ + localZ;
        AddCityBuildingInstance(
            scene,
            assetRegistry,
            cache,
            config,
            blockX,
            blockZ,
            20 + localZ,
            Vec3Make(TileCenter(txWest), 0.0f, TileCenter(tzWest)),
            90.0f);

        const int txEast = baseTileX + kLotsPerBlockSide;
        const int tzEast = baseTileZ + localZ;
        AddCityBuildingInstance(
            scene,
            assetRegistry,
            cache,
            config,
            blockX,
            blockZ,
            30 + localZ,
            Vec3Make(TileCenter(txEast), 0.0f, TileCenter(tzEast)),
            -90.0f);
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

void AddStreetWorld(SceneData& scene, const AssetRegistry& assetRegistry, const CitySceneConfig& config)
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
            AddGroundTile(scene, assetRegistry, config, cache, tx, tz);
            AddRoadTile(scene, assetRegistry, cache, config, tx, tz);
        }
    }

    for (int bz = minBlock; bz < maxBlockExclusive; ++bz)
    {
        for (int bx = minBlock; bx < maxBlockExclusive; ++bx)
        {
            AddBlockPerimeterBuildings(scene, assetRegistry, cache, bx, bz, config);
        }
    }

    AddStreetProps(scene, assetRegistry, cache);
    if (config.trafficVehiclesEnabled)
    {
        AddTrafficVehicles(scene, assetRegistry, cache, minTile, maxTile);
    }
}
}
