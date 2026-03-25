#include "gameplay/player_controller.h"

#include <SDL3/SDL_keyboard.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
Vec3 HorizontalForward(const Camera& camera)
{
    return Vec3Normalize(Vec3Make(std::sin(camera.yawRadians), 0.0f, std::cos(camera.yawRadians)));
}

Vec3 HorizontalRight(const Camera& camera)
{
    Vec3 forward = HorizontalForward(camera);
    return Vec3Normalize(Vec3Cross(forward, Vec3Make(0.0f, 1.0f, 0.0f)));
}

void ResolveSphere(
    PlayerController& player,
    const TriangleMeshCollider& collider,
    Vec3& position,
    Vec3& velocity
)
{
    std::vector<TriangleMeshCollider::SphereContact> contacts;
    contacts.reserve(32);

    for (int iteration = 0; iteration < 4; ++iteration)
    {
        collider.GatherSphereContacts(position, player.radius, contacts);
        if (contacts.empty())
        {
            return;
        }

        bool moved = false;
        for (const TriangleMeshCollider::SphereContact& contact : contacts)
        {
            position = Vec3Add(position, Vec3Scale(contact.normal, contact.penetration));
            float inwardSpeed = Vec3Dot(velocity, contact.normal);
            if (inwardSpeed < 0.0f)
            {
                velocity = Vec3Sub(velocity, Vec3Scale(contact.normal, inwardSpeed));
            }
            if (contact.normal.y > 0.45f)
            {
                player.onGround = true;
            }
            moved = true;
        }

        if (!moved)
        {
            return;
        }
    }
}
}

void PlayerSpawn(PlayerController& player, const TriangleMeshCollider& collider, const SceneBounds& sceneBounds)
{
    Vec3 spawn = sceneBounds.valid ? sceneBounds.center : Vec3Make(0.0f, 0.0f, 0.0f);
    float rayStart = sceneBounds.valid ? sceneBounds.max.y + 8.0f : 8.0f;
    TriangleMeshCollider::RayHit hit = collider.RaycastDown(
        spawn.x,
        spawn.z,
        rayStart,
        rayStart + 32.0f
    );

    if (hit.hit)
    {
        player.position = Vec3Make(spawn.x, hit.position.y + player.radius + 0.05f, spawn.z);
        player.onGround = true;
    }
    else
    {
        player.position = Vec3Make(
            spawn.x,
            sceneBounds.valid ? sceneBounds.center.y + std::max(sceneBounds.radius * 0.5f, 1.0f) : 1.5f,
            spawn.z + (sceneBounds.valid ? sceneBounds.radius * 0.5f : 0.0f)
        );
        player.onGround = false;
    }
}

void PlayerSyncCamera(const PlayerController& player, Camera& camera)
{
    Vec3 target = Vec3Make(
        player.position.x,
        player.position.y + player.cameraTargetHeight,
        player.position.z
    );
    Vec3 forward = CameraForward(camera);
    camera.position = Vec3Sub(target, Vec3Scale(forward, player.cameraDistance));
}

void PlayerUpdateFromInput(
    PlayerController& player,
    const TriangleMeshCollider& collider,
    const Camera& camera,
    float dtSeconds,
    bool inputEnabled
)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);

    Vec3 moveInput = Vec3Make(0.0f, 0.0f, 0.0f);
    Vec3 forward = HorizontalForward(camera);
    Vec3 right = HorizontalRight(camera);

    if (keys[SDL_SCANCODE_W])
    {
        moveInput = Vec3Add(moveInput, forward);
    }
    if (keys[SDL_SCANCODE_S])
    {
        moveInput = Vec3Sub(moveInput, forward);
    }
    if (keys[SDL_SCANCODE_D])
    {
        moveInput = Vec3Add(moveInput, right);
    }
    if (keys[SDL_SCANCODE_A])
    {
        moveInput = Vec3Sub(moveInput, right);
    }

    float moveScale = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]
        ? player.sprintMultiplier
        : 1.0f;

    Vec3 desiredHorizontalVelocity = Vec3Make(0.0f, player.velocity.y, 0.0f);
    if (inputEnabled && Vec3Length(moveInput) > 0.0001f)
    {
        Vec3 moveDir = Vec3Normalize(moveInput);
        desiredHorizontalVelocity.x = moveDir.x * player.moveSpeed * moveScale;
        desiredHorizontalVelocity.z = moveDir.z * player.moveSpeed * moveScale;
    }
    else
    {
        desiredHorizontalVelocity.x = 0.0f;
        desiredHorizontalVelocity.z = 0.0f;
    }

    player.velocity.x = desiredHorizontalVelocity.x;
    player.velocity.z = desiredHorizontalVelocity.z;

    if (inputEnabled && player.onGround && keys[SDL_SCANCODE_SPACE])
    {
        player.velocity.y = player.jumpSpeed;
        player.onGround = false;
    }

    if (!player.onGround)
    {
        player.velocity.y -= player.gravity * dtSeconds;
    }
    else if (player.velocity.y < 0.0f)
    {
        player.velocity.y = 0.0f;
    }

    Vec3 newPosition = Vec3Add(player.position, Vec3Scale(player.velocity, dtSeconds));
    bool wasGrounded = player.onGround;
    player.onGround = false;
    ResolveSphere(player, collider, newPosition, player.velocity);

    if (!player.onGround)
    {
        TriangleMeshCollider::RayHit groundHit = collider.RaycastDown(
            newPosition.x,
            newPosition.z,
            newPosition.y + player.radius,
            player.radius + player.groundSnapDistance
        );
        if (groundHit.hit && player.velocity.y <= 0.0f)
        {
            newPosition.y = groundHit.position.y + player.radius;
            player.velocity.y = 0.0f;
            player.onGround = true;
        }
    }

    if (!player.onGround && wasGrounded)
    {
        TriangleMeshCollider::RayHit groundHit = collider.RaycastDown(
            newPosition.x,
            newPosition.z,
            newPosition.y + player.radius,
            player.radius + player.groundSnapDistance * 2.0f
        );
        if (groundHit.hit && player.velocity.y <= 0.0f)
        {
            newPosition.y = groundHit.position.y + player.radius;
            player.velocity.y = 0.0f;
            player.onGround = true;
        }
    }

    player.position = newPosition;

    if (player.position.y < -200.0f)
    {
        player.position = Vec3Make(0.0f, 3.0f, 0.0f);
        player.velocity = Vec3Make(0.0f, 0.0f, 0.0f);
        player.onGround = false;
    }
}
