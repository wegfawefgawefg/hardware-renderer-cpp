#include "gameplay/traffic.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
        return Vec3Make(-kLaneOffset, 0.0f, 0.0f);
    case kEast:
        return Vec3Make(0.0f, 0.0f, kLaneOffset);
    case kSouth:
        return Vec3Make(kLaneOffset, 0.0f, 0.0f);
    default:
        return Vec3Make(0.0f, 0.0f, -kLaneOffset);
    }
}

float DirectionYawDegrees(int direction)
{
    switch (direction)
    {
    case kNorth:
        return 180.0f;
    case kEast:
        return -90.0f;
    case kSouth:
        return 0.0f;
    default:
        return 90.0f;
    }
}

float WrapDegrees(float degrees)
{
    while (degrees > 180.0f)
    {
        degrees -= 360.0f;
    }
    while (degrees < -180.0f)
    {
        degrees += 360.0f;
    }
    return degrees;
}

float RotateTowardDegrees(float current, float target, float maxStep)
{
    float delta = WrapDegrees(target - current);
    if (std::fabs(delta) <= maxStep)
    {
        return target;
    }
    return current + (delta > 0.0f ? maxStep : -maxStep);
}

std::uint32_t NextRandom(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

bool TrafficLoggingEnabled()
{
    const char* value = std::getenv("HR_TRAFFIC_LOG");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

const char* DirectionName(int direction)
{
    switch (direction)
    {
    case kNorth:
        return "north";
    case kEast:
        return "east";
    case kSouth:
        return "south";
    default:
        return "west";
    }
}

int LaneCellIndex(const TrafficSystem& traffic, int tx, int tz, int direction)
{
    return traffic.CellIndex(tx, tz) * 4 + direction;
}

Vec3 SegmentStartPosition(const TrafficAgent& agent)
{
    return Vec3Add(TileBasePosition(agent.tx, agent.tz), LaneOffset(agent.direction));
}

Vec3 SegmentEndPosition(const TrafficAgent& agent)
{
    if (!agent.reservedNext)
    {
        return SegmentStartPosition(agent);
    }
    return Vec3Add(TileBasePosition(agent.nextTx, agent.nextTz), LaneOffset(agent.nextDirection));
}

Mat4 BuildTrafficTransform(const TrafficAgent& agent)
{
    Vec3 start = SegmentStartPosition(agent);
    Vec3 end = SegmentEndPosition(agent);
    float t = std::clamp(agent.progress, 0.0f, 1.0f);
    float smoothT = t * t * (3.0f - 2.0f * t);
    Vec3 position = Vec3Add(start, Vec3Scale(Vec3Sub(end, start), smoothT));
    return Mat4Mul(
        Mat4Translate(position),
        Mat4Mul(Mat4RotateY(DegreesToRadians(agent.yawDegrees)), Mat4Scale(agent.modelScale))
    );
}

int ChooseNextDirection(TrafficAgent& agent, const TrafficSystem& traffic)
{
    if (!IsIntersectionTile(agent.tx, agent.tz))
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
        int nextX = agent.tx + static_cast<int>(delta.x);
        int nextZ = agent.tz + static_cast<int>(delta.z);
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

bool TryReserveNextCell(TrafficSystem& traffic, TrafficAgent& agent)
{
    int nextDirection = ChooseNextDirection(agent, traffic);
    Vec3 delta = DirectionVector(nextDirection);
    int nextX = agent.tx + static_cast<int>(delta.x);
    int nextZ = agent.tz + static_cast<int>(delta.z);

    if (!traffic.IsInBounds(nextX, nextZ) || !IsRoadTile(nextX, nextZ))
    {
        nextDirection = (agent.direction + 2) & 3;
        delta = DirectionVector(nextDirection);
        nextX = agent.tx + static_cast<int>(delta.x);
        nextZ = agent.tz + static_cast<int>(delta.z);
        if (!traffic.IsInBounds(nextX, nextZ) || !IsRoadTile(nextX, nextZ))
        {
            return false;
        }
    }

    const int nextLaneCell = LaneCellIndex(traffic, nextX, nextZ, nextDirection);
    if (traffic.m_occupancy[static_cast<std::size_t>(nextLaneCell)] >= 0)
    {
        return false;
    }

    const int currentLaneCell = LaneCellIndex(traffic, agent.tx, agent.tz, agent.direction);
    traffic.m_occupancy[static_cast<std::size_t>(currentLaneCell)] = -1;
    traffic.m_occupancy[static_cast<std::size_t>(nextLaneCell)] = static_cast<int>(&agent - traffic.m_agents.data());
    agent.nextTx = nextX;
    agent.nextTz = nextZ;
    agent.nextDirection = nextDirection;
    agent.targetYawDegrees = DirectionYawDegrees(nextDirection);
    agent.progress = 0.0f;
    agent.reservedNext = true;
    return true;
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
    m_occupancy.assign(static_cast<std::size_t>(m_side * m_side * 4), -1);

    std::uint32_t seed = 1;
    const bool logTraffic = TrafficLoggingEnabled();
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
        agent.direction = entity.trafficDirection >= 0 ? entity.trafficDirection : kEast;
        agent.nextTx = tx;
        agent.nextTz = tz;
        agent.nextDirection = agent.direction;
        agent.speedTilesPerSecond = 0.55f + static_cast<float>(seed % 5) * 0.05f;
        agent.modelScale = Vec3Length(Vec3Make(entity.transform.m[0], entity.transform.m[1], entity.transform.m[2]));
        agent.yawDegrees = DirectionYawDegrees(agent.direction);
        agent.targetYawDegrees = agent.yawDegrees;
        agent.rngState = seed * 747796405u + 2891336453u;
        seed += 17u;
        m_agents.push_back(agent);
        const int laneCell = LaneCellIndex(*this, tx, tz, agent.direction);
        m_occupancy[static_cast<std::size_t>(laneCell)] = static_cast<int>(m_agents.size() - 1);
        scene.entities[entityIndex].transform = BuildTrafficTransform(m_agents.back());

        if (logTraffic)
        {
            std::fprintf(
                stderr,
                "traffic init entity=%u tile=(%d,%d) dir=%s scale=%.2f speed=%.2f\n",
                entityIndex,
                tx,
                tz,
                DirectionName(agent.direction),
                agent.modelScale,
                agent.speedTilesPerSecond
            );
        }
    }
}

void TrafficSystem::Update(SceneData& scene, float dtSeconds, std::uint32_t stepBudget)
{
    std::uint32_t stepsUsed = 0;

    for (TrafficAgent& agent : m_agents)
    {
        if (!agent.reservedNext && stepsUsed < stepBudget)
        {
            if (TryReserveNextCell(*this, agent))
            {
                ++stepsUsed;
            }
        }

        agent.yawDegrees = RotateTowardDegrees(
            agent.yawDegrees,
            agent.targetYawDegrees,
            dtSeconds * 220.0f
        );

        if (agent.reservedNext)
        {
            agent.progress += dtSeconds * agent.speedTilesPerSecond;
            if (agent.progress >= 1.0f)
            {
                agent.tx = agent.nextTx;
                agent.tz = agent.nextTz;
                agent.direction = agent.nextDirection;
                agent.yawDegrees = agent.targetYawDegrees;
                agent.progress = 0.0f;
                agent.reservedNext = false;
            }
        }

        if (agent.entityIndex < scene.entities.size())
        {
            scene.entities[agent.entityIndex].transform = BuildTrafficTransform(agent);
        }
    }
}
