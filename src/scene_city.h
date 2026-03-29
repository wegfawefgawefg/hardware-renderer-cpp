#pragma once

#include "assets/asset_registry.h"
#include "scene.h"

struct CitySceneConfig
{
    float buildingQuadSize = 0.625f;
};

SceneData BuildSampleCity(const AssetRegistry& assetRegistry, const CitySceneConfig& config);
