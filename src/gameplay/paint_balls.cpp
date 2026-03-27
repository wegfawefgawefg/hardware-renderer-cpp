#include "gameplay/paint_balls.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMaxLifetimeSeconds = 18.0f;
constexpr std::uint32_t kSubsteps = 4;
constexpr float kMinBounceSpeed = 0.6f;

Vec3 Reflect(Vec3 velocity, Vec3 normal, float restitution)
{
    float vn = Vec3Dot(velocity, normal);
    if (vn >= 0.0f)
    {
        return velocity;
    }
    return Vec3Sub(velocity, Vec3Scale(normal, (1.0f + restitution) * vn));
}

Vec3 HueColor(float t)
{
    float h = t - std::floor(t);
    float r = std::fabs(h * 6.0f - 3.0f) - 1.0f;
    float g = 2.0f - std::fabs(h * 6.0f - 2.0f);
    float b = 2.0f - std::fabs(h * 6.0f - 4.0f);
    return Vec3Make(
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f)
    );
}
}

void PaintBallSystem::Reset()
{
    m_balls = {};
}

void PaintBallSystem::Fire(Vec3 origin, Vec3 direction, const PaintBallSettings& settings)
{
    Vec3 dir = Vec3Normalize(direction);
    if (Vec3Length(dir) <= 1e-5f)
    {
        return;
    }

    std::uint32_t slot = 0;
    bool foundFree = false;
    std::uint64_t oldestShotId = std::numeric_limits<std::uint64_t>::max();
    for (std::uint32_t i = 0; i < m_balls.size(); ++i)
    {
        if (!m_balls[i].active)
        {
            slot = i;
            foundFree = true;
            break;
        }
        if (m_balls[i].shotId < oldestShotId)
        {
            oldestShotId = m_balls[i].shotId;
            slot = i;
        }
    }

    PaintBall& ball = m_balls[slot];
    ball.active = true;
    ball.position = origin;
    ball.velocity = Vec3Scale(dir, settings.shootSpeed);
    ball.color = settings.cycleColorOnShoot
        ? HueColor(static_cast<float>((m_nextShotId % 24u)) / 24.0f)
        : settings.baseColor;
    ball.radius = settings.radius;
    ball.lifetime = 0.0f;
    ball.remainingBounces = settings.bounceLimit;
    ball.shotId = m_nextShotId++;

    (void)foundFree;
}

void PaintBallSystem::Update(
    const TriangleMeshCollider& worldCollider,
    float dtSeconds,
    const PaintBallSettings& settings,
    std::vector<PaintSplatSpawn>& outSplats
)
{
    if (dtSeconds <= 0.0f)
    {
        return;
    }

    std::vector<TriangleMeshCollider::SphereContact> contacts;
    contacts.reserve(32);

    float subDt = dtSeconds / static_cast<float>(kSubsteps);
    for (PaintBall& ball : m_balls)
    {
        if (!ball.active)
        {
            continue;
        }

        ball.lifetime += dtSeconds;
        if (ball.lifetime > kMaxLifetimeSeconds || ball.position.y < -40.0f)
        {
            ball.active = false;
            continue;
        }

        for (std::uint32_t step = 0; step < kSubsteps && ball.active; ++step)
        {
            ball.velocity.y -= settings.gravity * subDt;
            ball.position = Vec3Add(ball.position, Vec3Scale(ball.velocity, subDt));

            contacts.clear();
            worldCollider.GatherSphereContacts(ball.position, ball.radius, contacts);
            if (contacts.empty())
            {
                continue;
            }

            Vec3 normalSum = {};
            bool collided = false;
            for (const auto& contact : contacts)
            {
                if (contact.penetration <= 0.0f)
                {
                    continue;
                }
                ball.position = Vec3Add(ball.position, Vec3Scale(contact.normal, contact.penetration + 0.001f));
                normalSum = Vec3Add(normalSum, contact.normal);
                collided = true;
            }

            if (!collided)
            {
                continue;
            }

            Vec3 bounceNormal = Vec3Normalize(normalSum);
            if (Vec3Length(bounceNormal) <= 1e-5f)
            {
                continue;
            }

            std::uint32_t impactEntity = UINT32_MAX;
            std::uint32_t impactPrimitive = UINT32_MAX;
            Vec2 impactUv = {};
            float impactUvWorldScale = 1.0f;
            float deepestPenetration = -1.0f;
            for (const auto& contact : contacts)
            {
                if (contact.penetration > deepestPenetration)
                {
                    deepestPenetration = contact.penetration;
                    impactEntity = contact.entityIndex;
                    impactPrimitive = contact.primitiveIndex;
                    impactUv = contact.uv;
                    impactUvWorldScale = contact.uvWorldScale;
                }
            }

            float incoming = Vec3Dot(ball.velocity, bounceNormal);
            if (incoming >= -1e-4f)
            {
                continue;
            }

            outSplats.push_back(PaintSplatSpawn{
                .position = Vec3Add(ball.position, Vec3Scale(bounceNormal, 0.015f)),
                .normal = bounceNormal,
                .color = ball.color,
                .radius = std::max(settings.blobRadius, 0.05f),
                .entityIndex = impactEntity,
                .primitiveIndex = impactPrimitive,
                .uv = impactUv,
                .uvWorldScale = impactUvWorldScale,
            });

            if (ball.remainingBounces == 0)
            {
                ball.active = false;
                break;
            }

            ball.velocity = Reflect(ball.velocity, bounceNormal, settings.restitution);
            --ball.remainingBounces;
            if (Vec3Length(ball.velocity) < kMinBounceSpeed)
            {
                ball.active = false;
                break;
            }
        }
    }
}

std::uint32_t PaintBallSystem::ActiveCount() const
{
    std::uint32_t count = 0;
    for (const PaintBall& ball : m_balls)
    {
        count += ball.active ? 1u : 0u;
    }
    return count;
}
