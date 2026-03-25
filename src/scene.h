#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

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
};

struct SceneData
{
    std::vector<ModelData> models;
    std::vector<EntityData> entities;
};

struct SceneBounds
{
    bool valid = false;
    Vec3 min = {};
    Vec3 max = {};
    Vec3 center = {};
    float radius = 1.0f;
};

SceneBounds ComputeSceneBounds(const SceneData& scene);
std::uint32_t CountSceneTriangles(const SceneData& scene);
SceneData LoadSampleScene();
