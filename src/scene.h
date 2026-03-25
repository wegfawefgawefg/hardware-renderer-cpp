#pragma once

#include <cstdint>
#include <vector>

#include "math_types.h"

struct Vertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct TextureData
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;
};

struct SceneData
{
    MeshData mesh;
    TextureData texture;
};

SceneData LoadSampleScene();
