#pragma once

#include "assets/asset_registry.h"
#include "scene.h"

struct CitySceneConfig
{
    enum class BuildingMode
    {
        Kenney,
        Procedural,
    };

    BuildingMode buildingMode = BuildingMode::Kenney;
    float buildingQuadSize = 0.625f;
    int roadLightStride = 1;
    bool trafficVehiclesEnabled = true;
};

SceneData BuildSampleCity(const AssetRegistry& assetRegistry, const CitySceneConfig& config);
