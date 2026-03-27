#include "app.h"

#include <algorithm>
#include <cmath>

#include <SDL3/SDL_keyboard.h>

#include "imgui.h"

namespace
{
Vec3 CameraRayDirection(const Camera& camera)
{
    return CameraForward(camera);
}

void BuildPaintBasis(Vec3 normal, Vec3& tangent, Vec3& bitangent)
{
    Vec3 up = std::fabs(normal.y) < 0.95f ? Vec3Make(0.0f, 1.0f, 0.0f) : Vec3Make(1.0f, 0.0f, 0.0f);
    tangent = Vec3Normalize(Vec3Cross(up, normal));
    bitangent = Vec3Normalize(Vec3Cross(normal, tangent));
}
}

void App::AppendPaintSplat(const PaintSplatSpawn& splat)
{
    auto& paint = m_state.paint;
    PaintSplat& dst = paint.splats[paint.nextSplatIndex];
    dst.position = splat.position;
    dst.radius = splat.radius;
    dst.normal = Vec3Normalize(splat.normal);
    dst.color = splat.color;
    paint.nextSplatIndex = (paint.nextSplatIndex + 1u) % kMaxPaintSplats;
    paint.splatCount = std::min(paint.splatCount + 1u, kMaxPaintSplats);
}

void App::AppendPersistentPaint(const PaintSplatSpawn& splat)
{
    auto& core = m_state.core;
    std::array<std::pair<std::uint32_t, std::uint32_t>, 16> touched = {};
    std::uint32_t touchedCount = 0;
    auto appendTouched = [&](const PaintSplatSpawn& hitSplat)
    {
        if (hitSplat.entityIndex == UINT32_MAX || hitSplat.primitiveIndex == UINT32_MAX)
        {
            return;
        }
        for (std::uint32_t i = 0; i < touchedCount; ++i)
        {
            if (touched[i].first == hitSplat.entityIndex && touched[i].second == hitSplat.primitiveIndex)
            {
                return;
            }
        }
        if (touchedCount < touched.size())
        {
            touched[touchedCount++] = {hitSplat.entityIndex, hitSplat.primitiveIndex};
            core.renderer.AppendPersistentPaint(hitSplat);
        }
    };

    appendTouched(splat);

    Vec3 normal = Vec3Normalize(splat.normal);
    Vec3 tangent = {};
    Vec3 bitangent = {};
    BuildPaintBasis(normal, tangent, bitangent);
    float ringRadius = std::max(splat.radius * 0.9f, 0.05f);
    float midRadius = ringRadius * 0.5f;
    float rayStartOffset = std::max(splat.radius * 0.35f, 0.08f);
    float rayDistance = std::max(splat.radius * 0.8f, 0.18f);
    std::array<Vec3, 13> probeOffsets = {
        Vec3Make(0.0f, 0.0f, 0.0f),
        Vec3Scale(tangent, midRadius),
        Vec3Scale(tangent, -midRadius),
        Vec3Scale(bitangent, midRadius),
        Vec3Scale(bitangent, -midRadius),
        Vec3Scale(tangent, ringRadius),
        Vec3Scale(tangent, -ringRadius),
        Vec3Scale(bitangent, ringRadius),
        Vec3Scale(bitangent, -ringRadius),
        Vec3Add(Vec3Scale(tangent, ringRadius * 0.7071f), Vec3Scale(bitangent, ringRadius * 0.7071f)),
        Vec3Add(Vec3Scale(tangent, ringRadius * 0.7071f), Vec3Scale(bitangent, -ringRadius * 0.7071f)),
        Vec3Add(Vec3Scale(tangent, -ringRadius * 0.7071f), Vec3Scale(bitangent, ringRadius * 0.7071f)),
        Vec3Add(Vec3Scale(tangent, -ringRadius * 0.7071f), Vec3Scale(bitangent, -ringRadius * 0.7071f)),
    };

    for (const Vec3& offset : probeOffsets)
    {
        Vec3 probeWorld = Vec3Add(splat.position, offset);
        Vec3 rayOrigin = Vec3Add(probeWorld, Vec3Scale(normal, rayStartOffset));
        TriangleMeshCollider::RayHit hit = core.worldCollider.Raycast(
            rayOrigin,
            Vec3Scale(normal, -1.0f),
            rayDistance
        );
        if (!hit.hit)
        {
            continue;
        }

        PaintSplatSpawn projected = splat;
        projected.position = hit.position;
        projected.normal = hit.normal;
        projected.entityIndex = hit.entityIndex;
        projected.primitiveIndex = hit.primitiveIndex;
        projected.uv = hit.uv;
        projected.uvWorldScale = hit.uvWorldScale;
        appendTouched(projected);
    }
}

std::uint32_t App::CountAccumulatedPaintStamps() const
{
    return m_state.core.renderer.GetAccumulatedPaintHitCount();
}

bool App::TryFirePaintBall()
{
    auto& core = m_state.core;
    auto& paint = m_state.paint;
    Vec3 forward = CameraRayDirection(core.camera);
    Vec3 spawn = Vec3Add(
        core.camera.position,
        Vec3Add(
            Vec3Scale(forward, 0.45f),
            Vec3Make(0.0f, -0.08f, 0.0f)
        )
    );
    core.paintBalls.Fire(spawn, forward, paint.ballSettings);
    float fireRate = std::max(paint.ballSettings.fireRate, 0.01f);
    paint.fireCooldown = 1.0f / fireRate;
    return true;
}

void App::UpdatePaintBalls(float dtSeconds)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& paint = m_state.paint;
    std::vector<PaintSplatSpawn> newSplats;
    newSplats.reserve(16);
    paint.fireCooldown = std::max(0.0f, paint.fireCooldown - dtSeconds);
    if (paint.fireHeld &&
        runtime.mouseCaptured &&
        (ImGui::GetCurrentContext() == nullptr || !ImGui::GetIO().WantCaptureMouse))
    {
        while (paint.fireCooldown <= 0.0f)
        {
            if (!TryFirePaintBall())
            {
                break;
            }
        }
    }
    core.paintBalls.Update(core.worldCollider, dtSeconds, paint.ballSettings, newSplats);
    for (const PaintSplatSpawn& splat : newSplats)
    {
        AppendPaintSplat(splat);
        AppendPersistentPaint(splat);
    }
}
