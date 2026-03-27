#include "app.h"

#include "imgui.h"

#include <algorithm>

namespace
{
constexpr float kWindowPad = 16.0f;
constexpr ImVec2 kLeftColumnWindowSize(340.0f, 0.0f);
constexpr ImVec2 kRightColumnWindowSize(300.0f, 348.0f);

void DrawProfilerMetric(const char* label, float valueMs, float totalMs, ImU32 color)
{
    float fraction = totalMs > 0.0001f ? std::clamp(valueMs / totalMs, 0.0f, 1.0f) : 0.0f;
    ImGui::Text("%-10s %6.2f ms  %5.1f%%", label, valueMs, fraction * 100.0f);
    ImVec2 start = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = 10.0f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height), IM_COL32(40, 44, 52, 255), 3.0f);
    draw->AddRectFilled(start, ImVec2(start.x + width * fraction, start.y + height), color, 3.0f);
    ImGui::Dummy(ImVec2(width, height + 6.0f));
}
}

void App::BuildLightingWindow(bool& debugSettingsChanged)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    ImGui::SetNextWindowPos(ImVec2(kWindowPad, kWindowPad), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(kLeftColumnWindowSize, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Lighting"))
    {
        int sceneKind = static_cast<int>(lighting.sceneKind);
        const char* sceneNames[] = {"Player Mask Test", "City", "Shadow Test", "Spot Shadow Test", "Vehicle Light Test"};
        if (ImGui::Combo("Scene", &sceneKind, sceneNames, 5))
        {
            lighting.sceneKind = static_cast<SceneKind>(sceneKind);
            runtime.reloadSceneRequested = true;
            debugSettingsChanged = true;
        }

        float ms = runtime.smoothedFps > 0.0f ? 1000.0f / runtime.smoothedFps : 0.0f;
        ImGui::Text("%.2f ms  %.0f fps", ms, runtime.smoothedFps);
        ImGui::Text("%u ents  %u tris", static_cast<std::uint32_t>(core.scene.entities.size()), runtime.sceneTriangleCount);
        ImGui::Separator();
        debugSettingsChanged |= ImGui::Checkbox("Cycle day/night", &lighting.cycleDayNight);
        debugSettingsChanged |= ImGui::SliderFloat("Time of day", &lighting.timeOfDay, 0.0f, 1.0f, "%.3f");
        debugSettingsChanged |= ImGui::SliderFloat("Cycle speed", &lighting.dayNightSpeed, 0.0f, 0.20f, "%.3f");
        debugSettingsChanged |= ImGui::Checkbox("Animate azimuth", &lighting.animateSunAzimuth);
        debugSettingsChanged |= ImGui::SliderFloat("Sun azimuth", &lighting.sunAzimuthDegrees, -180.0f, 180.0f, "%.1f deg");
        debugSettingsChanged |= ImGui::Checkbox("Visualize UVs", &lighting.debugVisualizeUv);
        static const char* uvModeNames[] = {"Checker", "U", "V", "UV Color", "Gradient U", "Gradient V", "Out of 0..1"};
        int uvMode = static_cast<int>(lighting.uvDebugMode);
        debugSettingsChanged |= ImGui::Combo("UV mode", &uvMode, uvModeNames, 7);
        lighting.uvDebugMode = static_cast<std::uint32_t>(uvMode);
        debugSettingsChanged |= ImGui::SliderFloat("UV scale", &lighting.uvDebugScale, 1.0f, 32.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Orbit distance", &lighting.orbitDistanceScale, 0.5f, 3.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Sun intensity", &lighting.sunIntensity, 0.0f, 3.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Moon intensity", &lighting.moonIntensity, 0.0f, 1.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Ambient intensity", &lighting.ambientIntensity, 0.0f, 1.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Point lights", &lighting.pointLightIntensity, 0.0f, 3.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Cascade split", &lighting.shadowCascadeSplit, 8.0f, 96.0f, "%.1f");
        ImGui::Separator();
        ImGui::Text("%s", runtime.mouseCaptured ? "Play mode: F1 or Esc to release mouse" : "Mouse mode: F1 to enter play mode");
        ImGui::Text("Sun  %.1f %.1f %.1f", lighting.sunWorldPosition.x, lighting.sunWorldPosition.y, lighting.sunWorldPosition.z);
        ImGui::Text("Moon %.1f %.1f %.1f", lighting.moonWorldPosition.x, lighting.moonWorldPosition.y, lighting.moonWorldPosition.z);
    }
    ImGui::End();
}

void App::BuildProfilerWindow()
{
    auto& runtime = m_state.runtime;
    auto& core = m_state.core;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 profilerPos(viewport->WorkPos.x + kWindowPad, viewport->WorkPos.y + 360.0f);
    ImGui::SetNextWindowPos(profilerPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 250.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Profiler"))
    {
        ImGui::Text("Visible draw items: %u / %u", core.renderer.GetVisibleDrawItemCount(), core.renderer.GetDrawItemCount());
        ImGui::Spacing();

        ImGui::Text("CPU");
        ImGui::Separator();
        float cpuTotal = std::max(runtime.cpuProfiling.frameMs, 0.0001f);
        DrawProfilerMetric("input", runtime.cpuProfiling.inputMs, cpuTotal, IM_COL32(90, 170, 255, 255));
        DrawProfilerMetric("imgui", runtime.cpuProfiling.imguiMs, cpuTotal, IM_COL32(120, 220, 140, 255));
        DrawProfilerMetric("lighting", runtime.cpuProfiling.lightingMs, cpuTotal, IM_COL32(255, 190, 90, 255));
        DrawProfilerMetric("render", runtime.cpuProfiling.renderMs, cpuTotal, IM_COL32(255, 120, 120, 255));
        DrawProfilerMetric("frame", runtime.cpuProfiling.frameMs, cpuTotal, IM_COL32(210, 210, 220, 255));

        const RenderProfilingStats& gpu = core.renderer.GetProfilingStats();
        if (gpu.gpuValid)
        {
            ImGui::Spacing();
            ImGui::Text("GPU");
            ImGui::Separator();
            float gpuTotal = std::max(gpu.gpuFrameMs, 0.0001f);
            DrawProfilerMetric("shadow", gpu.gpuShadowMs, gpuTotal, IM_COL32(180, 120, 255, 255));
            DrawProfilerMetric("sun", gpu.gpuSunShadowMs, gpuTotal, IM_COL32(150, 110, 255, 255));
            DrawProfilerMetric("spot", gpu.gpuSpotShadowMs, gpuTotal, IM_COL32(220, 150, 255, 255));
            DrawProfilerMetric("main", gpu.gpuMainMs, gpuTotal, IM_COL32(90, 170, 255, 255));
            DrawProfilerMetric("debug", gpu.gpuDebugMs, gpuTotal, IM_COL32(120, 220, 140, 255));
            DrawProfilerMetric("ui", gpu.gpuUiMs, gpuTotal, IM_COL32(255, 190, 90, 255));
            DrawProfilerMetric("frame", gpu.gpuFrameMs, gpuTotal, IM_COL32(210, 210, 220, 255));
        }
        else
        {
            ImGui::Spacing();
            ImGui::TextUnformatted("GPU timestamps not available yet");
        }
    }
    ImGui::End();
}

void App::BuildSunViewWindow()
{
    auto& core = m_state.core;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(
        viewport->WorkPos.x + viewport->WorkSize.x - kRightColumnWindowSize.x - kWindowPad,
        viewport->WorkPos.y + kWindowPad
    );
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(kRightColumnWindowSize, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Sun View"))
    {
        ImGui::Text("Cascade 0");
        ImGui::Separator();
        ImGui::Image(core.renderer.GetShadowDebugTexture(0), ImVec2(268.0f, 128.0f));
        ImGui::Separator();
        ImGui::Text("Cascade 1");
        ImGui::Separator();
        ImGui::Image(core.renderer.GetShadowDebugTexture(1), ImVec2(268.0f, 128.0f));
    }
    ImGui::End();
}

void App::BuildShadowsWindow(bool& debugSettingsChanged)
{
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(
        viewport->WorkPos.x + viewport->WorkSize.x - kRightColumnWindowSize.x - kWindowPad,
        viewport->WorkPos.y + kRightColumnWindowSize.y + kWindowPad * 2.0f
    );
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 132.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Shadows"))
    {
        static constexpr int kShadowSizes[] = {512, 1024, 2048, 4096};
        int currentSizeIndex = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (lighting.shadowMapSize == static_cast<std::uint32_t>(kShadowSizes[i]))
            {
                currentSizeIndex = i;
                break;
            }
        }

        const char* shadowSizeLabels[] = {"512", "1024", "2048", "4096"};
        if (ImGui::Combo("Map size", &currentSizeIndex, shadowSizeLabels, 4))
        {
            lighting.shadowMapSize = static_cast<std::uint32_t>(kShadowSizes[currentSizeIndex]);
            runtime.reloadSceneRequested = true;
            debugSettingsChanged = true;
        }

        debugSettingsChanged |= ImGui::Checkbox("Blur (3x3 PCF)", &lighting.shadowBlur);
        debugSettingsChanged |= ImGui::SliderFloat("Main draw dist", &lighting.mainDrawDistance, 24.0f, 320.0f, "%.0f");
        debugSettingsChanged |= ImGui::SliderFloat("Shadow dist", &lighting.shadowDrawDistance, 24.0f, 320.0f, "%.0f");
        ImGui::Text("Current: %u x %u", lighting.shadowMapSize, lighting.shadowMapSize);
    }
    ImGui::End();
}

void App::BuildSpotlightsWindow(bool& debugSettingsChanged)
{
    auto& core = m_state.core;
    auto& lighting = m_state.lighting;
    auto& vehicle = m_state.vehicleLights;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(
        viewport->WorkPos.x + viewport->WorkSize.x - kRightColumnWindowSize.x - kWindowPad,
        viewport->WorkPos.y + kRightColumnWindowSize.y + 132.0f + kWindowPad * 3.0f
    );
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 252.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Spotlights"))
    {
        debugSettingsChanged |= ImGui::SliderFloat("Intensity", &lighting.spotLightIntensityScale, 0.0f, 4.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Range", &lighting.spotLightRangeScale, 0.25f, 2.5f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Inner angle", &lighting.spotLightInnerAngleDegrees, 4.0f, 40.0f, "%.1f deg");
        debugSettingsChanged |= ImGui::SliderFloat("Outer angle", &lighting.spotLightOuterAngleDegrees, 8.0f, 60.0f, "%.1f deg");
        int maxActive = static_cast<int>(lighting.spotLightMaxActive);
        debugSettingsChanged |= ImGui::SliderInt("Max active", &maxActive, 0, static_cast<int>(kMaxSceneSpotLights));
        lighting.spotLightMaxActive = static_cast<std::uint32_t>(maxActive);
        debugSettingsChanged |= ImGui::SliderFloat("Activation dist", &lighting.spotLightActivationDistance, 2.0f, 96.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Forward offset", &lighting.spotLightActivationForwardOffset, -16.0f, 48.0f, "%.1f");
        int shadowedMaxActive = static_cast<int>(lighting.shadowedSpotLightMaxActive);
        debugSettingsChanged |= ImGui::SliderInt("Shadowed active", &shadowedMaxActive, 0, static_cast<int>(kMaxShadowedSpotLights));
        lighting.shadowedSpotLightMaxActive = static_cast<std::uint32_t>(shadowedMaxActive);
        debugSettingsChanged |= ImGui::SliderFloat("Shadowed dist", &lighting.shadowedSpotLightActivationDistance, 2.0f, 64.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Shadowed fwd", &lighting.shadowedSpotLightActivationForwardOffset, -16.0f, 32.0f, "%.1f");
        debugSettingsChanged |= ImGui::DragFloat3("Source offset", &lighting.spotLightSourceOffset.x, 0.01f, -2.0f, 2.0f, "%.3f");
        debugSettingsChanged |= ImGui::Checkbox("Show light proxies", &lighting.drawLightProxies);
        debugSettingsChanged |= ImGui::Checkbox("Show activation", &lighting.debugDrawActivationVolumes);
        debugSettingsChanged |= ImGui::Checkbox("Show light gizmos", &lighting.debugDrawSceneLightGizmos);
        debugSettingsChanged |= ImGui::Checkbox("Show direction lines", &lighting.debugDrawLightDirections);
        debugSettingsChanged |= ImGui::Checkbox("Show light volumes", &lighting.debugDrawLightVolumes);
        debugSettingsChanged |= ImGui::Checkbox("Show labels", &lighting.debugDrawLightLabels);
        debugSettingsChanged |= ImGui::Checkbox("Show vehicle volumes", &vehicle.debugDrawVehicleVolumes);
        if (lighting.spotLightOuterAngleDegrees < lighting.spotLightInnerAngleDegrees)
        {
            lighting.spotLightOuterAngleDegrees = lighting.spotLightInnerAngleDegrees;
            debugSettingsChanged = true;
        }
        ImGui::Text("Scene spots: %zu", core.scene.spotLights.size());
        ImGui::Text("GPU cap: %u", static_cast<std::uint32_t>(kMaxSceneSpotLights));
        ImGui::Text("Active budget: %u", lighting.spotLightMaxActive);
        ImGui::Text("Shadowed cap: %u", static_cast<std::uint32_t>(kMaxShadowedSpotLights));
        if (lighting.sceneKind == SceneKind::ShadowTest)
        {
            ImGui::Separator();
            ImGui::Text("Left click geometry to aim");
            if (lighting.shadowTestSpotTargetValid)
            {
                ImGui::Text("Target %.2f %.2f %.2f", lighting.shadowTestSpotTargetWorld.x, lighting.shadowTestSpotTargetWorld.y, lighting.shadowTestSpotTargetWorld.z);
                ImGui::Text("Offset %.2f %.2f %.2f", lighting.shadowTestSpotTargetOffset.x, lighting.shadowTestSpotTargetOffset.y, lighting.shadowTestSpotTargetOffset.z);
            }
        }
    }
    ImGui::End();
}

void App::BuildPaintBallsWindow(bool& debugSettingsChanged)
{
    auto& core = m_state.core;
    auto& paint = m_state.paint;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(viewport->WorkPos.x + kWindowPad, viewport->WorkPos.y + 628.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Paint Balls"))
    {
        ImGui::Text("Active: %u / %u", core.paintBalls.ActiveCount(), PaintBallSettings::kMaxBalls);
        ImGui::Text("Paint splats: %u / %u", paint.splatCount, kMaxPaintSplats);
        ImGui::Text("Accumulated: %u", CountAccumulatedPaintStamps());
        if (ImGui::Button("Reset paint balls"))
        {
            core.paintBalls.Reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset paint"))
        {
                paint.splatCount = 0;
                paint.nextSplatIndex = 0;
                paint.splats = {};
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset accumulated"))
        {
            core.renderer.ResetAccumulatedPaint();
        }

        int bounceLimit = static_cast<int>(paint.ballSettings.bounceLimit);
        debugSettingsChanged |= ImGui::SliderInt("Bounce limit", &bounceLimit, 0, 12);
        paint.ballSettings.bounceLimit = static_cast<std::uint32_t>(bounceLimit);
        debugSettingsChanged |= ImGui::SliderFloat("Shoot speed", &paint.ballSettings.shootSpeed, 6.0f, 48.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Fire rate", &paint.ballSettings.fireRate, 1.0f, 40.0f, "%.1f /s");
        debugSettingsChanged |= ImGui::SliderFloat("Gravity", &paint.ballSettings.gravity, 4.0f, 36.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Restitution", &paint.ballSettings.restitution, 0.0f, 0.95f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Ball radius", &paint.ballSettings.radius, 0.04f, 0.30f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Blob radius", &paint.ballSettings.blobRadius, 0.08f, 1.20f, "%.2f");
        debugSettingsChanged |= ImGui::ColorEdit3("Paint color", &paint.ballSettings.baseColor.x);
        debugSettingsChanged |= ImGui::Checkbox("Cycle color on shoot", &paint.ballSettings.cycleColorOnShoot);
        ImGui::Separator();
        ImGui::TextUnformatted("Play mode: hold left click to rapid fire paint balls");
        ImGui::TextUnformatted("Mouse mode: Shift + Left click places test-scene tools");
    }
    ImGui::End();
}

void App::BuildSurfaceMasksWindow(bool& debugSettingsChanged)
{
    auto& paint = m_state.paint;
    auto& lighting = m_state.lighting;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(
        viewport->WorkPos.x + viewport->WorkSize.x - kRightColumnWindowSize.x - kWindowPad,
        viewport->WorkPos.y + kRightColumnWindowSize.y + 132.0f + 252.0f + kWindowPad * 4.0f
    );
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Surface Masks"))
    {
        static const char* interactionNames[] = {"Paint Balls", "Surface Brush"};
        int interactionMode = static_cast<int>(paint.interactionMode);
        debugSettingsChanged |= ImGui::Combo("Interaction", &interactionMode, interactionNames, 2);
        paint.interactionMode = static_cast<PaintInteractionMode>(interactionMode);

        static const char* maskChannelNames[] = {"Grime", "Glow", "Wetness", "Vanish"};
        int maskChannel = static_cast<int>(paint.surfaceMaskBrush.channel);
        debugSettingsChanged |= ImGui::Combo("Brush channel", &maskChannel, maskChannelNames, 4);
        paint.surfaceMaskBrush.channel = static_cast<SurfaceMaskChannel>(maskChannel);
        debugSettingsChanged |= ImGui::SliderFloat("Brush radius", &paint.surfaceMaskBrush.radius, 0.05f, 1.20f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Brush strength", &paint.surfaceMaskBrush.strength, 0.05f, 1.0f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Brush rate", &paint.surfaceMaskBrush.flowRate, 1.0f, 48.0f, "%.1f /s");
        ImGui::Separator();
        ImGui::TextUnformatted("Layer meanings in Player Mask Test:");
        ImGui::BulletText("R grime");
        ImGui::BulletText("G glow");
        ImGui::BulletText("B wetness");
        ImGui::BulletText("A vanish");
        ImGui::Separator();
        if (lighting.sceneKind == SceneKind::PlayerMaskTest)
        {
            ImGui::TextUnformatted("Surface Brush uses a direct camera ray and does not spawn paint splats.");
        }
        else
        {
            ImGui::TextUnformatted("Persistent surface masks are only active in Player Mask Test.");
        }
    }
    ImGui::End();
}

void App::BuildVehicleLightsWindow(bool& debugSettingsChanged)
{
    auto& core = m_state.core;
    auto& lighting = m_state.lighting;
    auto& vehicle = m_state.vehicleLights;
    if (lighting.sceneKind != SceneKind::VehicleLightTest)
    {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(
        viewport->WorkPos.x + viewport->WorkSize.x - kRightColumnWindowSize.x - kWindowPad,
        viewport->WorkPos.y + kRightColumnWindowSize.y + 132.0f + 252.0f + kWindowPad * 4.0f
    );
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Vehicle Lights"))
    {
        int activeVehicle = FindActiveVehicleLightIndex();
        ImGui::Text("Active vehicle: %d", activeVehicle);
        if (activeVehicle >= 0)
        {
            const auto& item = core.scene.vehicleLightTestItems[static_cast<std::size_t>(activeVehicle)];
            VehicleLightRig& rig = vehicle.rigs[item.assetPath];
            int slot = static_cast<int>(vehicle.slot);
            const char* slotNames[] = {"Head A", "Head B", "Rear A", "Rear B"};
            debugSettingsChanged |= ImGui::Combo("Edit slot", &slot, slotNames, 4);
            vehicle.slot = static_cast<VehicleLightSlot>(slot);
            if (vehicle.slot == VehicleLightSlot::HeadA || vehicle.slot == VehicleLightSlot::HeadB)
            {
                VehicleFrontLightConfig& light = vehicle.slot == VehicleLightSlot::HeadA ? rig.headA : rig.headB;
                debugSettingsChanged |= ImGui::DragFloat3("Offset", &light.offset.x, 0.01f, -4.0f, 4.0f, "%.3f");
                debugSettingsChanged |= ImGui::SliderFloat("Yaw", &light.yawDegrees, -180.0f, 180.0f, "%.1f");
                debugSettingsChanged |= ImGui::SliderFloat("Pitch", &light.pitchDegrees, -89.0f, 89.0f, "%.1f");
                debugSettingsChanged |= ImGui::SliderFloat("Range", &light.range, 1.0f, 24.0f, "%.1f");
            }
            else
            {
                VehicleRearLightConfig& light = vehicle.slot == VehicleLightSlot::RearA ? rig.rearA : rig.rearB;
                debugSettingsChanged |= ImGui::DragFloat3("Offset", &light.offset.x, 0.01f, -4.0f, 4.0f, "%.3f");
                debugSettingsChanged |= ImGui::SliderFloat("Range", &light.range, 0.5f, 12.0f, "%.1f");
                debugSettingsChanged |= ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 8.0f, "%.2f");
            }
            ImGui::Separator();
            ImGui::Text("Left click on vehicle to place selected slot");
            ImGui::TextUnformatted(item.assetPath.c_str());
        }
        else
        {
            ImGui::Text("Stand inside a vehicle volume");
        }
    }
    ImGui::End();
}
