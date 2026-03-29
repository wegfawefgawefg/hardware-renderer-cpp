#include "collision/triangle_collider.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 out = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    if (std::fabs(out.w) > 1e-6f && out.w != 1.0f)
    {
        float invW = 1.0f / out.w;
        return Vec3Make(out.x * invW, out.y * invW, out.z * invW);
    }
    return Vec3Make(out.x, out.y, out.z);
}

Vec3 ClosestPointOnTriangle(Vec3 p, Vec3 a, Vec3 b, Vec3 c)
{
    Vec3 ab = Vec3Sub(b, a);
    Vec3 ac = Vec3Sub(c, a);
    Vec3 ap = Vec3Sub(p, a);

    float d1 = Vec3Dot(ab, ap);
    float d2 = Vec3Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
    {
        return a;
    }

    Vec3 bp = Vec3Sub(p, b);
    float d3 = Vec3Dot(ab, bp);
    float d4 = Vec3Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
    {
        return b;
    }

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return Vec3Add(a, Vec3Scale(ab, v));
    }

    Vec3 cp = Vec3Sub(p, c);
    float d5 = Vec3Dot(ab, cp);
    float d6 = Vec3Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
    {
        return c;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return Vec3Add(a, Vec3Scale(ac, w));
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        Vec3 bc = Vec3Sub(c, b);
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return Vec3Add(b, Vec3Scale(bc, w));
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return Vec3Add(a, Vec3Add(Vec3Scale(ab, v), Vec3Scale(ac, w)));
}

Vec3 BarycentricForPoint(Vec3 p, Vec3 a, Vec3 b, Vec3 c)
{
    Vec3 v0 = Vec3Sub(b, a);
    Vec3 v1 = Vec3Sub(c, a);
    Vec3 v2 = Vec3Sub(p, a);
    float d00 = Vec3Dot(v0, v0);
    float d01 = Vec3Dot(v0, v1);
    float d11 = Vec3Dot(v1, v1);
    float d20 = Vec3Dot(v2, v0);
    float d21 = Vec3Dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    if (std::fabs(denom) <= 1e-8f)
    {
        return Vec3Make(1.0f, 0.0f, 0.0f);
    }

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;
    return Vec3Make(u, v, w);
}

Vec2 InterpolateUv(const TriangleMeshCollider::Tri& tri, Vec3 p)
{
    Vec3 bary = BarycentricForPoint(p, tri.a, tri.b, tri.c);
    return Vec2Make(
        tri.uvA.x * bary.x + tri.uvB.x * bary.y + tri.uvC.x * bary.z,
        tri.uvA.y * bary.x + tri.uvB.y * bary.y + tri.uvC.y * bary.z
    );
}

float ComputeUvWorldScale(Vec3 a, Vec3 b, Vec3 c, Vec2 uvA, Vec2 uvB, Vec2 uvC)
{
    auto uvDistance = [](Vec2 lhs, Vec2 rhs)
    {
        float dx = lhs.x - rhs.x;
        float dy = lhs.y - rhs.y;
        return std::sqrt(dx * dx + dy * dy);
    };
    float worldAB = Vec3Length(Vec3Sub(b, a));
    float worldBC = Vec3Length(Vec3Sub(c, b));
    float worldCA = Vec3Length(Vec3Sub(a, c));
    float uvAB = uvDistance(uvB, uvA);
    float uvBC = uvDistance(uvC, uvB);
    float uvCA = uvDistance(uvA, uvC);

    float scaleSum = 0.0f;
    float weightSum = 0.0f;
    auto addEdge = [&](float worldLen, float uvLen)
    {
        if (uvLen > 1e-5f && worldLen > 1e-5f)
        {
            scaleSum += worldLen / uvLen;
            weightSum += 1.0f;
        }
    };
    addEdge(worldAB, uvAB);
    addEdge(worldBC, uvBC);
    addEdge(worldCA, uvCA);
    return weightSum > 0.0f ? scaleSum / weightSum : 1.0f;
}

Vec3 Vec3Min(Vec3 a, Vec3 b)
{
    return Vec3Make(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

Vec3 Vec3Max(Vec3 a, Vec3 b)
{
    return Vec3Make(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

bool SphereOverlapsAabb(Vec3 center, float radius, Vec3 boundsMin, Vec3 boundsMax)
{
    float d2 = 0.0f;
    auto axisDistance = [&](float value, float minValue, float maxValue)
    {
        if (value < minValue)
        {
            float d = minValue - value;
            d2 += d * d;
        }
        else if (value > maxValue)
        {
            float d = value - maxValue;
            d2 += d * d;
        }
    };
    axisDistance(center.x, boundsMin.x, boundsMax.x);
    axisDistance(center.y, boundsMin.y, boundsMax.y);
    axisDistance(center.z, boundsMin.z, boundsMax.z);
    return d2 <= radius * radius;
}

bool RayIntersectsAabb(
    Vec3 origin,
    Vec3 invDir,
    const std::array<bool, 3>& dirNegative,
    Vec3 boundsMin,
    Vec3 boundsMax,
    float maxDistance,
    float& outNear
)
{
    float tx0 = ((dirNegative[0] ? boundsMax.x : boundsMin.x) - origin.x) * invDir.x;
    float tx1 = ((dirNegative[0] ? boundsMin.x : boundsMax.x) - origin.x) * invDir.x;
    float ty0 = ((dirNegative[1] ? boundsMax.y : boundsMin.y) - origin.y) * invDir.y;
    float ty1 = ((dirNegative[1] ? boundsMin.y : boundsMax.y) - origin.y) * invDir.y;
    float tz0 = ((dirNegative[2] ? boundsMax.z : boundsMin.z) - origin.z) * invDir.z;
    float tz1 = ((dirNegative[2] ? boundsMin.z : boundsMax.z) - origin.z) * invDir.z;

    float tMin = std::max(std::max(tx0, ty0), std::max(tz0, 0.0f));
    float tMax = std::min(std::min(tx1, ty1), std::min(tz1, maxDistance));
    outNear = tMin;
    return tMax >= tMin;
}

bool RaycastTriangle(
    const TriangleMeshCollider::Tri& tri,
    Vec3 origin,
    Vec3 rayDir,
    float maxDistance,
    bool twoSided,
    float& outDistance
)
{
    Vec3 edge1 = Vec3Sub(tri.b, tri.a);
    Vec3 edge2 = Vec3Sub(tri.c, tri.a);
    Vec3 pvec = Vec3Cross(rayDir, edge2);
    float det = Vec3Dot(edge1, pvec);
    if (!twoSided)
    {
        if (det < 1e-8f)
        {
            return false;
        }
    }
    else if (std::fabs(det) < 1e-8f)
    {
        return false;
    }

    float invDet = 1.0f / det;
    Vec3 tvec = Vec3Sub(origin, tri.a);
    float u = Vec3Dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    Vec3 qvec = Vec3Cross(tvec, edge1);
    float v = Vec3Dot(rayDir, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }

    float t = Vec3Dot(edge2, qvec) * invDet;
    if (t < 0.0f || t > maxDistance)
    {
        return false;
    }

    outDistance = t;
    return true;
}

void ExpandBounds(Vec3 point, Vec3& ioMin, Vec3& ioMax)
{
    ioMin = Vec3Min(ioMin, point);
    ioMax = Vec3Max(ioMax, point);
}
}

void TriangleMeshCollider::BuildFromScene(const SceneData& scene, const BuildOptions& options)
{
    m_cellSize = options.cellSize > 1e-6f ? options.cellSize : 1.5f;
    m_twoSided = options.twoSided;
    m_tris.clear();
    m_triIndices.clear();
    m_bvhNodes.clear();
    m_rootNode = UINT32_MAX;

    for (std::uint32_t entityIndex = 0; entityIndex < scene.entities.size(); ++entityIndex)
    {
        const EntityData& entity = scene.entities[entityIndex];
        if (entity.modelIndex >= scene.models.size())
        {
            continue;
        }
        if (!entity.collidable)
        {
            continue;
        }

        const ModelData& model = scene.models[entity.modelIndex];
        const MeshData& mesh = model.mesh;
        for (std::uint32_t primitiveIndex = 0; primitiveIndex < model.primitives.size(); ++primitiveIndex)
        {
            const PrimitiveData& primitive = model.primitives[primitiveIndex];
            std::uint32_t endIndex = primitive.firstIndex + primitive.indexCount;
            for (std::uint32_t i = primitive.firstIndex; i + 2 < endIndex; i += 3)
            {
                std::uint32_t i0 = mesh.indices[i + 0];
                std::uint32_t i1 = mesh.indices[i + 1];
                std::uint32_t i2 = mesh.indices[i + 2];
                if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
                {
                    continue;
                }

                Vec3 a = TransformPoint(entity.transform, mesh.vertices[i0].position);
                Vec3 b = TransformPoint(entity.transform, mesh.vertices[i1].position);
                Vec3 c = TransformPoint(entity.transform, mesh.vertices[i2].position);

                Vec3 n = Vec3Cross(Vec3Sub(b, a), Vec3Sub(c, a));
                float nLength = Vec3Length(n);
                if (nLength <= 1e-10f)
                {
                    continue;
                }
                n = Vec3Scale(n, 1.0f / nLength);

                Tri tri{};
                tri.a = a;
                tri.b = b;
                tri.c = c;
                tri.n = n;
                tri.boundsMin = Vec3Min(a, Vec3Min(b, c));
                tri.boundsMax = Vec3Max(a, Vec3Max(b, c));
                tri.centroid = Vec3Scale(Vec3Add(a, Vec3Add(b, c)), 1.0f / 3.0f);
                tri.entityIndex = entityIndex;
                tri.primitiveIndex = primitiveIndex;
                tri.uvA = mesh.vertices[i0].uv;
                tri.uvB = mesh.vertices[i1].uv;
                tri.uvC = mesh.vertices[i2].uv;
                tri.uvWorldScale = ComputeUvWorldScale(a, b, c, tri.uvA, tri.uvB, tri.uvC);
                m_tris.push_back(tri);
            }
        }
    }

    m_triIndices.resize(m_tris.size());
    for (std::uint32_t i = 0; i < m_tris.size(); ++i)
    {
        m_triIndices[i] = i;
    }

    if (!m_tris.empty())
    {
        m_bvhNodes.reserve(m_tris.size() * 2);
        m_rootNode = BuildBvhNode(0, static_cast<std::uint32_t>(m_triIndices.size()));
    }
}

void TriangleMeshCollider::BuildFromScene(const SceneData& scene)
{
    BuildFromScene(scene, BuildOptions{});
}

std::uint32_t TriangleMeshCollider::BuildBvhNode(std::uint32_t firstIndex, std::uint32_t indexCount)
{
    BvhNode node{};
    node.firstIndex = firstIndex;
    node.indexCount = indexCount;

    Vec3 boundsMin = Vec3Make(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    Vec3 boundsMax = Vec3Make(
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max());
    Vec3 centroidMin = boundsMin;
    Vec3 centroidMax = boundsMax;
    for (std::uint32_t i = firstIndex; i < firstIndex + indexCount; ++i)
    {
        const Tri& tri = m_tris[m_triIndices[i]];
        ExpandBounds(tri.boundsMin, boundsMin, boundsMax);
        ExpandBounds(tri.boundsMax, boundsMin, boundsMax);
        ExpandBounds(tri.centroid, centroidMin, centroidMax);
    }
    node.boundsMin = boundsMin;
    node.boundsMax = boundsMax;

    std::uint32_t nodeIndex = static_cast<std::uint32_t>(m_bvhNodes.size());
    m_bvhNodes.push_back(node);

    constexpr std::uint32_t kLeafTriangleCount = 8;
    if (indexCount <= kLeafTriangleCount)
    {
        return nodeIndex;
    }

    Vec3 centroidExtent = Vec3Sub(centroidMax, centroidMin);
    int splitAxis = 0;
    if (centroidExtent.y > centroidExtent.x && centroidExtent.y >= centroidExtent.z)
    {
        splitAxis = 1;
    }
    else if (centroidExtent.z > centroidExtent.x && centroidExtent.z >= centroidExtent.y)
    {
        splitAxis = 2;
    }

    float axisExtent = splitAxis == 0 ? centroidExtent.x : (splitAxis == 1 ? centroidExtent.y : centroidExtent.z);
    if (axisExtent <= 1e-5f)
    {
        return nodeIndex;
    }

    auto centroidValue = [&](std::uint32_t triIndex)
    {
        const Vec3 centroid = m_tris[triIndex].centroid;
        return splitAxis == 0 ? centroid.x : (splitAxis == 1 ? centroid.y : centroid.z);
    };

    std::uint32_t mid = firstIndex + indexCount / 2;
    std::nth_element(
        m_triIndices.begin() + firstIndex,
        m_triIndices.begin() + mid,
        m_triIndices.begin() + firstIndex + indexCount,
        [&](std::uint32_t lhs, std::uint32_t rhs)
        {
            return centroidValue(lhs) < centroidValue(rhs);
        });

    if (mid == firstIndex || mid == firstIndex + indexCount)
    {
        return nodeIndex;
    }

    m_bvhNodes[nodeIndex].indexCount = 0;
    m_bvhNodes[nodeIndex].leftChild = BuildBvhNode(firstIndex, mid - firstIndex);
    m_bvhNodes[nodeIndex].rightChild = BuildBvhNode(mid, firstIndex + indexCount - mid);
    return nodeIndex;
}

TriangleMeshCollider::RayHit TriangleMeshCollider::Raycast(
    Vec3 origin,
    Vec3 direction,
    float maxDistance
) const
{
    RayHit best{};
    if (m_tris.empty() || m_rootNode == UINT32_MAX || maxDistance <= 0.0f)
    {
        return best;
    }

    Vec3 rayDir = Vec3Normalize(direction);
    if (Vec3Length(rayDir) <= 1e-6f)
    {
        return best;
    }

    Vec3 invDir = Vec3Make(
        std::fabs(rayDir.x) > 1e-8f ? 1.0f / rayDir.x : std::copysign(std::numeric_limits<float>::max(), rayDir.x == 0.0f ? 1.0f : rayDir.x),
        std::fabs(rayDir.y) > 1e-8f ? 1.0f / rayDir.y : std::copysign(std::numeric_limits<float>::max(), rayDir.y == 0.0f ? 1.0f : rayDir.y),
        std::fabs(rayDir.z) > 1e-8f ? 1.0f / rayDir.z : std::copysign(std::numeric_limits<float>::max(), rayDir.z == 0.0f ? 1.0f : rayDir.z));
    std::array<bool, 3> dirNegative = {rayDir.x < 0.0f, rayDir.y < 0.0f, rayDir.z < 0.0f};

    std::vector<std::uint32_t> stack;
    stack.reserve(64);
    stack.push_back(m_rootNode);

    float closestDistance = maxDistance;
    while (!stack.empty())
    {
        std::uint32_t nodeIndex = stack.back();
        stack.pop_back();
        const BvhNode& node = m_bvhNodes[nodeIndex];

        float nodeNear = 0.0f;
        if (!RayIntersectsAabb(origin, invDir, dirNegative, node.boundsMin, node.boundsMax, closestDistance, nodeNear))
        {
            continue;
        }

        if (node.indexCount > 0)
        {
            for (std::uint32_t i = node.firstIndex; i < node.firstIndex + node.indexCount; ++i)
            {
                const Tri& tri = m_tris[m_triIndices[i]];
                float hitDistance = 0.0f;
                if (!RaycastTriangle(tri, origin, rayDir, closestDistance, m_twoSided, hitDistance))
                {
                    continue;
                }

                closestDistance = hitDistance;
                best.hit = true;
                best.distance = hitDistance;
                best.position = Vec3Add(origin, Vec3Scale(rayDir, hitDistance));
                Vec3 hitNormal = tri.n;
                if (Vec3Dot(hitNormal, rayDir) > 0.0f)
                {
                    hitNormal = Vec3Scale(hitNormal, -1.0f);
                }
                best.normal = hitNormal;
                best.entityIndex = tri.entityIndex;
                best.primitiveIndex = tri.primitiveIndex;
                best.uv = InterpolateUv(tri, best.position);
                best.uvWorldScale = tri.uvWorldScale;
            }
            continue;
        }

        float leftNear = 0.0f;
        float rightNear = 0.0f;
        bool hitLeft = node.leftChild != UINT32_MAX &&
                       RayIntersectsAabb(
                           origin,
                           invDir,
                           dirNegative,
                           m_bvhNodes[node.leftChild].boundsMin,
                           m_bvhNodes[node.leftChild].boundsMax,
                           closestDistance,
                           leftNear);
        bool hitRight = node.rightChild != UINT32_MAX &&
                        RayIntersectsAabb(
                            origin,
                            invDir,
                            dirNegative,
                            m_bvhNodes[node.rightChild].boundsMin,
                            m_bvhNodes[node.rightChild].boundsMax,
                            closestDistance,
                            rightNear);

        if (hitLeft && hitRight)
        {
            if (leftNear < rightNear)
            {
                stack.push_back(node.rightChild);
                stack.push_back(node.leftChild);
            }
            else
            {
                stack.push_back(node.leftChild);
                stack.push_back(node.rightChild);
            }
        }
        else if (hitLeft)
        {
            stack.push_back(node.leftChild);
        }
        else if (hitRight)
        {
            stack.push_back(node.rightChild);
        }
    }

    return best;
}

TriangleMeshCollider::RayHit TriangleMeshCollider::RaycastDown(
    float x,
    float z,
    float yStart,
    float maxDistance
) const
{
    return Raycast(Vec3Make(x, yStart, z), Vec3Make(0.0f, -1.0f, 0.0f), maxDistance);
}

void TriangleMeshCollider::GatherSphereContacts(
    Vec3 center,
    float radius,
    std::vector<SphereContact>& out
) const
{
    out.clear();
    if (m_tris.empty() || m_rootNode == UINT32_MAX || radius <= 0.0f)
    {
        return;
    }

    std::vector<std::uint32_t> stack;
    stack.reserve(64);
    stack.push_back(m_rootNode);

    while (!stack.empty())
    {
        std::uint32_t nodeIndex = stack.back();
        stack.pop_back();
        const BvhNode& node = m_bvhNodes[nodeIndex];
        if (!SphereOverlapsAabb(center, radius, node.boundsMin, node.boundsMax))
        {
            continue;
        }

        if (node.indexCount > 0)
        {
            for (std::uint32_t i = node.firstIndex; i < node.firstIndex + node.indexCount; ++i)
            {
                const Tri& tri = m_tris[m_triIndices[i]];
                Vec3 closest = ClosestPointOnTriangle(center, tri.a, tri.b, tri.c);
                Vec3 delta = Vec3Sub(center, closest);
                float distance = Vec3Length(delta);
                if (distance >= radius)
                {
                    continue;
                }

                SphereContact contact{};
                contact.point = closest;
                if (distance > 1e-6f)
                {
                    contact.normal = Vec3Scale(delta, 1.0f / distance);
                    contact.penetration = radius - distance;
                }
                else
                {
                    contact.normal = tri.n;
                    contact.penetration = radius;
                }
                contact.entityIndex = tri.entityIndex;
                contact.primitiveIndex = tri.primitiveIndex;
                contact.uv = InterpolateUv(tri, closest);
                contact.uvWorldScale = tri.uvWorldScale;
                out.push_back(contact);
            }
            continue;
        }

        if (node.leftChild != UINT32_MAX)
        {
            stack.push_back(node.leftChild);
        }
        if (node.rightChild != UINT32_MAX)
        {
            stack.push_back(node.rightChild);
        }
    }
}

std::uint32_t TriangleMeshCollider::TriangleCount() const
{
    return static_cast<std::uint32_t>(m_tris.size());
}
