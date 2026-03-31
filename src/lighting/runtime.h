#pragma once

#include "render/renderer.h"
#include "state.h"

struct ManyLightsGridLayout
{
    Vec3 center = {};
    Vec3 extents = {};
    std::uint32_t countX = 1;
    std::uint32_t countY = 1;
    std::uint32_t countZ = 1;
    float stepX = 0.0f;
    float stepY = 0.0f;
    float stepZ = 0.0f;
};

ManyLightsGridLayout ComputeManyLightsGridLayout(const State& state, std::uint32_t lightCount);
void ApplyRuntimeLighting(State& state, SceneUniforms& uniforms, float dtSeconds);
