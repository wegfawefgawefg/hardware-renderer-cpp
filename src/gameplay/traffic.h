#pragma once

#include <cstdint>
#include <vector>

#include "scene.h"

struct TrafficAgent
{
    std::uint32_t entityIndex = 0;
    int tx = 0;
    int tz = 0;
    int direction = 0;
    float progress = 0.0f;
    float speedTilesPerSecond = 1.8f;
    float modelScale = 1.0f;
    std::uint32_t rngState = 1;
};

struct TrafficSystem
{
    void Initialize(SceneData& scene);
    void Update(SceneData& scene, float dtSeconds, std::uint32_t stepBudget);

    int CellIndex(int tx, int tz) const;
    bool IsInBounds(int tx, int tz) const;

    int m_minTile = 0;
    int m_maxTile = -1;
    int m_side = 0;
    std::vector<int> m_occupancy;
    std::vector<TrafficAgent> m_agents;
};
