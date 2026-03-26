#include "app.h"

#include <cmath>
#include <cstdio>

#include "imgui.h"

namespace
{
Vec3 CameraRayDirection(const Camera& camera, float aspect, float ndcX, float ndcY)
{
    constexpr float kFovYRadians = 1.0471975512f;
    float tanHalfFov = std::tan(kFovYRadians * 0.5f);
    Vec3 forward = CameraForward(camera);
    Vec3 right = CameraRight(camera);
    Vec3 up = Vec3Normalize(Vec3Cross(right, forward));
    Vec3 ray = Vec3Add(
        forward,
        Vec3Add(
            Vec3Scale(right, ndcX * aspect * tanHalfFov),
            Vec3Scale(up, ndcY * tanHalfFov)
        )
    );
    return Vec3Normalize(ray);
}

Vec3 RotateYOffset(Vec3 v, float yawDegrees)
{
    float radians = DegreesToRadians(yawDegrees);
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Make(
        c * v.x + s * v.z,
        v.y,
        -s * v.x + c * v.z
    );
}
}

void App::TryPlaceShadowTestSpotlight(int mouseX, int mouseY)
{
    if (m_sceneKind != SceneKind::ShadowTest || m_scene.spotLights.empty())
    {
        return;
    }
    if (m_windowWidth == 0 || m_windowHeight == 0)
    {
        return;
    }

    float pixelX = static_cast<float>(mouseX) + 0.5f;
    float pixelY = static_cast<float>(mouseY) + 0.5f;
    float ndcX = pixelX / static_cast<float>(m_windowWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - pixelY / static_cast<float>(m_windowHeight) * 2.0f;
    float aspect = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);
    Vec3 rayDir = CameraRayDirection(m_camera, aspect, ndcX, ndcY);

    TriangleMeshCollider::RayHit hit = m_worldCollider.Raycast(m_camera.position, rayDir, 500.0f);
    if (!hit.hit)
    {
        return;
    }

    Vec3 source = Vec3Add(
        m_scene.spotLights[0].position,
        RotateYOffset(m_spotLightSourceOffset, m_scene.spotLights[0].yawDegrees)
    );
    Vec3 offset = Vec3Sub(hit.position, source);
    if (Vec3Length(offset) <= 1e-4f)
    {
        return;
    }

    m_shadowTestSpotTargetValid = true;
    m_shadowTestSpotTargetWorld = hit.position;
    m_shadowTestSpotTargetOffset = offset;

    std::printf(
        "shadow-test spotlight target offset: %.3f %.3f %.3f\n",
        offset.x,
        offset.y,
        offset.z
    );
    std::fflush(stdout);
}
