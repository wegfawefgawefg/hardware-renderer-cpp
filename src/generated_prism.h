#pragma once

#include "assets/asset_registry.h"
#include "scene.h"

struct GeneratedPrismLayout
{
    std::uint32_t segX = 1;
    std::uint32_t segY = 1;
    std::uint32_t segZ = 1;
};

GeneratedPrismLayout ComputeGeneratedPrismLayout(Vec3 halfExtents, float targetQuadSize);
ModelData MakeGeneratedPrismModel(
    const AssetRegistry& assetRegistry,
    Vec3 halfExtents,
    float targetQuadSize
);
ModelData MakeGeneratedProcCityModel(
    const AssetRegistry& assetRegistry,
    Vec3 halfExtents,
    float targetQuadSize
);
ModelData MakeGeneratedProcCityGroundTileModel(float tileSize);
ModelData MakeGeneratedProcCityRoadTileModel(float tileSize, bool intersection);
ModelData MakeGeneratedProcCityStreetLightModel();
