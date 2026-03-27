#pragma once

#include "camera.h"
#include "collision/triangle_collider.h"
#include "scene.h"

struct PlayerController
{
    Vec3 position = {};
    Vec3 velocity = {};
    float radius = 0.35f;
    float eyeHeight = 0.9f;
    float moveSpeed = 5.5f;
    float sprintMultiplier = 1.75f;
    float jumpSpeed = 5.0f;
    float gravity = 17.0f;
    float groundSnapDistance = 0.18f;
    float cameraDistance = 3.4f;
    float cameraTargetHeight = 1.15f;
    bool onGround = false;
};

void PlayerSpawn(PlayerController& player, const TriangleMeshCollider& collider, const SceneBounds& sceneBounds);
void PlayerSyncCamera(const PlayerController& player, const TriangleMeshCollider& collider, Camera& camera);
void PlayerUpdateFromInput(
    PlayerController& player,
    const TriangleMeshCollider& collider,
    const Camera& camera,
    float dtSeconds,
    bool inputEnabled
);
void PlayerFlyUpdate(Camera& camera, float dtSeconds, bool inputEnabled);
