#include "app.h"

#include <cmath>
#include <stdexcept>

namespace
{
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

ModelData BuildStaticCharacterModel(const SkinnedCharacterAsset& asset)
{
    ModelData model{};
    model.mesh = asset.mesh;
    model.textures.push_back(asset.texture);
    model.materials.push_back(MaterialData{.name = "skin", .textureIndex = 0});
    model.primitives.push_back(PrimitiveData{
        .firstIndex = 0,
        .indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()),
        .materialIndex = 0,
    });
    return model;
}

float ComputeStaticModelFootprint(const ModelData& model, Mat4 transform)
{
    if (model.mesh.vertices.empty())
    {
        return 1.0f;
    }

    Vec3 mn = Vec3Make(1e30f, 1e30f, 1e30f);
    Vec3 mx = Vec3Make(-1e30f, -1e30f, -1e30f);
    for (const Vertex& vertex : model.mesh.vertices)
    {
        Vec4 p = Mat4MulVec4(transform, Vec4Make(vertex.position.x, vertex.position.y, vertex.position.z, 1.0f));
        mn.x = std::min(mn.x, p.x);
        mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x);
        mx.z = std::max(mx.z, p.z);
    }
    return std::max(std::max(mx.x - mn.x, mx.z - mn.z), 0.001f);
}

float ComputeStaticModelMinY(const ModelData& model, Mat4 transform)
{
    if (model.mesh.vertices.empty())
    {
        return 0.0f;
    }

    float minY = 1e30f;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        Vec4 p = Mat4MulVec4(transform, Vec4Make(vertex.position.x, vertex.position.y, vertex.position.z, 1.0f));
        minY = std::min(minY, p.y);
    }
    return minY;
}
}

void App::ReloadScene()
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    ShutdownImGui();
    core.renderer.Shutdown();

    core.scene = LoadSampleScene(core.assetRegistry, lighting.sceneKind);
    if (lighting.sceneKind == SceneKind::PlayerMaskTest && runtime.hasCharacter && core.scene.models.size() >= 2 && core.scene.entities.size() >= 2)
    {
        ModelData characterModel = BuildStaticCharacterModel(core.characterSet.asset);
        Mat4 baseTransform = core.characterSet.asset.modelOffset;
        float footprint = ComputeStaticModelFootprint(characterModel, baseTransform);
        float scale = 7.5f / std::max(footprint, 0.001f);
        Mat4 scaledTransform = Mat4Mul(baseTransform, Mat4Scale(scale));
        float minY = ComputeStaticModelMinY(characterModel, scaledTransform);

        core.scene.models[1] = std::move(characterModel);
        core.scene.entities[1].modelIndex = 1;
        core.scene.entities[1].transform = Mat4Mul(
            Mat4Translate(Vec3Make(0.0f, -minY, 0.0f)),
            scaledTransform
        );
        core.scene.entities[1].collidable = true;
    }
    core.sceneBounds = ComputeSceneBounds(core.scene);
    runtime.sceneTriangleCount = CountSceneTriangles(core.scene);
    m_state.paint.splats = {};
    m_state.paint.splatCount = 0;
    m_state.paint.nextSplatIndex = 0;
    core.traffic.Initialize(core.scene);
    core.worldCollider.BuildFromScene(core.scene);

    if (core.sceneBounds.valid)
    {
        core.camera.yawRadians = DegreesToRadians(180.0f);
        core.camera.pitchRadians = DegreesToRadians(-14.0f);
    }

    PlayerSpawn(core.player, core.worldCollider, core.sceneBounds);
    runtime.characterModelYaw = core.camera.yawRadians;
    PlayerSyncCamera(core.player, core.worldCollider, core.camera);

    if (lighting.sceneKind == SceneKind::PlayerMaskTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.56f;
        lighting.sunAzimuthDegrees = -40.0f;
        lighting.sunIntensity = 1.4f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.12f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 18.0f;
        core.player.position = Vec3Make(0.0f, 1.0f, 10.5f);
        core.player.velocity = Vec3Make(0.0f, 0.0f, 0.0f);
        core.player.onGround = true;
        core.camera.position = Vec3Make(0.0f, 3.0f, 15.0f);
        core.camera.yawRadians = DegreesToRadians(180.0f);
        core.camera.pitchRadians = DegreesToRadians(-8.0f);
        PlayerSyncCamera(core.player, core.worldCollider, core.camera);
    }
    else if (lighting.sceneKind == SceneKind::ShadowTest)
    {
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.50f;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.sunIntensity = 1.35f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.18f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 12.0f;
        if (!core.scene.spotLights.empty())
        {
            Vec3 source = Vec3Add(
                core.scene.spotLights[0].position,
                RotateYOffset(lighting.spotLightSourceOffset, core.scene.spotLights[0].yawDegrees)
            );
            Vec3 defaultTarget = Vec3Add(source, Vec3Scale(core.scene.spotLights[0].direction, 7.0f));
            TriangleMeshCollider::RayHit hit = core.worldCollider.Raycast(source, core.scene.spotLights[0].direction, 24.0f);
            lighting.shadowTestSpotTargetValid = hit.hit;
            lighting.shadowTestSpotTargetWorld = hit.hit ? hit.position : defaultTarget;
            lighting.shadowTestSpotTargetOffset = Vec3Sub(lighting.shadowTestSpotTargetWorld, source);
        }
    }
    else if (lighting.sceneKind == SceneKind::SpotShadowTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.50f;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.sunIntensity = 0.0f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.05f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 16.0f;
    }
    else if (lighting.sceneKind == SceneKind::VehicleLightTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.72f;
        lighting.sunAzimuthDegrees = -28.0f;
        lighting.sunIntensity = 0.12f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.06f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 20.0f;
        core.player.position = Vec3Make(-56.0f, 1.0f, -6.0f);
        core.player.velocity = Vec3Make(0.0f, 0.0f, 0.0f);
        core.player.onGround = true;
        core.camera.position = Vec3Make(-56.0f, 3.2f, -12.0f);
        core.camera.yawRadians = DegreesToRadians(180.0f);
        core.camera.pitchRadians = DegreesToRadians(-10.0f);
        PlayerSyncCamera(core.player, core.worldCollider, core.camera);
    }
    else
    {
        lighting.shadowTestSpotTargetValid = false;
    }

    core.renderer.m_shadowMapSize = lighting.shadowMapSize;
    core.renderer.Initialize(m_window, core.scene, runtime.hasCharacter ? &core.characterSet.asset : nullptr);
    InitializeImGui();
    SyncRendererSize();
    UpdateWindowTitle();
    UpdateOverlayText();
    runtime.reloadSceneRequested = false;
}
