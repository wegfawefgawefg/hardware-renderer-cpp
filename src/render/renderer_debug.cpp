#include "render/renderer.h"
#include "render/internal.h"

#include <algorithm>
#include <array>

#include <cmath>

namespace
{
constexpr float kPi = 3.1415926535f;

void AppendLine(std::span<LightMarkerVertex> lines, std::uint32_t& count, Vec3 a, Vec3 b, Vec3 color)
{
    if (count + 1 >= lines.size())
    {
        return;
    }
    lines[count++] = LightMarkerVertex{a, color};
    lines[count++] = LightMarkerVertex{b, color};
}

void AppendCircle(
    std::span<LightMarkerVertex> lines,
    std::uint32_t& count,
    Vec3 center,
    Vec3 axisA,
    Vec3 axisB,
    float radius,
    Vec3 color,
    int segments
)
{
    for (int i = 0; i < segments; ++i)
    {
        float t0 = static_cast<float>(i) / static_cast<float>(segments);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
        float a0 = t0 * 2.0f * kPi;
        float a1 = t1 * 2.0f * kPi;
        Vec3 p0 = Vec3Add(center, Vec3Add(Vec3Scale(axisA, std::cos(a0) * radius), Vec3Scale(axisB, std::sin(a0) * radius)));
        Vec3 p1 = Vec3Add(center, Vec3Add(Vec3Scale(axisA, std::cos(a1) * radius), Vec3Scale(axisB, std::sin(a1) * radius)));
        AppendLine(lines, count, p0, p1, color);
    }
}

void AppendSphere(std::span<LightMarkerVertex> lines, std::uint32_t& count, Vec3 center, float radius, Vec3 color)
{
    constexpr int kSegments = 24;
    AppendCircle(lines, count, center, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 1.0f, 0.0f), radius, color, kSegments);
    AppendCircle(lines, count, center, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
    AppendCircle(lines, count, center, Vec3Make(0.0f, 1.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
}

void AppendCone(
    std::span<LightMarkerVertex> lines,
    std::uint32_t& count,
    Vec3 apex,
    Vec3 direction,
    float range,
    float outerCos,
    Vec3 color
)
{
    constexpr int kSegments = 20;
    Vec3 dir = Vec3Normalize(direction);
    if (Vec3Length(dir) <= 0.0001f || range <= 0.0001f)
    {
        return;
    }

    float outerAngle = std::acos(std::clamp(outerCos, -1.0f, 1.0f));
    float baseRadius = std::tan(outerAngle) * range;
    Vec3 center = Vec3Add(apex, Vec3Scale(dir, range));
    Vec3 up = std::fabs(dir.y) < 0.95f ? Vec3Make(0.0f, 1.0f, 0.0f) : Vec3Make(1.0f, 0.0f, 0.0f);
    Vec3 right = Vec3Normalize(Vec3Cross(dir, up));
    Vec3 forward = Vec3Normalize(Vec3Cross(right, dir));

    AppendCircle(lines, count, center, right, forward, baseRadius, color, kSegments);
    AppendLine(lines, count, apex, center, color);
    constexpr std::array<float, 4> kAngles = {0.0f, 0.5f * kPi, kPi, 1.5f * kPi};
    for (float angle : kAngles)
    {
        Vec3 rim = Vec3Add(center, Vec3Add(Vec3Scale(right, std::cos(angle) * baseRadius), Vec3Scale(forward, std::sin(angle) * baseRadius)));
        AppendLine(lines, count, apex, rim, color);
    }
}

void AppendCylinder(
    std::span<LightMarkerVertex> lines,
    std::uint32_t& count,
    Vec3 baseCenter,
    float radius,
    float height,
    Vec3 color
)
{
    constexpr int kSegments = 24;
    Vec3 topCenter = Vec3Add(baseCenter, Vec3Make(0.0f, height, 0.0f));
    AppendCircle(lines, count, baseCenter, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
    AppendCircle(lines, count, topCenter, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
    constexpr std::array<float, 4> kAngles = {0.0f, 0.5f * kPi, kPi, 1.5f * kPi};
    for (float angle : kAngles)
    {
        Vec3 offset = Vec3Make(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        AppendLine(lines, count, Vec3Add(baseCenter, offset), Vec3Add(topCenter, offset), color);
    }
}

void AppendTriangle(std::span<LightMarkerVertex> solids, std::uint32_t& count, Vec3 a, Vec3 b, Vec3 c, Vec3 color)
{
    if (count + 2 >= solids.size())
    {
        return;
    }
    solids[count++] = LightMarkerVertex{a, color};
    solids[count++] = LightMarkerVertex{b, color};
    solids[count++] = LightMarkerVertex{c, color};
}

void AppendCube(std::span<LightMarkerVertex> solids, std::uint32_t& count, Vec3 center, float halfExtent, Vec3 color)
{
    Vec3 p000 = Vec3Add(center, Vec3Make(-halfExtent, -halfExtent, -halfExtent));
    Vec3 p001 = Vec3Add(center, Vec3Make(-halfExtent, -halfExtent, halfExtent));
    Vec3 p010 = Vec3Add(center, Vec3Make(-halfExtent, halfExtent, -halfExtent));
    Vec3 p011 = Vec3Add(center, Vec3Make(-halfExtent, halfExtent, halfExtent));
    Vec3 p100 = Vec3Add(center, Vec3Make(halfExtent, -halfExtent, -halfExtent));
    Vec3 p101 = Vec3Add(center, Vec3Make(halfExtent, -halfExtent, halfExtent));
    Vec3 p110 = Vec3Add(center, Vec3Make(halfExtent, halfExtent, -halfExtent));
    Vec3 p111 = Vec3Add(center, Vec3Make(halfExtent, halfExtent, halfExtent));

    AppendTriangle(solids, count, p001, p101, p111, color);
    AppendTriangle(solids, count, p001, p111, p011, color);
    AppendTriangle(solids, count, p100, p000, p010, color);
    AppendTriangle(solids, count, p100, p010, p110, color);
    AppendTriangle(solids, count, p000, p001, p011, color);
    AppendTriangle(solids, count, p000, p011, p010, color);
    AppendTriangle(solids, count, p101, p100, p110, color);
    AppendTriangle(solids, count, p101, p110, p111, color);
    AppendTriangle(solids, count, p010, p011, p111, color);
    AppendTriangle(solids, count, p010, p111, p110, color);
    AppendTriangle(solids, count, p000, p100, p101, color);
    AppendTriangle(solids, count, p000, p101, p001, color);
}
}

void VulkanRenderer::BuildDebugLightGeometry(
    const SceneUniforms& uniforms,
    const DebugRenderOptions& debug,
    std::span<LightMarkerVertex> lightMarkers,
    std::span<LightMarkerVertex> lightLines,
    std::span<LightMarkerVertex> lightSolids
)
{
    m_lightLineVertexCount = 0;
    m_lightSolidVertexCount = 0;

    for (std::size_t i = 0; i < 4; ++i)
    {
        const bool showProxy = debug.drawLightProxies && uniforms.lightPositions[i].w > 0.0001f && uniforms.lightPositions[i].y > -999.0f;
        lightMarkers[i].position = debug.drawLightMarkers
            ? Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z)
            : Vec3Make(0.0f, -10000.0f, 0.0f);
        lightMarkers[i].color = Vec3Make(uniforms.lightColors[i].x, uniforms.lightColors[i].y, uniforms.lightColors[i].z);
        if (debug.drawLightVolumes && uniforms.lightPositions[i].w > 0.0001f && uniforms.lightPositions[i].y > -999.0f)
        {
            AppendSphere(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z),
                uniforms.lightPositions[i].w,
                lightMarkers[i].color
            );
        }
        if (showProxy)
        {
            AppendCube(
                lightSolids,
                m_lightSolidVertexCount,
                Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z),
                0.10f,
                lightMarkers[i].color
            );
        }
    }

    for (std::size_t i = 0; i < 2; ++i)
    {
        lightMarkers[4 + i].position = debug.drawLightMarkers
            ? Vec3Make(uniforms.celestialPositions[i].x, uniforms.celestialPositions[i].y, uniforms.celestialPositions[i].z)
            : Vec3Make(0.0f, -10000.0f, 0.0f);
        lightMarkers[4 + i].color = Vec3Make(uniforms.celestialColors[i].x, uniforms.celestialColors[i].y, uniforms.celestialColors[i].z);
    }

    std::uint32_t sceneSpotCount = static_cast<std::uint32_t>(uniforms.sceneLightCounts.x);
    for (std::uint32_t i = 0; i < kMaxSceneSpotLights; ++i)
    {
        std::size_t markerIndex = 6 + i;
        if (i < sceneSpotCount)
        {
            Vec3 source = Vec3Make(uniforms.spotLightPositions[i].x, uniforms.spotLightPositions[i].y, uniforms.spotLightPositions[i].z);
            Vec3 color = Vec3Make(uniforms.spotLightColors[i].x, uniforms.spotLightColors[i].y, uniforms.spotLightColors[i].z);
            lightMarkers[markerIndex].position = debug.drawLightMarkers ? source : Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = color;
            if (debug.drawLightProxies)
            {
                AppendCube(lightSolids, m_lightSolidVertexCount, source, 0.08f, color);
            }
            if (debug.drawLightDirections)
            {
                Vec3 dir = Vec3Make(uniforms.spotLightDirections[i].x, uniforms.spotLightDirections[i].y, uniforms.spotLightDirections[i].z);
                float range = std::min(3.0f, uniforms.spotLightParams[i].z * 0.35f);
                AppendLine(lightLines, m_lightLineVertexCount, source, Vec3Add(source, Vec3Scale(dir, range)), color);
            }
            if (debug.drawLightVolumes)
            {
                AppendCone(
                    lightLines,
                    m_lightLineVertexCount,
                    source,
                    Vec3Make(uniforms.spotLightDirections[i].x, uniforms.spotLightDirections[i].y, uniforms.spotLightDirections[i].z),
                    uniforms.spotLightPositions[i].w,
                    uniforms.spotLightParams[i].y,
                    color
                );
            }
        }
        else
        {
            lightMarkers[markerIndex].position = Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = Vec3Make(0.0f, 0.0f, 0.0f);
        }
    }

    std::uint32_t shadowedSpotCount = static_cast<std::uint32_t>(uniforms.sceneLightCounts.y);
    for (std::uint32_t i = 0; i < kMaxShadowedSpotLights; ++i)
    {
        std::size_t markerIndex = 6 + kMaxSceneSpotLights + i;
        if (i < shadowedSpotCount)
        {
            Vec3 source = Vec3Make(
                uniforms.shadowedSpotLightPositions[i].x,
                uniforms.shadowedSpotLightPositions[i].y,
                uniforms.shadowedSpotLightPositions[i].z
            );
            Vec3 color = Vec3Make(
                uniforms.shadowedSpotLightColors[i].x,
                uniforms.shadowedSpotLightColors[i].y,
                uniforms.shadowedSpotLightColors[i].z
            );
            lightMarkers[markerIndex].position = debug.drawLightMarkers ? source : Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = color;
            if (debug.drawLightProxies)
            {
                AppendCube(lightSolids, m_lightSolidVertexCount, source, 0.08f, color);
            }
            if (debug.drawLightDirections)
            {
                Vec3 dir = Vec3Make(
                    uniforms.shadowedSpotLightDirections[i].x,
                    uniforms.shadowedSpotLightDirections[i].y,
                    uniforms.shadowedSpotLightDirections[i].z
                );
                float range = std::min(3.0f, uniforms.shadowedSpotLightParams[i].z * 0.35f);
                AppendLine(lightLines, m_lightLineVertexCount, source, Vec3Add(source, Vec3Scale(dir, range)), color);
            }
            if (debug.drawLightVolumes)
            {
                AppendCone(
                    lightLines,
                    m_lightLineVertexCount,
                    source,
                    Vec3Make(
                        uniforms.shadowedSpotLightDirections[i].x,
                        uniforms.shadowedSpotLightDirections[i].y,
                        uniforms.shadowedSpotLightDirections[i].z
                    ),
                    uniforms.shadowedSpotLightPositions[i].w,
                    uniforms.shadowedSpotLightParams[i].y,
                    color
                );
            }
        }
        else
        {
            lightMarkers[markerIndex].position = Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = Vec3Make(0.0f, 0.0f, 0.0f);
        }
    }

    if (debug.drawActivationVolumes)
    {
        if (debug.activationVolumeA.w > 0.0001f)
        {
            AppendCylinder(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(debug.activationVolumeA.x, debug.activationVolumeA.y, debug.activationVolumeA.z),
                debug.activationVolumeA.w,
                18.0f,
                Vec3Make(0.0f, 1.0f, 1.0f)
            );
        }
        if (debug.activationVolumeB.w > 0.0001f)
        {
            AppendCylinder(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(debug.activationVolumeB.x, debug.activationVolumeB.y, debug.activationVolumeB.z),
                debug.activationVolumeB.w,
                18.0f,
                Vec3Make(1.0f, 0.35f, 1.0f)
            );
        }
    }

    for (std::uint32_t i = 0; i < debug.selectionSphereCount && i < DebugRenderOptions::kMaxSelectionSpheres; ++i)
    {
        Vec4 sphere = debug.selectionSpheres[i];
        Vec4 color = debug.selectionSphereColors[i];
        if (sphere.w > 0.0001f)
        {
            AppendSphere(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(sphere.x, sphere.y, sphere.z),
                sphere.w,
                Vec3Make(color.x, color.y, color.z)
            );
        }
    }

    for (std::uint32_t i = 0; i < debug.customCubeCount && i < DebugRenderOptions::kMaxCustomCubes; ++i)
    {
        Vec4 cube = debug.customCubes[i];
        Vec4 color = debug.customCubeColors[i];
        if (cube.w > 0.0001f)
        {
            AppendCube(
                lightSolids,
                m_lightSolidVertexCount,
                Vec3Make(cube.x, cube.y, cube.z),
                cube.w,
                Vec3Make(color.x, color.y, color.z)
            );
        }
    }

    for (std::uint32_t i = 0; i < debug.customLineCount && i < DebugRenderOptions::kMaxCustomLines; ++i)
    {
        Vec4 start = debug.customLineStarts[i];
        Vec4 end = debug.customLineEnds[i];
        Vec4 color = debug.customLineColors[i];
        AppendLine(
            lightLines,
            m_lightLineVertexCount,
            Vec3Make(start.x, start.y, start.z),
            Vec3Make(end.x, end.y, end.z),
            Vec3Make(color.x, color.y, color.z));
    }
}
