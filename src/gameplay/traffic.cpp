#include "gameplay/traffic.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr float kRoadTileSize = 6.0f;
constexpr int kRoadStrideTiles = 4;
constexpr float kLaneOffset = 1.1f;
enum Direction
{
    kNorth = 0,
    kEast = 1,
    kSouth = 2,
    kWest = 3,
};

bool IsRoadTile(int tx, int tz)
{
    return tx % kRoadStrideTiles == 0 || tz % kRoadStrideTiles == 0;
}

bool IsIntersectionTile(int tx, int tz)
{
    return tx % kRoadStrideTiles == 0 && tz % kRoadStrideTiles == 0;
}

Vec3 TileBasePosition(int tx, int tz)
{
    return Vec3Make(static_cast<float>(tx) * kRoadTileSize, 0.0f, static_cast<float>(tz) * kRoadTileSize);
}

Vec3 DirectionVector(int direction)
{
    switch (direction)
    {
    case kNorth:
        return Vec3Make(0.0f, 0.0f, 1.0f);
    case kEast:
        return Vec3Make(1.0f, 0.0f, 0.0f);
    case kSouth:
        return Vec3Make(0.0f, 0.0f, -1.0f);
    default:
        return Vec3Make(-1.0f, 0.0f, 0.0f);
    }
}

Vec3 LaneOffset(int direction)
{
    switch (direction)
    {
    case kNorth:
        return Vec3Make(kLaneOffset, 0.0f, 0.0f);
    case kEast:
        return Vec3Make(0.0f, 0.0f, -kLaneOffset);
    case kSouth:
        return Vec3Make(-kLaneOffset, 0.0f, 0.0f);
    default:
        return Vec3Make(0.0f, 0.0f, kLaneOffset);
    }
}

float DirectionYawDegrees(int direction)
{
    switch (direction)
    {
    case kNorth:
        return 0.0f;
    case kEast:
        return 90.0f;
    case kSouth:
        return 180.0f;
    default:
        return -90.0f;
    }
}

std::uint32_t NextRandom(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

int InferDirection(const EntityData& entity)
{
    Vec4 forward4 = Mat4MulVec4(entity.transform, Vec4Make(0.0f, 0.0f, 1.0f, 0.0f));
    Vec3 forward = Vec3Make(forward4.x, 0.0f, forward4.z);
    if (std::fabs(forward.x) > std::fabs(forward.z))
    {
        return forward.x >= 0.0f ? kEast : kWest;
    }
    return forward.z >= 0.0f ? kNorth : kSouth;
}

int ChooseNextDirection(
    TrafficAgent& agent,
    const TrafficSystem& traffic,
    int currentTileX,
    int currentTileZ
)
{
    if (!IsIntersectionTile(currentTileX, currentTileZ))
    {
        return agent.direction;
    }

    std::array<int, 3> candidates = {
        agent.direction,
        (agent.direction + 1) & 3,
        (agent.direction + 3) & 3,
    };
    std::array<int, 3> valid = {-1, -1, -1};
    int validCount = 0;

    for (int candidate : candidates)
    {
        Vec3 delta = DirectionVector(candidate);
        int nextX = currentTileX + static_cast<int>(delta.x);
        int nextZ = currentTileZ + static_cast<int>(delta.z);
        if (!traffic.IsInBounds(nextX, nextZ) || !IsRoadTile(nextX, nextZ))
        {
            continue;
        }
        valid[validCount++] = candidate;
    }

    if (validCount == 0)
    {
        return (agent.direction + 2) & 3;
    }

    std::uint32_t random = NextRandom(agent.rngState);
    return valid[random % static_cast<std::uint32_t>(validCount)];
}

Mat4 BuildTrafficTransform(const TrafficAgent& agent)
{
    Vec3 start = TileBasePosition(agent.tx, agent.tz);
    Vec3 dir = DirectionVector(agent.direction);
    Vec3 along = Vec3Scale(dir, kRoadTileSize * agent.progress);
    Vec3 lane = LaneOffset(agent.direction);
    return Mat4Mul(
        Mat4Translate(Vec3Add(start, Vec3Add(along, lane))),
        Mat4Mul(
            Mat4RotateY(DegreesToRadians(DirectionYawDegrees(agent.direction))),
            Mat4Scale(agent.modelScale)
        )
    );
}
}

int TrafficSystem::CellIndex(int tx, int tz) const
{
    return (tz - m_minTile) * m_side + (tx - m_minTile);
}

bool TrafficSystem::IsInBounds(int tx, int tz) const
{
    return tx >= m_minTile && tx <= m_maxTile && tz >= m_minTile && tz <= m_maxTile;
}

void TrafficSystem::Initialize(SceneData& scene)
{
    m_agents.clear();
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    bool boundsValid = false;
    for (const EntityData& entity : scene.entities)
    {
        Vec3 position = Vec3Make(entity.transform.m[12], entity.transform.m[13], entity.transform.m[14]);
        if (!boundsValid)
        {
            minX = maxX = position.x;
            minZ = maxZ = position.z;
            boundsValid = true;
            continue;
        }
        minX = std::min(minX, position.x);
        maxX = std::max(maxX, position.x);
        minZ = std::min(minZ, position.z);
        maxZ = std::max(maxZ, position.z);
    }

    m_minTile = static_cast<int>(std::floor(std::min(minX, minZ) / kRoadTileSize)) - 2;
    m_maxTile = static_cast<int>(std::ceil(std::max(maxX, maxZ) / kRoadTileSize)) + 2;
    m_side = m_maxTile - m_minTile + 1;
    m_occupancy.assign(static_cast<std::size_t>(m_side * m_side), -1);

    std::uint32_t seed = 1;
    for (std::uint32_t entityIndex = 0; entityIndex < scene.entities.size(); ++entityIndex)
    {
        const EntityData& entity = scene.entities[entityIndex];
        if (!entity.traffic)
        {
            continue;
        }

        Vec3 position = Vec3Make(entity.transform.m[12], entity.transform.m[13], entity.transform.m[14]);
        int tx = static_cast<int>(std::round(position.x / kRoadTileSize));
        int tz = static_cast<int>(std::round(position.z / kRoadTileSize));
        if (!IsInBounds(tx, tz))
        {
            continue;
        }

        TrafficAgent agent{};
        agent.entityIndex = entityIndex;
        agent.tx = tx;
        agent.tz = tz;
        agent.direction = InferDirection(entity);
        agent.speedTilesPerSecond = 1.6f + static_cast<float>(seed % 5) * 0.18f;
        agent.modelScale = Vec3Length(Vec3Make(entity.transform.m[0], entity.transform.m[1], entity.transform.m[2]));
        agent.rngState = seed * 747796405u + 2891336453u;
        seed += 17u;
        m_agents.push_back(agent);
        m_occupancy[static_cast<std::size_t>(CellIndex(tx, tz))] = static_cast<int>(m_agents.size() - 1);
        scene.entities[entityIndex].transform = BuildTrafficTransform(m_agents.back());
    }
}

void TrafficSystem::Update(SceneData& scene, float dtSeconds, std::uint32_t stepBudget)
{
    std::uint32_t stepsUsed = 0;

    for (TrafficAgent& agent : m_agents)
    {
        agent.progress += dtSeconds * agent.speedTilesPerSecond;
        while (agent.progress >= 1.0f && stepsUsed < stepBudget)
        {
            int nextDirection = ChooseNextDirection(agent, *this, agent.tx, agent.tz);
            Vec3 delta = DirectionVector(nextDirection);
            int nextX = agent.tx + static_cast<int>(delta.x);
            int nextZ = agent.tz + static_cast<int>(delta.z);
            if (!IsInBounds(nextX, nextZ) || !IsRoadTile(nextX, nextZ))
            {
                agent.direction = (agent.direction + 2) & 3;
                agent.progress = 0.0f;
                break;
            }

            int nextCell = CellIndex(nextX, nextZ);
            if (m_occupancy[static_cast<std::size_t>(nextCell)] >= 0)
            {
                agent.progress = std::min(agent.progress, 0.95f);
                break;
            }

            const int oldCell = CellIndex(agent.tx, agent.tz);
            m_occupancy[static_cast<std::size_t>(oldCell)] = -1;
            agent.tx = nextX;
            agent.tz = nextZ;
            agent.direction = nextDirection;
            m_occupancy[static_cast<std::size_t>(nextCell)] = static_cast<int>(&agent - m_agents.data());
            agent.progress -= 1.0f;
            ++stepsUsed;
        }

        if (agent.entityIndex < scene.entities.size())
        {
            scene.entities[agent.entityIndex].transform = BuildTrafficTransform(agent);
        }
    }
}
