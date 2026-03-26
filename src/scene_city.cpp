#include "scene_city.h"

#include "scene_city_internal.h"

SceneData BuildSampleCity(const AssetRegistry& assetRegistry)
{
    SceneData scene{};
    city_internal::AddStreetWorld(scene, assetRegistry);
    return scene;
}
