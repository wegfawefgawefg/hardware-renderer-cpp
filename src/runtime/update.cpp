#include "app.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "imgui.h"

namespace
{
constexpr float kTitleRefreshPeriod = 0.20f;
constexpr float kOverlayRefreshPeriod = 0.12f;
}

void App::HandleEvent(const SDL_Event& event)
{
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
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
                paint.fireHeld = true;
                TryFirePaintBall();
            }
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            paint.fireHeld = false;
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
    auto& paint = m_state.paint;
    auto& overlay = m_state.overlay;

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
    PlayerUpdateFromInput(core.player, core.worldCollider, core.camera, dtSeconds, runtime.mouseCaptured && !imguiCapturingKeyboard);
    PlayerSyncCamera(core.player, core.worldCollider, core.camera);
    core.traffic.Update(core.scene, dtSeconds, 64);
    UpdatePaintBalls(dtSeconds);
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
    uniforms.cameraPosition = Vec4Make(core.camera.position.x, core.camera.position.y, core.camera.position.z, 1.0f);

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
    uniforms.paintSplatCounts = Vec4Make(static_cast<float>(paint.splatCount), 0.0f, 0.0f, 0.0f);
    for (std::uint32_t i = 0; i < paint.splatCount && i < kMaxPaintSplats; ++i)
    {
        const PaintSplat& splat = paint.splats[i];
        uniforms.paintSplatPositions[i] = Vec4Make(splat.position.x, splat.position.y, splat.position.z, splat.radius);
        uniforms.paintSplatNormals[i] = Vec4Make(splat.normal.x, splat.normal.y, splat.normal.z, 0.0f);
        uniforms.paintSplatColors[i] = Vec4Make(splat.color.x, splat.color.y, splat.color.z, 1.0f);
    }

    if (runtime.hasCharacter)
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
        UpdateOverlayText(&uniforms);
        runtime.overlayRefreshSeconds = kOverlayRefreshPeriod;
    }

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

    core.renderer.FlushDirtyPaintTextures();

    auto renderStart = Clock::now();
    core.renderer.Render(
        uniforms,
        overlay.pixels,
        overlay.width,
        overlay.height,
        runtime.hasCharacter ? &core.characterRenderState : nullptr,
        &debugOptions
    );
    auto renderEnd = Clock::now();
    runtime.cpuProfiling.renderMs = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();
    runtime.cpuProfiling.frameMs = std::chrono::duration<float, std::milli>(renderEnd - frameStart).count();
}
