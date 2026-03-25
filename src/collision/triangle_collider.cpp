#include "collision/triangle_collider.h"

#include <algorithm>
#include <cmath>

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
}

TriangleMeshCollider::CellKey TriangleMeshCollider::CellFor(float x, float z) const
{
    return CellKey{
        static_cast<int>(std::floor(x / m_cellSize)),
        static_cast<int>(std::floor(z / m_cellSize)),
    };
}

void TriangleMeshCollider::BuildFromScene(const SceneData& scene, const BuildOptions& options)
{
    m_cellSize = options.cellSize > 1e-6f ? options.cellSize : 1.5f;
    m_twoSided = options.twoSided;
    m_tris.clear();
    m_seenStamp.clear();
    m_grid.clear();

    for (const EntityData& entity : scene.entities)
    {
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
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
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
            tri.minx = std::min({a.x, b.x, c.x});
            tri.maxx = std::max({a.x, b.x, c.x});
            tri.minz = std::min({a.z, b.z, c.z});
            tri.maxz = std::max({a.z, b.z, c.z});
            m_tris.push_back(tri);
        }
    }

    m_seenStamp.assign(m_tris.size(), 0);

    for (std::uint32_t i = 0; i < m_tris.size(); ++i)
    {
        const Tri& tri = m_tris[i];
        CellKey minCell = CellFor(tri.minx, tri.minz);
        CellKey maxCell = CellFor(tri.maxx, tri.maxz);
        for (int z = minCell.z; z <= maxCell.z; ++z)
        {
            for (int x = minCell.x; x <= maxCell.x; ++x)
            {
                m_grid[CellKey{x, z}].push_back(i);
            }
        }
    }
}

void TriangleMeshCollider::BuildFromScene(const SceneData& scene)
{
    BuildFromScene(scene, BuildOptions{});
}

void TriangleMeshCollider::GatherCandidates(
    float x,
    float z,
    float radius,
    std::vector<std::uint32_t>& out,
    std::uint32_t stamp
) const
{
    out.clear();
    CellKey minCell = CellFor(x - radius, z - radius);
    CellKey maxCell = CellFor(x + radius, z + radius);
    for (int cellZ = minCell.z; cellZ <= maxCell.z; ++cellZ)
    {
        for (int cellX = minCell.x; cellX <= maxCell.x; ++cellX)
        {
            auto it = m_grid.find(CellKey{cellX, cellZ});
            if (it == m_grid.end())
            {
                continue;
            }

            for (std::uint32_t triIndex : it->second)
            {
                if (triIndex >= m_seenStamp.size())
                {
                    continue;
                }
                if (m_seenStamp[triIndex] == stamp)
                {
                    continue;
                }

                m_seenStamp[triIndex] = stamp;
                out.push_back(triIndex);
            }
        }
    }
}

TriangleMeshCollider::RayHit TriangleMeshCollider::RaycastDown(
    float x,
    float z,
    float yStart,
    float maxDistance
) const
{
    RayHit best{};
    if (m_tris.empty() || maxDistance <= 0.0f)
    {
        return best;
    }

    std::vector<std::uint32_t> candidates;
    candidates.reserve(256);
    static std::uint32_t stamp = 1;
    stamp = stamp == 0 ? 1 : stamp + 1;
    GatherCandidates(x, z, 0.01f, candidates, stamp);

    Vec3 rayOrigin = Vec3Make(x, yStart, z);
    Vec3 rayDir = Vec3Make(0.0f, -1.0f, 0.0f);

    for (std::uint32_t triIndex : candidates)
    {
        const Tri& tri = m_tris[triIndex];
        float denom = Vec3Dot(tri.n, rayDir);
        if (std::fabs(denom) < 1e-8f)
        {
            continue;
        }

        float t = Vec3Dot(tri.n, Vec3Sub(tri.a, rayOrigin)) / denom;
        if (t < 0.0f || t > maxDistance)
        {
            continue;
        }

        Vec3 p = Vec3Add(rayOrigin, Vec3Scale(rayDir, t));
        Vec3 ab = Vec3Sub(tri.b, tri.a);
        Vec3 bc = Vec3Sub(tri.c, tri.b);
        Vec3 ca = Vec3Sub(tri.a, tri.c);
        Vec3 ap = Vec3Sub(p, tri.a);
        Vec3 bp = Vec3Sub(p, tri.b);
        Vec3 cp = Vec3Sub(p, tri.c);

        float e0 = Vec3Dot(tri.n, Vec3Cross(ab, ap));
        float e1 = Vec3Dot(tri.n, Vec3Cross(bc, bp));
        float e2 = Vec3Dot(tri.n, Vec3Cross(ca, cp));
        if (!m_twoSided)
        {
            if (e0 < -1e-5f || e1 < -1e-5f || e2 < -1e-5f)
            {
                continue;
            }
        }
        else if (!((e0 >= -1e-5f && e1 >= -1e-5f && e2 >= -1e-5f) ||
                   (e0 <= 1e-5f && e1 <= 1e-5f && e2 <= 1e-5f)))
        {
            continue;
        }

        if (!best.hit || t < best.distance)
        {
            best.hit = true;
            best.distance = t;
            best.position = p;
            best.normal = tri.n;
        }
    }

    return best;
}

void TriangleMeshCollider::GatherSphereContacts(
    Vec3 center,
    float radius,
    std::vector<SphereContact>& out
) const
{
    out.clear();
    if (m_tris.empty() || radius <= 0.0f)
    {
        return;
    }

    std::vector<std::uint32_t> candidates;
    candidates.reserve(256);
    static std::uint32_t stamp = 1;
    stamp = stamp == 0 ? 1 : stamp + 1;
    GatherCandidates(center.x, center.z, radius, candidates, stamp);

    for (std::uint32_t triIndex : candidates)
    {
        const Tri& tri = m_tris[triIndex];
        if (center.y + radius < std::min({tri.a.y, tri.b.y, tri.c.y}) - radius)
        {
            continue;
        }
        if (center.y - radius > std::max({tri.a.y, tri.b.y, tri.c.y}) + radius)
        {
            continue;
        }

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
        out.push_back(contact);
    }
}

std::uint32_t TriangleMeshCollider::TriangleCount() const
{
    return static_cast<std::uint32_t>(m_tris.size());
}
