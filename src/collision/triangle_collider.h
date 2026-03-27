#pragma once

#include <cstdint>
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
        Vec3 boundsMin = {};
        Vec3 boundsMax = {};
        Vec3 centroid = {};
        std::uint32_t entityIndex = UINT32_MAX;
        std::uint32_t primitiveIndex = UINT32_MAX;
        Vec2 uvA = {};
        Vec2 uvB = {};
        Vec2 uvC = {};
        float uvWorldScale = 1.0f;
    };

    struct BvhNode
    {
        Vec3 boundsMin = {};
        Vec3 boundsMax = {};
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t leftChild = UINT32_MAX;
        std::uint32_t rightChild = UINT32_MAX;
    };

    std::uint32_t BuildBvhNode(std::uint32_t firstIndex, std::uint32_t indexCount);

    float m_cellSize = 1.5f;
    bool m_twoSided = false;
    std::vector<Tri> m_tris;
    std::vector<std::uint32_t> m_triIndices;
    std::vector<BvhNode> m_bvhNodes;
    std::uint32_t m_rootNode = UINT32_MAX;
};
