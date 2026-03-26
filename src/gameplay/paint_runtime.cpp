#include "app.h"

#include <cmath>

#include <SDL3/SDL_keyboard.h>

#include "imgui.h"

namespace
{
Vec3 CameraRayDirection(const Camera& camera)
{
    return CameraForward(camera);
}

Mat4 InverseAffine(Mat4 m)
{
    float a00 = m.m[0], a01 = m.m[4], a02 = m.m[8];
    float a10 = m.m[1], a11 = m.m[5], a12 = m.m[9];
    float a20 = m.m[2], a21 = m.m[6], a22 = m.m[10];
    float det = a00 * (a11 * a22 - a12 * a21) -
                a01 * (a10 * a22 - a12 * a20) +
                a02 * (a10 * a21 - a11 * a20);
    if (std::fabs(det) <= 1e-8f)
    {
        return Mat4Identity();
    }

    float invDet = 1.0f / det;
    Mat4 out = Mat4Identity();
    out.m[0] = (a11 * a22 - a12 * a21) * invDet;
    out.m[4] = (a02 * a21 - a01 * a22) * invDet;
    out.m[8] = (a01 * a12 - a02 * a11) * invDet;
    out.m[1] = (a12 * a20 - a10 * a22) * invDet;
    out.m[5] = (a00 * a22 - a02 * a20) * invDet;
    out.m[9] = (a02 * a10 - a00 * a12) * invDet;
    out.m[2] = (a10 * a21 - a11 * a20) * invDet;
    out.m[6] = (a01 * a20 - a00 * a21) * invDet;
    out.m[10] = (a00 * a11 - a01 * a10) * invDet;

    Vec3 t = Vec3Make(m.m[12], m.m[13], m.m[14]);
    Vec3 invT = Vec3Make(
        -(out.m[0] * t.x + out.m[4] * t.y + out.m[8] * t.z),
        -(out.m[1] * t.x + out.m[5] * t.y + out.m[9] * t.z),
        -(out.m[2] * t.x + out.m[6] * t.y + out.m[10] * t.z)
    );
    out.m[12] = invT.x;
    out.m[13] = invT.y;
    out.m[14] = invT.z;
    return out;
}

Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 v = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    return Vec3Make(v.x, v.y, v.z);
}

Vec3 TransformDirection(Mat4 m, Vec3 d)
{
    Vec4 v = Mat4MulVec4(m, Vec4Make(d.x, d.y, d.z, 0.0f));
    return Vec3Normalize(Vec3Make(v.x, v.y, v.z));
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
    auto& paint = m_state.paint;
    if (splat.entityIndex >= core.scene.entities.size() || splat.entityIndex >= paint.entityLayers.size())
    {
        return;
    }

    const EntityData& entity = core.scene.entities[splat.entityIndex];
    Mat4 worldToLocal = InverseAffine(entity.transform);
    EntityPaintLayer& layer = paint.entityLayers[splat.entityIndex];
    PersistentPaintStamp& dst = layer.stamps[layer.nextStampIndex];
    dst.localPosition = TransformPoint(worldToLocal, splat.position);
    dst.localNormal = TransformDirection(worldToLocal, splat.normal);
    dst.radius = splat.radius;
    dst.color = splat.color;
    dst.opacity = 1.0f;
    dst.seed = static_cast<float>(layer.nextStampIndex);
    layer.nextStampIndex = (layer.nextStampIndex + 1u) % kMaxAccumulatedPaintPerEntity;
    layer.stampCount = std::min(layer.stampCount + 1u, kMaxAccumulatedPaintPerEntity);
}

std::uint32_t App::CountAccumulatedPaintStamps() const
{
    std::uint32_t count = 0;
    for (const EntityPaintLayer& layer : m_state.paint.entityLayers)
    {
        count += layer.stampCount;
    }
    return count;
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
