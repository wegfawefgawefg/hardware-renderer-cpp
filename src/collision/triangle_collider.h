#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "math_types.h"
#include "scene.h"

struct TriangleMeshCollider
{
    struct BuildOptions
    {
        float cellSize = 1.5f;
        bool twoSided = false;
    };

    struct RayHit
    {
        bool hit = false;
        float distance = 0.0f;
        Vec3 position = {};
        Vec3 normal = {};
        std::uint32_t entityIndex = UINT32_MAX;
        std::uint32_t primitiveIndex = UINT32_MAX;
        Vec2 uv = {};
        float uvWorldScale = 1.0f;
    };

    struct SphereContact
    {
        Vec3 point = {};
        Vec3 normal = {};
        float penetration = 0.0f;
        std::uint32_t entityIndex = UINT32_MAX;
        std::uint32_t primitiveIndex = UINT32_MAX;
        Vec2 uv = {};
        float uvWorldScale = 1.0f;
    };

    void BuildFromScene(const SceneData& scene);
    void BuildFromScene(const SceneData& scene, const BuildOptions& options);
    RayHit Raycast(Vec3 origin, Vec3 direction, float maxDistance) const;
    RayHit RaycastDown(float x, float z, float yStart, float maxDistance) const;
    void GatherSphereContacts(Vec3 center, float radius, std::vector<SphereContact>& out) const;
    std::uint32_t TriangleCount() const;

    struct Tri
    {
        Vec3 a;
        Vec3 b;
        Vec3 c;
        Vec3 n;
        float minx = 0.0f;
        float maxx = 0.0f;
        float minz = 0.0f;
        float maxz = 0.0f;
        std::uint32_t entityIndex = UINT32_MAX;
        std::uint32_t primitiveIndex = UINT32_MAX;
        Vec2 uvA = {};
        Vec2 uvB = {};
        Vec2 uvC = {};
        float uvWorldScale = 1.0f;
    };

    struct CellKey
    {
        int x = 0;
        int z = 0;

        bool operator==(const CellKey& other) const
        {
            return x == other.x && z == other.z;
        }
    };

    struct CellKeyHasher
    {
        std::size_t operator()(const CellKey& key) const
        {
            std::size_t h0 = std::hash<int>{}(key.x);
            std::size_t h1 = std::hash<int>{}(key.z);
            return h0 ^ (h1 << 1);
        }
    };

    CellKey CellFor(float x, float z) const;
    void GatherCandidates(float x, float z, float radius, std::vector<std::uint32_t>& out, std::uint32_t stamp) const;

    float m_cellSize = 1.5f;
    bool m_twoSided = false;
    std::vector<Tri> m_tris;
    mutable std::vector<std::uint32_t> m_seenStamp;
    std::unordered_map<CellKey, std::vector<std::uint32_t>, CellKeyHasher> m_grid;
};
