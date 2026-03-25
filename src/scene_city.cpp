#include "scene_city.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "assets/fbx_loader.h"
#include "assets/texture_loader.h"

namespace
{
using ModelCache = std::unordered_map<std::string, std::uint32_t>;

constexpr float kRoadTileSize = 6.0f;
constexpr int kLotsPerBlockSide = 3;
constexpr int kRoadStrideTiles = kLotsPerBlockSide + 1;
constexpr int kCityTilesPerSide = 32;
constexpr int kHalfCityTiles = kCityTilesPerSide / 2;
constexpr float kVehicleFootprint = 4.5f;
constexpr float kBuildingFootprint = 5.0f;
constexpr float kRoadLightOffset = kRoadTileSize * 0.48f;

Mat4 PlacementTransform(Vec3 position, float yawDegrees, float scale = 1.0f)
{
    return Mat4Mul(
        Mat4Translate(position),
        Mat4Mul(Mat4RotateY(DegreesToRadians(yawDegrees)), Mat4Scale(scale))
    );
}

void AddGroundPlane(SceneData& scene, Vec3 center, float y, float extent)
{
    ModelData ground{};
    ground.materials.push_back(MaterialData{
        .name = "ground",
        .textureIndex = 0,
    });
    ground.textures.push_back(MakeSolidTexture(110, 116, 128, 255));

    auto pushVertex = [&](float x, float z, float u, float v) {
        Vertex vertex{};
        vertex.position = Vec3Make(x, y, z);
        vertex.normal = Vec3Make(0.0f, 1.0f, 0.0f);
        vertex.uv = Vec2Make(u, v);
        ground.mesh.vertices.push_back(vertex);
    };

    float x0 = center.x - extent;
    float x1 = center.x + extent;
    float z0 = center.z - extent;
    float z1 = center.z + extent;
    pushVertex(x0, z0, 0.0f, 0.0f);
    pushVertex(x1, z0, 1.0f, 0.0f);
    pushVertex(x1, z1, 1.0f, 1.0f);
    pushVertex(x0, z1, 0.0f, 1.0f);

    ground.mesh.indices = {0, 1, 2, 0, 2, 3};
    ground.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = 6,
        .materialIndex = 0,
    });

    std::uint32_t modelIndex = static_cast<std::uint32_t>(scene.models.size());
    scene.models.push_back(std::move(ground));
    scene.entities.push_back(EntityData{
        .modelIndex = modelIndex,
        .transform = Mat4Identity(),
    });
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

std::uint32_t AddModelInstanceWithFootprint(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    std::string_view relativePath,
    Vec3 position,
    float yawDegrees,
    float targetFootprint,
    bool collidable = true,
    bool traffic = false
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

        ModelData model = LoadFbxModel(path->string());
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

    const float footprint = ComputeModelFootprint(scene.models[modelIndex]);
    const float scale = targetFootprint / footprint;
    scene.entities.push_back(EntityData{
        .modelIndex = modelIndex,
        .transform = PlacementTransform(position, yawDegrees, scale),
        .collidable = collidable,
        .traffic = traffic,
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

void AddRoadLights(
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
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx",
            Vec3Add(center, Vec3Make(-kRoadLightOffset, 0.0f, 0.0f)),
            90.0f,
            1.6f
        );
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx",
            Vec3Add(center, Vec3Make(kRoadLightOffset, 0.0f, 0.0f)),
            -90.0f,
            1.6f
        );
        return;
    }

    AddModelInstanceWithFootprint(
        scene,
        assetRegistry,
        cache,
        "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx",
        Vec3Add(center, Vec3Make(0.0f, 0.0f, -kRoadLightOffset)),
        0.0f,
        1.6f
    );
    AddModelInstanceWithFootprint(
        scene,
        assetRegistry,
        cache,
        "kenney/kenney_city-kit-roads/Models/FBX format/light-square.fbx",
        Vec3Add(center, Vec3Make(0.0f, 0.0f, kRoadLightOffset)),
        180.0f,
        1.6f
    );
}

void AddHighwaySigns(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int minTile,
    int maxTile
)
{
    for (int tx = minTile + 8; tx <= maxTile - 8; tx += 16)
    {
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            "kenney/kenney_city-kit-roads/Models/FBX format/sign-highway-wide.fbx",
            Vec3Make(TileCenter(tx), 0.0f, TileCenter(4)),
            180.0f,
            5.0f
        );
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            "kenney/kenney_city-kit-roads/Models/FBX format/sign-highway-wide.fbx",
            Vec3Make(TileCenter(tx), 0.0f, TileCenter(-4)),
            0.0f,
            5.0f
        );
    }
}

void AddRoadTile(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int tx,
    int tz
)
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
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            ChooseBuildingAsset(blockX, blockZ, localX),
            Vec3Make(TileCenter(txNorth), 0.0f, TileCenter(tzNorth)),
            180.0f,
            kBuildingFootprint
        );

        const int txSouth = baseTileX + localX;
        const int tzSouth = baseTileZ + kLotsPerBlockSide;
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            ChooseBuildingAsset(blockX, blockZ, 10 + localX),
            Vec3Make(TileCenter(txSouth), 0.0f, TileCenter(tzSouth)),
            0.0f,
            kBuildingFootprint
        );
    }

    for (int localZ = 2; localZ <= kLotsPerBlockSide - 1; ++localZ)
    {
        const int txWest = baseTileX + 1;
        const int tzWest = baseTileZ + localZ;
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            ChooseBuildingAsset(blockX, blockZ, 20 + localZ),
            Vec3Make(TileCenter(txWest), 0.0f, TileCenter(tzWest)),
            90.0f,
            kBuildingFootprint
        );

        const int txEast = baseTileX + kLotsPerBlockSide;
        const int tzEast = baseTileZ + localZ;
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            ChooseBuildingAsset(blockX, blockZ, 30 + localZ),
            Vec3Make(TileCenter(txEast), 0.0f, TileCenter(tzEast)),
            -90.0f,
            kBuildingFootprint
        );
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
    for (int tx = minTile + 2; tx <= maxTile - 2; tx += 8)
    {
        if (tx % kRoadStrideTiles == 0)
        {
            continue;
        }

        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            kVehicles[vehicleIndex % kVehicleCount],
            Vec3Make(TileCenter(tx), 0.0f, TileCenter(-4)),
            0.0f,
            kVehicleFootprint,
            false,
            true
        );
        ++vehicleIndex;
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            kVehicles[vehicleIndex % kVehicleCount],
            Vec3Make(TileCenter(tx), 0.0f, TileCenter(4)),
            180.0f,
            kVehicleFootprint,
            false,
            true
        );
        ++vehicleIndex;
    }

    for (int tz = minTile + 2; tz <= maxTile - 2; tz += 8)
    {
        if (tz % kRoadStrideTiles == 0)
        {
            continue;
        }

        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            kVehicles[vehicleIndex % kVehicleCount],
            Vec3Make(TileCenter(-4), 0.0f, TileCenter(tz)),
            0.0f,
            kVehicleFootprint,
            false,
            true
        );
        ++vehicleIndex;
        AddModelInstanceWithFootprint(
            scene,
            assetRegistry,
            cache,
            kVehicles[vehicleIndex % kVehicleCount],
            Vec3Make(TileCenter(4), 0.0f, TileCenter(tz)),
            180.0f,
            kVehicleFootprint,
            false,
            true
        );
        ++vehicleIndex;
    }
}

void AddStreetProps(
    SceneData& scene,
    const AssetRegistry& assetRegistry,
    ModelCache& cache,
    int minTile,
    int maxTile
)
{
    AddHighwaySigns(scene, assetRegistry, cache, minTile, maxTile);

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

    AddStreetProps(scene, assetRegistry, cache, minTile, maxTile);
    AddTrafficVehicles(scene, assetRegistry, cache, minTile, maxTile);
}
}

SceneData BuildSampleCity(const AssetRegistry& assetRegistry)
{
    SceneData scene{};
    const float halfExtent = TileCenter(kHalfCityTiles) + kRoadTileSize * 4.0f;
    AddGroundPlane(scene, Vec3Make(0.0f, 0.0f, 0.0f), -0.02f, halfExtent);
    AddStreetWorld(scene, assetRegistry);
    return scene;
}
