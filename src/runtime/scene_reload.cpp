#include "app.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include "decals/flat_decal_system.h"
#include "scene_city.h"

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

void ComputeTriangleBounds(
    const ModelData& model,
    std::uint32_t triIndex,
    Vec3& outMin,
    Vec3& outMax,
    Vec3& outCentroid)
{
    const std::uint32_t i0 = model.mesh.indices[triIndex * 3u + 0u];
    const std::uint32_t i1 = model.mesh.indices[triIndex * 3u + 1u];
    const std::uint32_t i2 = model.mesh.indices[triIndex * 3u + 2u];
    const Vec3 p0 = model.mesh.vertices[i0].position;
    const Vec3 p1 = model.mesh.vertices[i1].position;
    const Vec3 p2 = model.mesh.vertices[i2].position;
    outMin = Vec3Make(
        std::min(std::min(p0.x, p1.x), p2.x),
        std::min(std::min(p0.y, p1.y), p2.y),
        std::min(std::min(p0.z, p1.z), p2.z));
    outMax = Vec3Make(
        std::max(std::max(p0.x, p1.x), p2.x),
        std::max(std::max(p0.y, p1.y), p2.y),
        std::max(std::max(p0.z, p1.z), p2.z));
    outCentroid = Vec3Scale(Vec3Add(Vec3Add(p0, p1), p2), 1.0f / 3.0f);
}

std::int32_t BuildVirtualGeomClusterRecursive(
    const ModelData& model,
    VirtualGeomState& virtualGeom,
    std::uint32_t begin,
    std::uint32_t end,
    std::uint32_t depth)
{
    VirtualGeomCluster node{};
    node.firstTriangle = begin;
    node.triangleCount = end - begin;
    node.depth = depth;
    node.min = Vec3Make(1e30f, 1e30f, 1e30f);
    node.max = Vec3Make(-1e30f, -1e30f, -1e30f);

    std::vector<Vec3> centroids;
    centroids.reserve(end - begin);
    for (std::uint32_t i = begin; i < end; ++i)
    {
        Vec3 triMin{}, triMax{}, centroid{};
        ComputeTriangleBounds(model, virtualGeom.triangleOrder[i], triMin, triMax, centroid);
        node.min.x = std::min(node.min.x, triMin.x);
        node.min.y = std::min(node.min.y, triMin.y);
        node.min.z = std::min(node.min.z, triMin.z);
        node.max.x = std::max(node.max.x, triMax.x);
        node.max.y = std::max(node.max.y, triMax.y);
        node.max.z = std::max(node.max.z, triMax.z);
        centroids.push_back(centroid);
    }
    node.center = Vec3Scale(Vec3Add(node.min, node.max), 0.5f);
    node.radius = Vec3Length(Vec3Sub(node.max, node.center));

    const std::int32_t nodeIndex = static_cast<std::int32_t>(virtualGeom.clusters.size());
    virtualGeom.clusters.push_back(node);

    if (node.triangleCount <= virtualGeom.maxClusterTriangles || depth >= virtualGeom.maxDepth)
    {
        ++virtualGeom.leafClusterCount;
        return nodeIndex;
    }

    Vec3 size = Vec3Sub(node.max, node.min);
    int axis = 0;
    if (size.y > size.x && size.y >= size.z) axis = 1;
    else if (size.z > size.x && size.z >= size.y) axis = 2;

    const std::uint32_t mid = begin + (end - begin) / 2u;
    auto axisValue = [&](std::uint32_t orderedTriIndex) {
        Vec3 triMin{}, triMax{}, centroid{};
        ComputeTriangleBounds(model, orderedTriIndex, triMin, triMax, centroid);
        return axis == 0 ? centroid.x : (axis == 1 ? centroid.y : centroid.z);
    };
    std::nth_element(
        virtualGeom.triangleOrder.begin() + begin,
        virtualGeom.triangleOrder.begin() + mid,
        virtualGeom.triangleOrder.begin() + end,
        [&](std::uint32_t a, std::uint32_t b) {
            return axisValue(a) < axisValue(b);
        });

    virtualGeom.clusters[static_cast<std::size_t>(nodeIndex)].leftChild =
        BuildVirtualGeomClusterRecursive(model, virtualGeom, begin, mid, depth + 1u);
    virtualGeom.clusters[static_cast<std::size_t>(nodeIndex)].rightChild =
        BuildVirtualGeomClusterRecursive(model, virtualGeom, mid, end, depth + 1u);
    return nodeIndex;
}

void RebuildVirtualGeomHierarchy(State& state)
{
    VirtualGeomState& virtualGeom = state.virtualGeom;
    virtualGeom.triangleOrder.clear();
    virtualGeom.clusters.clear();
    virtualGeom.activeDraws.clear();
    virtualGeom.activeClusterCount = 0;
    virtualGeom.leafClusterCount = 0;

    if (state.lighting.sceneKind != SceneKind::VirtualGeomTest ||
        state.core.scene.models.empty() ||
        state.core.scene.models[0].mesh.indices.size() < 3u)
    {
        return;
    }

    const ModelData& model = state.core.scene.models[0];
    const std::uint32_t triangleCount = static_cast<std::uint32_t>(model.mesh.indices.size() / 3u);
    virtualGeom.triangleOrder.resize(triangleCount);
    std::iota(virtualGeom.triangleOrder.begin(), virtualGeom.triangleOrder.end(), 0u);
    BuildVirtualGeomClusterRecursive(model, virtualGeom, 0u, triangleCount, 0u);
}
}

void App::ReloadScene()
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    auto& fracture = m_state.fracture;
    auto& city = m_state.city;
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
        config.prismQuadSize = fracture.prism.quadSize;
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
    else if (lighting.sceneKind == SceneKind::City || lighting.sceneKind == SceneKind::ProcCity)
    {
        CitySceneConfig config{};
        config.buildingQuadSize = city.buildingQuadSize;
        config.buildingMode = lighting.sceneKind == SceneKind::ProcCity
            ? CitySceneConfig::BuildingMode::Procedural
            : CitySceneConfig::BuildingMode::Kenney;
        config.roadLightStride = lighting.sceneKind == SceneKind::ProcCity ? 2 : 1;
        core.scene = BuildSampleCity(core.assetRegistry, config);
    }
    else
    {
        if (lighting.sceneKind == SceneKind::VirtualGeomTest)
        {
            VirtualGeomSceneConfig config{};
            config.meshKind = m_state.virtualGeom.meshKind;
            config.sphereLongitudeSegments = m_state.virtualGeom.sphereLongitudeSegments;
            config.sphereLatitudeSegments = m_state.virtualGeom.sphereLatitudeSegments;
            config.gridCountX = m_state.virtualGeom.gridCountX;
            config.gridCountZ = m_state.virtualGeom.gridCountZ;
            config.gridSpacing = m_state.virtualGeom.gridSpacing;
            core.scene = BuildVirtualGeomTestScene(core.assetRegistry, config);
        }
        else
        {
            core.scene = LoadSampleScene(core.assetRegistry, lighting.sceneKind, lighting.manyLightsHeroModel);
        }
    }
    RebuildVirtualGeomHierarchy(m_state);
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
    else if (lighting.sceneKind == SceneKind::LightTileTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.sunIntensity = 0.0f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.02f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 16.0f;
        core.camera.position = Vec3Make(0.0f, 4.0f, -2.5f);
        core.camera.yawRadians = DegreesToRadians(0.0f);
        core.camera.pitchRadians = DegreesToRadians(0.0f);
    }
    else if (lighting.sceneKind == SceneKind::ManyLights)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.pointLightIntensity = 0.0f;
        if (lighting.manyLightsHeroModel == ManyLightsHeroModel::Sponza)
        {
            lighting.sunIntensity = 1.15f;
            lighting.moonIntensity = 0.0f;
            lighting.ambientIntensity = 0.05f;
            lighting.shadowCascadeSplit = 42.0f;
            core.camera.position = Vec3Make(0.0f, 6.0f, -18.0f);
            core.camera.yawRadians = DegreesToRadians(0.0f);
            core.camera.pitchRadians = DegreesToRadians(-8.0f);
        }
        else
        {
            lighting.sunIntensity = 0.35f;
            lighting.moonIntensity = 0.0f;
            lighting.ambientIntensity = 0.08f;
            lighting.shadowCascadeSplit = 16.0f;
            core.camera.position = Vec3Make(0.0f, 3.0f, -7.0f);
            core.camera.yawRadians = DegreesToRadians(0.0f);
            core.camera.pitchRadians = DegreesToRadians(-6.0f);
        }
        lighting.procCityDynamicLightMotionRadius = 0.0f;
    }
    else if (lighting.sceneKind == SceneKind::VirtualGeomTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.sunIntensity = 0.0f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.85f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 16.0f;
        float gridWidth = static_cast<float>(std::max(1u, m_state.virtualGeom.gridCountX) - 1u) * m_state.virtualGeom.gridSpacing;
        float gridDepth = static_cast<float>(std::max(1u, m_state.virtualGeom.gridCountZ) - 1u) * m_state.virtualGeom.gridSpacing;
        Vec3 focus = Vec3Make(0.0f, 3.0f, 0.0f);
        float sceneRadius = std::max(std::max(gridWidth, gridDepth) * 0.5f + 6.0f, 8.0f);
        core.camera.position = Vec3Make(
            focus.x,
            focus.y + std::max(2.5f, sceneRadius * 0.22f),
            focus.z - sceneRadius * 1.35f);
        core.camera.yawRadians = DegreesToRadians(0.0f);
        float pitch = std::atan2(
            focus.y - core.camera.position.y,
            std::max(0.001f, focus.z - core.camera.position.z));
        core.camera.pitchRadians = pitch;
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
