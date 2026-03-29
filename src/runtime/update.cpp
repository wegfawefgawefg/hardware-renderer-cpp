#include "app.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>

#include "imgui.h"

namespace
{
constexpr float kTitleRefreshPeriod = 0.20f;
constexpr float kOverlayRefreshPeriod = 0.12f;
constexpr float kGameplayFixedStepSeconds = 1.0f / 120.0f;
constexpr float kGameplayMaxCatchupSeconds = kGameplayFixedStepSeconds * 8.0f;

Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 out = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    if (std::fabs(out.w) > 1e-6f && out.w != 1.0f)
    {
        float invW = 1.0f / out.w;
        return Vec3Make(out.x * invW, out.y * invW, out.z * invW);
    }
    return Vec3Make(out.x, out.y, out.z);
}
}

void App::HandleEvent(const SDL_Event& event)
{
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    auto& fracture = m_state.fracture;
    auto& paint = m_state.paint;
    ProcessImGuiEvent(event);
    bool imguiCapturingMouse = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        runtime.running = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE)
        {
            if (runtime.mouseCaptured)
            {
                ResetMouseCapture(false);
            }
            else
            {
                runtime.running = false;
            }
        }
        if (event.key.key == SDLK_TAB)
        {
            runtime.showImGui = !runtime.showImGui;
        }
        if (event.key.key == SDLK_F1)
        {
            ResetMouseCapture(!runtime.mouseCaptured);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_LEFT && !imguiCapturingMouse)
        {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            bool placementModifier = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            if (!runtime.mouseCaptured && placementModifier && lighting.sceneKind == SceneKind::ShadowTest)
            {
                TryPlaceShadowTestSpotlight(event.button.x, event.button.y);
            }
            else if (!runtime.mouseCaptured && placementModifier && lighting.sceneKind == SceneKind::VehicleLightTest)
            {
                TryPlaceVehicleLight(event.button.x, event.button.y);
            }
            else if (runtime.mouseCaptured)
            {
                if (lighting.sceneKind == SceneKind::FractureTest ||
                    (lighting.sceneKind == SceneKind::City &&
                     paint.interactionMode == PaintInteractionMode::Damage))
                {
                    fracture.fireHeld = fracture.settings.mesh.mode == damage::Mode::DamageDecal;
                    TryFireFractureShot();
                }
                else if (paint.interactionMode == PaintInteractionMode::PaintBalls)
                {
                    paint.fireHeld = true;
                    TryFirePaintBall();
                }
                else
                {
                    paint.surfaceBrushHeld = true;
                    TryApplySurfaceMaskBrush();
                }
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            fracture.fireHeld = false;
            paint.fireHeld = false;
            paint.surfaceBrushHeld = false;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (runtime.mouseCaptured)
        {
            CameraAddMouseLook(
                m_state.core.camera,
                static_cast<float>(event.motion.xrel),
                static_cast<float>(event.motion.yrel)
            );
        }
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESIZED:
        runtime.windowResized = true;
        break;
    default:
        break;
    }
}

void App::Update(float dtSeconds)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    auto& vehicleLights = m_state.vehicleLights;
    auto& fracture = m_state.fracture;
    auto& paint = m_state.paint;
    auto& text = m_state.text;

    using Clock = std::chrono::steady_clock;
    auto frameStart = Clock::now();
    runtime.elapsedSeconds += dtSeconds;

    if (dtSeconds > 0.0f)
    {
        float instantFps = 1.0f / dtSeconds;
        runtime.smoothedFps = runtime.smoothedFps <= 0.0f
            ? instantFps
            : runtime.smoothedFps * 0.95f + instantFps * 0.05f;
    }

    auto inputStart = Clock::now();
    bool imguiCapturingKeyboard = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
    runtime.gameplayAccumulatorSeconds = std::min(
        runtime.gameplayAccumulatorSeconds + dtSeconds,
        kGameplayMaxCatchupSeconds);
    while (runtime.gameplayAccumulatorSeconds >= kGameplayFixedStepSeconds)
    {
        if (lighting.sceneKind == SceneKind::PlayerMaskTest || lighting.sceneKind == SceneKind::FractureTest)
        {
            PlayerFlyUpdate(core.camera, kGameplayFixedStepSeconds, runtime.mouseCaptured && !imguiCapturingKeyboard);
        }
        else
        {
            PlayerUpdateFromInput(
                core.player,
                core.worldCollider,
                core.camera,
                kGameplayFixedStepSeconds,
                runtime.mouseCaptured && !imguiCapturingKeyboard);
            PlayerSyncCamera(core.player, core.worldCollider, core.camera);
        }
        UpdateFractureSandbox(kGameplayFixedStepSeconds);
        core.traffic.Update(core.scene, kGameplayFixedStepSeconds, 64);
        UpdatePaintBalls(kGameplayFixedStepSeconds);
        UpdateSurfaceMaskBrush(kGameplayFixedStepSeconds);
        runtime.gameplayAccumulatorSeconds -= kGameplayFixedStepSeconds;
    }
    core.renderer.UpdateSceneTransforms(core.scene);
    auto inputEnd = Clock::now();
    runtime.cpuProfiling.inputMs = std::chrono::duration<float, std::milli>(inputEnd - inputStart).count();

    if (runtime.windowResized)
    {
        SyncRendererSize();
        runtime.windowResized = false;
    }

    runtime.titleRefreshSeconds -= dtSeconds;
    if (runtime.titleRefreshSeconds <= 0.0f)
    {
        UpdateWindowTitle();
        runtime.titleRefreshSeconds = kTitleRefreshPeriod;
    }

    auto imguiStart = Clock::now();
    BuildImGui();
    auto imguiEnd = Clock::now();
    runtime.cpuProfiling.imguiMs = std::chrono::duration<float, std::milli>(imguiEnd - imguiStart).count();
    if (runtime.reloadSceneRequested)
    {
        ReloadScene();
        return;
    }

    SceneUniforms uniforms{};
    uniforms.view = CameraViewMatrix(core.camera);
    uniforms.proj = Mat4Perspective(
        DegreesToRadians(60.0f),
        static_cast<float>(runtime.windowWidth) / static_cast<float>(runtime.windowHeight),
        0.1f,
        200.0f
    );
    uniforms.proj.m[5] *= -1.0f;
    uniforms.cameraPosition = Vec4Make(core.camera.position.x, core.camera.position.y, core.camera.position.z, runtime.elapsedSeconds);

    auto lightingStart = Clock::now();
    ApplyLighting(uniforms, dtSeconds);
    auto lightingEnd = Clock::now();
    runtime.cpuProfiling.lightingMs = std::chrono::duration<float, std::milli>(lightingEnd - lightingStart).count();

    for (Mat4& skinJoint : uniforms.skinJoints)
    {
        skinJoint = Mat4Identity();
    }
    for (Vec4& splatPos : uniforms.paintSplatPositions)
    {
        splatPos = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (Vec4& splatNormal : uniforms.paintSplatNormals)
    {
        splatNormal = Vec4Make(0.0f, 1.0f, 0.0f, 0.0f);
    }
    for (Vec4& splatColor : uniforms.paintSplatColors)
    {
        splatColor = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
    }
    uniforms.paintSplatCounts = Vec4Make(
        static_cast<float>(paint.splatCount),
        static_cast<float>(m_state.lighting.materialDebugMode),
        m_state.lighting.uvDebugScale,
        static_cast<float>(m_state.lighting.uvDebugMode)
    );
    uniforms.surfaceMaskParamsA = Vec4Make(
        paint.vanishSplitStrength,
        paint.vanishJitterStrength,
        paint.vanishStaticStrength,
        paint.vanishEdgeGlowStrength
    );
    uniforms.surfaceMaskParamsB = Vec4Make(
        lighting.normalMapStrength,
        lighting.sceneKind == SceneKind::City ? m_state.city.buildingQuadSize : fracture.prism.quadSize,
        0.0f,
        0.0f
    );
    for (std::uint32_t i = 0; i < paint.splatCount && i < kMaxPaintSplats; ++i)
    {
        const PaintSplat& splat = paint.splats[i];
        uniforms.paintSplatPositions[i] = Vec4Make(splat.position.x, splat.position.y, splat.position.z, splat.radius);
        uniforms.paintSplatNormals[i] = Vec4Make(splat.normal.x, splat.normal.y, splat.normal.z, 0.0f);
        uniforms.paintSplatColors[i] = Vec4Make(splat.color.x, splat.color.y, splat.color.z, 1.0f);
    }

    if (runtime.hasCharacter &&
        m_state.lighting.sceneKind != SceneKind::PlayerMaskTest &&
        m_state.lighting.sceneKind != SceneKind::FractureTest)
    {
        float moveMag = Vec3Length(Vec3Make(core.player.velocity.x, 0.0f, core.player.velocity.z));
        int wantedAnim = (!core.player.onGround && std::fabs(core.player.velocity.y) > 0.5f) ? 2 : (moveMag > 0.1f ? 1 : 0);
        if (moveMag > 0.1f)
        {
            runtime.characterModelYaw = std::atan2(core.player.velocity.x, core.player.velocity.z);
        }

        if (wantedAnim != runtime.activeCharacterAnim)
        {
            runtime.activeCharacterAnim = wantedAnim;
            runtime.characterAnimTime = 0.0f;
        }
        else
        {
            float animDt = dtSeconds;
            if (wantedAnim == 1)
            {
                float base = std::max(1e-5f, core.player.moveSpeed);
                animDt *= std::clamp(moveMag / base, 0.25f, 3.0f);
            }
            runtime.characterAnimTime += animDt;
        }

        const AnimationClip* clip = &core.characterSet.idle;
        if (runtime.activeCharacterAnim == 1)
        {
            clip = &core.characterSet.run;
        }
        else if (runtime.activeCharacterAnim == 2)
        {
            clip = &core.characterSet.jump;
        }

        EvaluateCharacterClip(core.characterSet.asset, *clip, runtime.characterAnimTime, core.characterRenderState);
        Vec3 renderPos = Vec3Sub(core.player.position, Vec3Make(0.0f, core.player.radius, 0.0f));
        core.characterRenderState.model = Mat4Mul(
            Mat4Translate(renderPos),
            Mat4Mul(Mat4RotateY(runtime.characterModelYaw), Mat4Mul(core.characterSet.asset.modelOffset, Mat4Scale(0.8f)))
        );
        for (std::uint32_t i = 0; i < core.characterRenderState.jointCount && i < kMaxSkinJoints; ++i)
        {
            uniforms.skinJoints[i] = core.characterRenderState.joints[i];
        }
    }
    else
    {
        core.characterRenderState = {};
    }

    runtime.overlayRefreshSeconds -= dtSeconds;
    if (runtime.overlayRefreshSeconds <= 0.0f)
    {
        runtime.overlayRefreshSeconds = kOverlayRefreshPeriod;
    }
    UpdateOverlayText(&uniforms);

    DebugRenderOptions debugOptions{};
    debugOptions.drawLightProxies = lighting.drawLightProxies;
    debugOptions.drawLightMarkers = lighting.debugDrawSceneLightGizmos;
    debugOptions.drawLightDirections = lighting.debugDrawLightDirections;
    debugOptions.drawLightVolumes = lighting.debugDrawLightVolumes;
    debugOptions.drawActivationVolumes = lighting.debugDrawActivationVolumes;
    debugOptions.mainDrawDistance = lighting.mainDrawDistance;
    debugOptions.shadowDrawDistance = lighting.shadowDrawDistance;
    Vec3 forward = CameraForward(core.camera);
    Vec3 activationBase = Vec3Make(core.player.position.x, core.player.position.y - core.player.radius, core.player.position.z);
    Vec3 centerA = Vec3Add(activationBase, Vec3Scale(forward, lighting.spotLightActivationForwardOffset));
    Vec3 centerB = Vec3Add(activationBase, Vec3Scale(forward, lighting.shadowedSpotLightActivationForwardOffset));
    debugOptions.activationVolumeA = Vec4Make(centerA.x, centerA.y, centerA.z, lighting.spotLightActivationDistance);
    debugOptions.activationVolumeB = Vec4Make(centerB.x, centerB.y, centerB.z, lighting.shadowedSpotLightActivationDistance);
    if (lighting.sceneKind == SceneKind::VehicleLightTest && vehicleLights.debugDrawVehicleVolumes)
    {
        int activeVehicle = FindActiveVehicleLightIndex();
        std::uint32_t sphereCount = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(core.scene.vehicleLightTestItems.size()),
            DebugRenderOptions::kMaxSelectionSpheres
        );
        debugOptions.selectionSphereCount = sphereCount;
        for (std::uint32_t i = 0; i < sphereCount; ++i)
        {
            const auto& item = core.scene.vehicleLightTestItems[i];
            debugOptions.selectionSpheres[i] = Vec4Make(item.origin.x, item.origin.y + 1.0f, item.origin.z, item.selectionRadius);
            Vec3 color = static_cast<int>(i) == activeVehicle ? Vec3Make(0.47f, 1.0f, 0.47f) : Vec3Make(1.0f, 1.0f, 1.0f);
            debugOptions.selectionSphereColors[i] = Vec4Make(color.x, color.y, color.z, 1.0f);
        }
    }

    debugOptions.customCubeCount = core.paintBalls.ActiveCount();
    std::uint32_t cubeIndex = 0;
    for (const PaintBall& ball : core.paintBalls.Balls())
    {
        if (!ball.active || cubeIndex >= DebugRenderOptions::kMaxCustomCubes)
        {
            continue;
        }
        debugOptions.customCubes[cubeIndex] = Vec4Make(ball.position.x, ball.position.y, ball.position.z, ball.radius);
        debugOptions.customCubeColors[cubeIndex] = Vec4Make(ball.color.x, ball.color.y, ball.color.z, 1.0f);
        ++cubeIndex;
    }

    if (paint.interactionMode == PaintInteractionMode::SurfaceBrush && paint.surfaceBrushHitValid)
    {
        if (debugOptions.selectionSphereCount < DebugRenderOptions::kMaxSelectionSpheres)
        {
            std::uint32_t sphereIndex = debugOptions.selectionSphereCount++;
            debugOptions.selectionSpheres[sphereIndex] = Vec4Make(
                paint.surfaceBrushHitPosition.x,
                paint.surfaceBrushHitPosition.y,
                paint.surfaceBrushHitPosition.z,
                paint.surfaceMaskBrush.radius
            );
            Vec3 color = Vec3Make(0.25f, 1.0f, 0.95f);
            if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Grime) color = Vec3Make(0.60f, 0.42f, 0.20f);
            if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Glow) color = Vec3Make(0.20f, 1.0f, 1.0f);
            if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Wetness) color = Vec3Make(0.20f, 0.55f, 1.0f);
            if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Vanish) color = Vec3Make(1.0f, 0.25f, 1.0f);
            debugOptions.selectionSphereColors[sphereIndex] = Vec4Make(color.x, color.y, color.z, 1.0f);
        }

        Vec3 beamEnd = paint.surfaceBrushHitPosition;
        Vec3 beamColor = Vec3Make(0.25f, 1.0f, 0.95f);
        if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Grime) beamColor = Vec3Make(0.60f, 0.42f, 0.20f);
        if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Glow) beamColor = Vec3Make(0.20f, 1.0f, 1.0f);
        if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Wetness) beamColor = Vec3Make(0.20f, 0.55f, 1.0f);
        if (paint.surfaceMaskBrush.channel == SurfaceMaskChannel::Vanish) beamColor = Vec3Make(1.0f, 0.25f, 1.0f);
        if (cubeIndex < DebugRenderOptions::kMaxCustomCubes)
        {
            debugOptions.customCubes[cubeIndex] = Vec4Make(beamEnd.x, beamEnd.y, beamEnd.z, 0.045f);
            debugOptions.customCubeColors[cubeIndex] = Vec4Make(beamColor.x, beamColor.y, beamColor.z, 1.0f);
            ++cubeIndex;
        }
    }
    debugOptions.customCubeCount = cubeIndex;
    if (lighting.sceneKind == SceneKind::FractureTest || lighting.sceneKind == SceneKind::City)
    {
        if (fracture.hitValid && debugOptions.selectionSphereCount < DebugRenderOptions::kMaxSelectionSpheres)
        {
            std::uint32_t sphereIndex = debugOptions.selectionSphereCount++;
            debugOptions.selectionSpheres[sphereIndex] = Vec4Make(
                fracture.hitPosition.x,
                fracture.hitPosition.y,
                fracture.hitPosition.z,
                fracture.settings.mesh.radius
            );
            debugOptions.selectionSphereColors[sphereIndex] = Vec4Make(1.0f, 0.55f, 0.15f, 1.0f);
        }

        if (fracture.hitValid && fracture.settings.mesh.mode == damage::Mode::Punch)
        {
            const float radius = fracture.settings.mesh.radius;
            const float innerRadius = radius * std::clamp(fracture.settings.mesh.punchInnerRadiusScale, 0.05f, 0.95f);
            const float coreRadius = radius * std::clamp(
                fracture.settings.mesh.punchCoreRadiusScale,
                0.02f,
                std::clamp(fracture.settings.mesh.punchInnerRadiusScale, 0.05f, 0.95f) - 0.01f);

            if (debugOptions.selectionSphereCount < DebugRenderOptions::kMaxSelectionSpheres)
            {
                std::uint32_t sphereIndex = debugOptions.selectionSphereCount++;
                debugOptions.selectionSpheres[sphereIndex] = Vec4Make(
                    fracture.hitPosition.x,
                    fracture.hitPosition.y,
                    fracture.hitPosition.z,
                    innerRadius
                );
                debugOptions.selectionSphereColors[sphereIndex] = Vec4Make(1.0f, 0.9f, 0.18f, 1.0f);
            }

            if (debugOptions.selectionSphereCount < DebugRenderOptions::kMaxSelectionSpheres)
            {
                std::uint32_t sphereIndex = debugOptions.selectionSphereCount++;
                debugOptions.selectionSpheres[sphereIndex] = Vec4Make(
                    fracture.hitPosition.x,
                    fracture.hitPosition.y,
                    fracture.hitPosition.z,
                    coreRadius
                );
                debugOptions.selectionSphereColors[sphereIndex] = Vec4Make(1.0f, 0.15f, 0.15f, 1.0f);
            }
        }

        if (fracture.showWireframe)
        {
            std::uint32_t lineIndex = 0;
            static constexpr std::string_view kCityBuildingPrefix = "generated/city_building_";
            for (const EntityData& entity : core.scene.entities)
            {
                if (!entity.collidable || entity.modelIndex >= core.scene.models.size())
                {
                    continue;
                }

                if (lighting.sceneKind == SceneKind::City)
                {
                    if (entity.assetPath.compare(0, kCityBuildingPrefix.size(), kCityBuildingPrefix) != 0)
                    {
                        continue;
                    }

                    Vec3 entityCenter = TransformPoint(entity.transform, Vec3Make(0.0f, 0.0f, 0.0f));
                    if (Vec3Length(Vec3Sub(entityCenter, core.camera.position)) > 20.0f)
                    {
                        continue;
                    }
                }

                const ModelData& model = core.scene.models[entity.modelIndex];
                const auto appendEdge = [&](Vec3 a, Vec3 b)
                {
                    if (lineIndex >= DebugRenderOptions::kMaxCustomLines)
                    {
                        return;
                    }
                    debugOptions.customLineStarts[lineIndex] = Vec4Make(a.x, a.y, a.z, 1.0f);
                    debugOptions.customLineEnds[lineIndex] = Vec4Make(b.x, b.y, b.z, 1.0f);
                    debugOptions.customLineColors[lineIndex] = Vec4Make(0.12f, 0.95f, 0.55f, 1.0f);
                    ++lineIndex;
                };

                for (std::size_t i = 0; i + 2 < model.mesh.indices.size() && lineIndex + 2 < DebugRenderOptions::kMaxCustomLines; i += 3)
                {
                    std::uint32_t i0 = model.mesh.indices[i + 0];
                    std::uint32_t i1 = model.mesh.indices[i + 1];
                    std::uint32_t i2 = model.mesh.indices[i + 2];
                    if (i0 >= model.mesh.vertices.size() || i1 >= model.mesh.vertices.size() || i2 >= model.mesh.vertices.size())
                    {
                        continue;
                    }
                    Vec3 a = TransformPoint(entity.transform, model.mesh.vertices[i0].position);
                    Vec3 b = TransformPoint(entity.transform, model.mesh.vertices[i1].position);
                    Vec3 c = TransformPoint(entity.transform, model.mesh.vertices[i2].position);
                    appendEdge(a, b);
                    appendEdge(b, c);
                    appendEdge(c, a);
                }
            }
            debugOptions.customLineCount = lineIndex;
        }
    }

    auto renderStart = Clock::now();
    core.renderer.Render(
        uniforms,
        text,
        &core.flatDecals,
        runtime.hasCharacter ? &core.characterRenderState : nullptr,
        &debugOptions
    );
    auto renderEnd = Clock::now();
    runtime.cpuProfiling.renderMs = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();
    runtime.cpuProfiling.frameMs = std::chrono::duration<float, std::milli>(renderEnd - frameStart).count();
}
