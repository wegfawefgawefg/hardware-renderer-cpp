#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "assets/asset_registry.h"
#include "math_types.h"

struct Vertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    std::array<std::uint16_t, 4> jointIndices = {0, 0, 0, 0};
    Vec4 jointWeights = {1.0f, 0.0f, 0.0f, 0.0f};
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct PrimitiveData
{
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t materialIndex = 0;
};

struct TextureData
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;
};

struct MaterialData
{
    std::string name;
    std::int32_t textureIndex = -1;
};

struct SpotLightData
{
    Vec3 position = {};
    float range = 0.0f;
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    float innerCos = 0.9f;
    Vec3 color = {1.0f, 1.0f, 1.0f};
    float outerCos = 0.8f;
    float intensity = 1.0f;
    float yawDegrees = 0.0f;
    float sourceOffsetScale = 1.0f;
};

struct ModelData
{
    MeshData mesh;
    std::vector<PrimitiveData> primitives;
    std::vector<MaterialData> materials;
    std::vector<TextureData> textures;
};

struct EntityData
{
    std::uint32_t modelIndex = 0;
    Mat4 transform = {};
    std::string assetPath;
    bool collidable = true;
    bool traffic = false;
    std::int32_t trafficDirection = -1;
};

struct SceneData
{
    std::vector<ModelData> models;
    std::vector<EntityData> entities;
    std::vector<SpotLightData> spotLights;
    struct VehicleLightTestItem
    {
        std::string assetPath;
        std::uint32_t entityIndex = 0;
        Vec3 origin = {};
        float scale = 1.0f;
        float selectionRadius = 1.0f;
    };
    std::vector<VehicleLightTestItem> vehicleLightTestItems;
};

struct SceneBounds
{
    bool valid = false;
    Vec3 min = {};
    Vec3 max = {};
    Vec3 center = {};
    float radius = 1.0f;
};

enum class SceneKind
{
    City,
    ShadowTest,
    SpotShadowTest,
    VehicleLightTest,
};

SceneBounds ComputeSceneBounds(const SceneData& scene);
std::uint32_t CountSceneTriangles(const SceneData& scene);
SceneData BuildVehicleLightTestScene(const AssetRegistry& assetRegistry);
SceneData LoadSampleScene(const AssetRegistry& assetRegistry, SceneKind kind);
