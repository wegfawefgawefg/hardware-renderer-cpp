#include "app.h"

#include <array>
#include <cmath>
#include <stdexcept>

#include "decals/flat_decal_system.h"

namespace
{
struct FractureDecalTemplateSpec
{
    const char* name;
    const char* albedoPath;
    const char* normalPath;
    bool flipNormalY;
};

constexpr std::array<FractureDecalTemplateSpec, 5> kFractureDecalTemplates = {{
    {"Metal Dent", "metal_dent_trans.png", "metal_dent_trans_normal.png", true},
    {"Circle Soft", "decal_circle_soft.png", "", true},
    {"Circle Hard", "decal_circle_hard.png", "", true},
    {"Ring Black", "decal_ring_black.png", "", true},
    {"Bullet Hole", "decal_bullet_hole_simple.png", "", true},
}};

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
    auto& fracture = m_state.fracture;
    ShutdownImGui();
    core.renderer.Shutdown();
    decals::ClearFlatDecals(core.flatDecals);
    decals::ResetFlatDecalTemplates(core.flatDecals);
    fracture.damageDecalTemplates = {};
    fracture.damageDecalTemplateNames = {};
    fracture.damageDecalTemplateCount = 0;
    fracture.selectedDamageDecalTemplate = 0;
    fracture.decalShotCounter = 0;

    if (lighting.sceneKind == SceneKind::FractureTest)
    {
        FractureSceneConfig config{};
        config.prismHalfExtents = fracture.prism.halfExtents;
        config.prismSegX = fracture.prism.segX;
        config.prismSegY = fracture.prism.segY;
        config.prismSegZ = fracture.prism.segZ;
        core.scene = BuildFractureTestScene(core.assetRegistry, config);
        for (const FractureDecalTemplateSpec& decalSpec : kFractureDecalTemplates)
        {
            if (fracture.damageDecalTemplateCount >= fracture.kMaxDamageDecalTemplates)
            {
                break;
            }

            decals::FlatDecalTemplateId templateId = decals::RegisterFlatDecalTemplate(
                core.flatDecals,
                decals::FlatDecalTemplate{
                    .name = decalSpec.name,
                    .albedoAssetPath = decalSpec.albedoPath,
                    .normalAssetPath = decalSpec.normalPath,
                    .flipNormalY = decalSpec.flipNormalY,
                });
            if (templateId == decals::kInvalidFlatDecalTemplateId)
            {
                continue;
            }

            std::uint32_t slot = fracture.damageDecalTemplateCount++;
            fracture.damageDecalTemplates[slot] = templateId;
            fracture.damageDecalTemplateNames[slot] = decalSpec.name;
        }
    }
    else
    {
        core.scene = LoadSampleScene(core.assetRegistry, lighting.sceneKind);
    }
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
    m_state.fracture.hitValid = false;
    m_state.fracture.fireHeld = false;
    m_state.fracture.fireCooldown = 0.0f;

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
    else if (lighting.sceneKind == SceneKind::FractureTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.sunAzimuthDegrees = -18.0f;
        lighting.sunIntensity = 1.4f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.10f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 24.0f;
        core.camera.position = Vec3Make(0.0f, 8.5f, 19.0f);
        core.camera.yawRadians = DegreesToRadians(180.0f);
        core.camera.pitchRadians = DegreesToRadians(-16.0f);
    }
    else if (lighting.sceneKind == SceneKind::ShadowTest)
    {
        lighting.cycleDayNight = false;
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
    core.renderer.Initialize(
        m_window,
        core.scene,
        &core.flatDecals,
        runtime.hasCharacter ? &core.characterSet.asset : nullptr,
        &m_state.text
    );
    InitializeImGui();
    SyncRendererSize();
    UpdateWindowTitle();
    UpdateOverlayText();
    runtime.reloadSceneRequested = false;
}

void App::RebuildCurrentSceneResources()
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;

    ShutdownImGui();
    core.renderer.Shutdown();
    core.sceneBounds = ComputeSceneBounds(core.scene);
    runtime.sceneTriangleCount = CountSceneTriangles(core.scene);
    core.worldCollider.BuildFromScene(core.scene);
    core.renderer.m_shadowMapSize = lighting.shadowMapSize;
    core.renderer.Initialize(
        m_window,
        core.scene,
        &core.flatDecals,
        runtime.hasCharacter ? &core.characterSet.asset : nullptr,
        &m_state.text
    );
    InitializeImGui();
    SyncRendererSize();
    UpdateWindowTitle();
}

void App::RefreshCurrentSceneGeometry()
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;

    core.sceneBounds = ComputeSceneBounds(core.scene);
    runtime.sceneTriangleCount = CountSceneTriangles(core.scene);
    core.worldCollider.BuildFromScene(core.scene);
    if (!core.renderer.UpdateSceneGeometry(core.scene))
    {
        RebuildCurrentSceneResources();
        return;
    }
}
