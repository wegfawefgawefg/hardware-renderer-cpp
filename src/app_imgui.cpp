#include "app.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
Vec3 RotateYOffset(Vec3 v, float yawDegrees)
{
    float radians = DegreesToRadians(yawDegrees);
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Make(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

Vec3 NormalizeOrFallback(Vec3 v, Vec3 fallback)
{
    float length = Vec3Length(v);
    return length > 1e-5f ? Vec3Scale(v, 1.0f / length) : fallback;
}

Vec3 VehicleLightDirection(float yawDegrees, float pitchDegrees)
{
    float yaw = DegreesToRadians(yawDegrees);
    float pitch = DegreesToRadians(pitchDegrees);
    float cp = std::cos(pitch);
    return NormalizeOrFallback(
        Vec3Make(std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp),
        Vec3Make(0.0f, -0.15f, 1.0f)
    );
}

bool ProjectWorldToScreen(const App& app, Vec3 world, ImVec2& outScreen)
{
    if (app.m_windowWidth == 0 || app.m_windowHeight == 0)
    {
        return false;
    }

    Mat4 view = CameraViewMatrix(app.m_camera);
    Mat4 proj = Mat4Perspective(
        DegreesToRadians(60.0f),
        static_cast<float>(app.m_windowWidth) / static_cast<float>(app.m_windowHeight),
        0.1f,
        200.0f
    );
    proj.m[5] *= -1.0f;

    Vec4 clip = Mat4MulVec4(Mat4Mul(proj, view), Vec4Make(world.x, world.y, world.z, 1.0f));
    if (clip.w <= 0.0001f)
    {
        return false;
    }

    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;
    float ndcZ = clip.z * invW;
    if (ndcZ < -1.0f || ndcZ > 1.0f)
    {
        return false;
    }

    outScreen.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(app.m_windowWidth);
    outScreen.y = (ndcY * 0.5f + 0.5f) * static_cast<float>(app.m_windowHeight);
    return true;
}

struct DebugSpotSelection
{
    std::vector<std::uint32_t> shadowed;
    std::vector<std::uint32_t> unshadowed;
};

DebugSpotSelection CollectDebugSpotSelection(const App& app)
{
    struct Candidate
    {
        float dist2 = 1e30f;
        std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    };

    DebugSpotSelection result{};
    std::array<Candidate, kMaxShadowedSpotLights> shadowed{};
    std::array<Candidate, kMaxSceneSpotLights> unshadowed{};
    for (Candidate& c : shadowed) c = Candidate{};
    for (Candidate& c : unshadowed) c = Candidate{};

    std::uint32_t shadowedBudget = std::min(app.m_shadowedSpotLightMaxActive, kMaxShadowedSpotLights);
    std::uint32_t unshadowedBudget = std::min(app.m_spotLightMaxActive, kMaxSceneSpotLights);
    Vec3 forward = CameraForward(app.m_camera);
    Vec3 shadowedCenter = Vec3Add(app.m_camera.position, Vec3Scale(forward, app.m_shadowedSpotLightActivationForwardOffset));
    Vec3 unshadowedCenter = Vec3Add(app.m_camera.position, Vec3Scale(forward, app.m_spotLightActivationForwardOffset));
    float shadowedDist2Max = app.m_shadowedSpotLightActivationDistance * app.m_shadowedSpotLightActivationDistance;
    float unshadowedDist2Max = app.m_spotLightActivationDistance * app.m_spotLightActivationDistance;

    auto insertCandidate = [](auto& arr, std::uint32_t budget, float dist2, std::uint32_t index) {
        for (std::uint32_t i = 0; i < budget; ++i)
        {
            if (dist2 >= arr[i].dist2)
            {
                continue;
            }
            for (std::uint32_t j = budget - 1; j > i; --j)
            {
                arr[j] = arr[j - 1];
            }
            arr[i] = Candidate{dist2, index};
            break;
        }
    };

    for (std::uint32_t i = 0; i < app.m_scene.spotLights.size(); ++i)
    {
        const SpotLightData& light = app.m_scene.spotLights[i];
        Vec3 lightPos = Vec3Add(light.position, RotateYOffset(app.m_spotLightSourceOffset, light.yawDegrees));
        Vec3 d0 = Vec3Sub(lightPos, shadowedCenter);
        float sd2 = Vec3Dot(d0, d0);
        if (sd2 <= shadowedDist2Max && shadowedBudget > 0)
        {
            insertCandidate(shadowed, shadowedBudget, sd2, i);
        }

        Vec3 d1 = Vec3Sub(lightPos, unshadowedCenter);
        float ud2 = Vec3Dot(d1, d1);
        if (ud2 <= unshadowedDist2Max && unshadowedBudget > 0)
        {
            insertCandidate(unshadowed, unshadowedBudget, ud2, i);
        }
    }

    std::vector<bool> used(app.m_scene.spotLights.size(), false);
    for (std::uint32_t i = 0; i < shadowedBudget; ++i)
    {
        if (shadowed[i].index < app.m_scene.spotLights.size())
        {
            result.shadowed.push_back(shadowed[i].index);
            used[shadowed[i].index] = true;
        }
    }
    for (std::uint32_t i = 0; i < unshadowedBudget; ++i)
    {
        if (unshadowed[i].index < app.m_scene.spotLights.size() && !used[unshadowed[i].index])
        {
            result.unshadowed.push_back(unshadowed[i].index);
        }
    }
    return result;
}

void DrawLightDebugOverlay(const App& app)
{
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (app.m_sceneKind == SceneKind::VehicleLightTest)
    {
        int activeVehicle = app.FindActiveVehicleLightIndex();
        if (activeVehicle >= 0)
        {
            const auto& item = app.m_scene.vehicleLightTestItems[static_cast<std::size_t>(activeVehicle)];
            App::VehicleLightRig rig = {};
            if (auto found = app.m_vehicleLightRigs.find(item.assetPath); found != app.m_vehicleLightRigs.end())
            {
                rig = found->second;
            }

            auto worldPoint = [&](Vec3 local) {
                return Vec3Add(item.origin, Vec3Scale(local, item.scale));
            };
            struct LightViz
            {
                const char* label;
                Vec3 position;
                Vec3 direction;
                float range;
                ImU32 color;
                bool directional;
            };
            std::array<LightViz, 4> lights = {{
                {"HA", worldPoint(rig.headA.offset), VehicleLightDirection(rig.headA.yawDegrees, rig.headA.pitchDegrees), rig.headA.range * item.scale, IM_COL32(255, 244, 160, 255), true},
                {"HB", worldPoint(rig.headB.offset), VehicleLightDirection(rig.headB.yawDegrees, rig.headB.pitchDegrees), rig.headB.range * item.scale, IM_COL32(255, 220, 120, 255), true},
                {"RA", worldPoint(rig.rearA.offset), Vec3Make(0.0f, 0.0f, -1.0f), rig.rearA.range * item.scale, IM_COL32(255, 90, 90, 255), false},
                {"RB", worldPoint(rig.rearB.offset), Vec3Make(0.0f, 0.0f, -1.0f), rig.rearB.range * item.scale, IM_COL32(255, 50, 50, 255), false},
            }};

            for (const LightViz& light : lights)
            {
                ImVec2 sourceScreen{};
                if (!ProjectWorldToScreen(app, light.position, sourceScreen))
                {
                    continue;
                }
                drawList->AddCircleFilled(sourceScreen, 4.0f, light.color, 12);
                if (app.m_debugDrawLightLabels)
                {
                    drawList->AddText(ImVec2(sourceScreen.x + 8.0f, sourceScreen.y - 10.0f), light.color, light.label);
                }
            }
        }
        return;
    }

    if (app.m_scene.spotLights.empty())
    {
        return;
    }
    DebugSpotSelection selection = CollectDebugSpotSelection(app);
    std::vector<std::uint8_t> state(app.m_scene.spotLights.size(), 0);
    for (std::uint32_t index : selection.unshadowed) state[index] = 1;
    for (std::uint32_t index : selection.shadowed) state[index] = 2;

    if (!app.m_debugDrawLightLabels)
    {
        return;
    }

    for (std::uint32_t i = 0; i < app.m_scene.spotLights.size(); ++i)
    {
        const SpotLightData& light = app.m_scene.spotLights[i];
        Vec3 source = Vec3Add(light.position, RotateYOffset(app.m_spotLightSourceOffset, light.yawDegrees));
        Vec3 direction = light.direction;
        if (app.m_sceneKind == SceneKind::ShadowTest && i == 0 && app.m_shadowTestSpotTargetValid)
        {
            direction = NormalizeOrFallback(Vec3Sub(app.m_shadowTestSpotTargetWorld, source), direction);
        }
        ImU32 color = IM_COL32(110, 110, 110, 220);
        if (state[i] == 1) color = IM_COL32(0, 255, 255, 255);
        if (state[i] == 2) color = IM_COL32(255, 0, 255, 255);
        if (state[i] == 0)
        {
            continue;
        }

        ImVec2 sourceScreen{};
        if (!ProjectWorldToScreen(app, source, sourceScreen))
        {
            continue;
        }

        if (app.m_debugDrawLightLabels)
        {
            const char* type = state[i] == 2 ? "SH" : (state[i] == 1 ? "SP" : "IN");
            char label[64];
            std::snprintf(label, sizeof(label), "%s %u", type, i);
            drawList->AddText(ImVec2(sourceScreen.x + 8.0f, sourceScreen.y - 8.0f), color, label);
        }
    }
}
}

void App::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL3_InitForVulkan(m_window);
    m_renderer.InitializeImGuiBackend();
}

void App::ShutdownImGui()
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    m_renderer.ShutdownImGuiBackend();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void App::ProcessImGuiEvent(const SDL_Event& event)
{
    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void App::BuildImGui()
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (m_showImGui)
    {
        bool debugSettingsChanged = false;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float pad = 16.0f;
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Lighting"))
        {
            int sceneKind = static_cast<int>(m_sceneKind);
            const char* sceneNames[] = {"City", "Shadow Test", "Spot Shadow Test", "Vehicle Light Test"};
            if (ImGui::Combo("Scene", &sceneKind, sceneNames, 4))
            {
                m_sceneKind = static_cast<SceneKind>(sceneKind);
                m_reloadSceneRequested = true;
                debugSettingsChanged = true;
            }
            float ms = m_smoothedFps > 0.0f ? 1000.0f / m_smoothedFps : 0.0f;
            ImGui::Text("%.2f ms  %.0f fps", ms, m_smoothedFps);
            ImGui::Text("%u ents  %u tris", static_cast<std::uint32_t>(m_scene.entities.size()), m_sceneTriangleCount);
            ImGui::Separator();
            debugSettingsChanged |= ImGui::Checkbox("Cycle day/night", &m_cycleDayNight);
            debugSettingsChanged |= ImGui::SliderFloat("Time of day", &m_timeOfDay, 0.0f, 1.0f, "%.3f");
            debugSettingsChanged |= ImGui::SliderFloat("Cycle speed", &m_dayNightSpeed, 0.0f, 0.20f, "%.3f");
            debugSettingsChanged |= ImGui::Checkbox("Animate azimuth", &m_animateSunAzimuth);
            debugSettingsChanged |= ImGui::SliderFloat("Sun azimuth", &m_sunAzimuthDegrees, -180.0f, 180.0f, "%.1f deg");
            debugSettingsChanged |= ImGui::SliderFloat("Orbit distance", &m_orbitDistanceScale, 0.5f, 3.0f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 3.0f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Moon intensity", &m_moonIntensity, 0.0f, 1.0f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Ambient intensity", &m_ambientIntensity, 0.0f, 1.0f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Point lights", &m_pointLightIntensity, 0.0f, 3.0f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Cascade split", &m_shadowCascadeSplit, 8.0f, 96.0f, "%.1f");
            ImGui::Separator();
            ImGui::Text("RMB capture camera");
            ImGui::Text("Sun  %.1f %.1f %.1f", m_sunWorldPosition.x, m_sunWorldPosition.y, m_sunWorldPosition.z);
            ImGui::Text("Moon %.1f %.1f %.1f", m_moonWorldPosition.x, m_moonWorldPosition.y, m_moonWorldPosition.z);
        }
        ImGui::End();

        ImVec2 profilerPos(
            viewport->WorkPos.x + pad,
            viewport->WorkPos.y + 360.0f
        );
        ImGui::SetNextWindowPos(profilerPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 250.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Profiler"))
        {
            auto drawMetric = [](const char* label, float valueMs, float totalMs, ImU32 color) {
                float fraction = totalMs > 0.0001f ? std::clamp(valueMs / totalMs, 0.0f, 1.0f) : 0.0f;
                ImGui::Text("%-10s %6.2f ms  %5.1f%%", label, valueMs, fraction * 100.0f);
                ImVec2 start = ImGui::GetCursorScreenPos();
                float width = ImGui::GetContentRegionAvail().x;
                float height = 10.0f;
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height), IM_COL32(40, 44, 52, 255), 3.0f);
                draw->AddRectFilled(start, ImVec2(start.x + width * fraction, start.y + height), color, 3.0f);
                ImGui::Dummy(ImVec2(width, height + 6.0f));
            };

            ImGui::Text("Visible draw items: %u / %u",
                m_renderer.GetVisibleDrawItemCount(),
                m_renderer.GetDrawItemCount());
            ImGui::Spacing();

            ImGui::Text("CPU");
            ImGui::Separator();
            float cpuTotal = std::max(m_cpuProfiling.frameMs, 0.0001f);
            drawMetric("input", m_cpuProfiling.inputMs, cpuTotal, IM_COL32(90, 170, 255, 255));
            drawMetric("imgui", m_cpuProfiling.imguiMs, cpuTotal, IM_COL32(120, 220, 140, 255));
            drawMetric("lighting", m_cpuProfiling.lightingMs, cpuTotal, IM_COL32(255, 190, 90, 255));
            drawMetric("render", m_cpuProfiling.renderMs, cpuTotal, IM_COL32(255, 120, 120, 255));
            drawMetric("frame", m_cpuProfiling.frameMs, cpuTotal, IM_COL32(210, 210, 220, 255));

            const RenderProfilingStats& gpu = m_renderer.GetProfilingStats();
            if (gpu.gpuValid)
            {
                ImGui::Spacing();
                ImGui::Text("GPU");
                ImGui::Separator();
                float gpuTotal = std::max(gpu.gpuFrameMs, 0.0001f);
                drawMetric("shadow", gpu.gpuShadowMs, gpuTotal, IM_COL32(180, 120, 255, 255));
                drawMetric("sun", gpu.gpuSunShadowMs, gpuTotal, IM_COL32(150, 110, 255, 255));
                drawMetric("spot", gpu.gpuSpotShadowMs, gpuTotal, IM_COL32(220, 150, 255, 255));
                drawMetric("main", gpu.gpuMainMs, gpuTotal, IM_COL32(90, 170, 255, 255));
                drawMetric("debug", gpu.gpuDebugMs, gpuTotal, IM_COL32(120, 220, 140, 255));
                drawMetric("ui", gpu.gpuUiMs, gpuTotal, IM_COL32(255, 190, 90, 255));
                drawMetric("frame", gpu.gpuFrameMs, gpuTotal, IM_COL32(210, 210, 220, 255));
            }
            else
            {
                ImGui::Spacing();
                ImGui::TextUnformatted("GPU timestamps not available yet");
            }
        }
        ImGui::End();

        const ImVec2 shadowWindowSize(300.0f, 348.0f);
        ImVec2 shadowWindowPos(
            viewport->WorkPos.x + viewport->WorkSize.x - shadowWindowSize.x - pad,
            viewport->WorkPos.y + pad
        );
        ImGui::SetNextWindowPos(shadowWindowPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(shadowWindowSize, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Sun View"))
        {
            ImGui::Text("Cascade 0");
            ImGui::Separator();
            ImGui::Image(m_renderer.GetShadowDebugTexture(0), ImVec2(268.0f, 128.0f));
            ImGui::Separator();
            ImGui::Text("Cascade 1");
            ImGui::Separator();
            ImGui::Image(m_renderer.GetShadowDebugTexture(1), ImVec2(268.0f, 128.0f));
        }
        ImGui::End();

        ImVec2 shadowControlsPos(
            viewport->WorkPos.x + viewport->WorkSize.x - shadowWindowSize.x - pad,
            viewport->WorkPos.y + shadowWindowSize.y + pad * 2.0f
        );
        ImGui::SetNextWindowPos(shadowControlsPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 132.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Shadows"))
        {
            static constexpr int kShadowSizes[] = {512, 1024, 2048, 4096};
            int currentSizeIndex = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (m_shadowMapSize == static_cast<std::uint32_t>(kShadowSizes[i]))
                {
                    currentSizeIndex = i;
                    break;
                }
            }

            const char* shadowSizeLabels[] = {"512", "1024", "2048", "4096"};
            if (ImGui::Combo("Map size", &currentSizeIndex, shadowSizeLabels, 4))
            {
                m_shadowMapSize = static_cast<std::uint32_t>(kShadowSizes[currentSizeIndex]);
                m_reloadSceneRequested = true;
                debugSettingsChanged = true;
            }

            debugSettingsChanged |= ImGui::Checkbox("Blur (3x3 PCF)", &m_shadowBlur);
            debugSettingsChanged |= ImGui::SliderFloat("Main draw dist", &m_mainDrawDistance, 24.0f, 320.0f, "%.0f");
            debugSettingsChanged |= ImGui::SliderFloat("Shadow dist", &m_shadowDrawDistance, 24.0f, 320.0f, "%.0f");
            ImGui::Text("Current: %u x %u", m_shadowMapSize, m_shadowMapSize);
        }
        ImGui::End();

        ImVec2 spotControlsPos(
            viewport->WorkPos.x + viewport->WorkSize.x - shadowWindowSize.x - pad,
            viewport->WorkPos.y + shadowWindowSize.y + 132.0f + pad * 3.0f
        );
        ImGui::SetNextWindowPos(spotControlsPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 252.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Spotlights"))
        {
            debugSettingsChanged |= ImGui::SliderFloat("Intensity", &m_spotLightIntensityScale, 0.0f, 4.0f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Range", &m_spotLightRangeScale, 0.25f, 2.5f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Inner angle", &m_spotLightInnerAngleDegrees, 4.0f, 40.0f, "%.1f deg");
            debugSettingsChanged |= ImGui::SliderFloat("Outer angle", &m_spotLightOuterAngleDegrees, 8.0f, 60.0f, "%.1f deg");
            int maxActive = static_cast<int>(m_spotLightMaxActive);
            debugSettingsChanged |= ImGui::SliderInt("Max active", &maxActive, 0, static_cast<int>(kMaxSceneSpotLights));
            m_spotLightMaxActive = static_cast<std::uint32_t>(maxActive);
            debugSettingsChanged |= ImGui::SliderFloat("Activation dist", &m_spotLightActivationDistance, 2.0f, 96.0f, "%.1f");
            debugSettingsChanged |= ImGui::SliderFloat("Forward offset", &m_spotLightActivationForwardOffset, -16.0f, 48.0f, "%.1f");
            int shadowedMaxActive = static_cast<int>(m_shadowedSpotLightMaxActive);
            debugSettingsChanged |= ImGui::SliderInt("Shadowed active", &shadowedMaxActive, 0, static_cast<int>(kMaxShadowedSpotLights));
            m_shadowedSpotLightMaxActive = static_cast<std::uint32_t>(shadowedMaxActive);
            debugSettingsChanged |= ImGui::SliderFloat("Shadowed dist", &m_shadowedSpotLightActivationDistance, 2.0f, 64.0f, "%.1f");
            debugSettingsChanged |= ImGui::SliderFloat("Shadowed fwd", &m_shadowedSpotLightActivationForwardOffset, -16.0f, 32.0f, "%.1f");
            debugSettingsChanged |= ImGui::DragFloat3("Source offset", &m_spotLightSourceOffset.x, 0.01f, -2.0f, 2.0f, "%.3f");
            debugSettingsChanged |= ImGui::Checkbox("Show light proxies", &m_drawLightProxies);
            debugSettingsChanged |= ImGui::Checkbox("Show activation", &m_debugDrawActivationVolumes);
            debugSettingsChanged |= ImGui::Checkbox("Show light gizmos", &m_debugDrawSceneLightGizmos);
            debugSettingsChanged |= ImGui::Checkbox("Show direction lines", &m_debugDrawLightDirections);
            debugSettingsChanged |= ImGui::Checkbox("Show light volumes", &m_debugDrawLightVolumes);
            debugSettingsChanged |= ImGui::Checkbox("Show labels", &m_debugDrawLightLabels);
            debugSettingsChanged |= ImGui::Checkbox("Show vehicle volumes", &m_debugDrawVehicleVolumes);
            if (m_spotLightOuterAngleDegrees < m_spotLightInnerAngleDegrees)
            {
                m_spotLightOuterAngleDegrees = m_spotLightInnerAngleDegrees;
                debugSettingsChanged = true;
            }
            ImGui::Text("Scene spots: %zu", m_scene.spotLights.size());
            ImGui::Text("GPU cap: %u", static_cast<std::uint32_t>(kMaxSceneSpotLights));
            ImGui::Text("Active budget: %u", m_spotLightMaxActive);
            ImGui::Text("Shadowed cap: %u", static_cast<std::uint32_t>(kMaxShadowedSpotLights));
            if (m_sceneKind == SceneKind::ShadowTest)
            {
                ImGui::Separator();
                ImGui::Text("Left click geometry to aim");
                if (m_shadowTestSpotTargetValid)
                {
                    ImGui::Text(
                        "Target %.2f %.2f %.2f",
                        m_shadowTestSpotTargetWorld.x,
                        m_shadowTestSpotTargetWorld.y,
                        m_shadowTestSpotTargetWorld.z
                    );
                    ImGui::Text(
                        "Offset %.2f %.2f %.2f",
                        m_shadowTestSpotTargetOffset.x,
                        m_shadowTestSpotTargetOffset.y,
                        m_shadowTestSpotTargetOffset.z
                    );
                }
            }
        }
        ImGui::End();

        ImVec2 paintPos(
            viewport->WorkPos.x + pad,
            viewport->WorkPos.y + 628.0f
        );
        ImGui::SetNextWindowPos(paintPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 180.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Paint Balls"))
        {
            ImGui::Text("Active: %u / %u", m_paintBalls.ActiveCount(), PaintBallSettings::kMaxBalls);
            if (ImGui::Button("Reset paint balls"))
            {
                m_paintBalls.Reset();
            }
            int bounceLimit = static_cast<int>(m_paintBallSettings.bounceLimit);
            debugSettingsChanged |= ImGui::SliderInt("Bounce limit", &bounceLimit, 0, 12);
            m_paintBallSettings.bounceLimit = static_cast<std::uint32_t>(bounceLimit);
            debugSettingsChanged |= ImGui::ColorEdit3("Paint color", &m_paintBallSettings.baseColor.x);
            debugSettingsChanged |= ImGui::Checkbox("Cycle color on shoot", &m_paintBallSettings.cycleColorOnShoot);
            ImGui::Separator();
            ImGui::TextUnformatted("Left click fires");
            ImGui::TextUnformatted("Shift + Left click places test-scene tools");
        }
        ImGui::End();

        if (m_sceneKind == SceneKind::VehicleLightTest)
        {
            ImVec2 vehiclePos(
                viewport->WorkPos.x + viewport->WorkSize.x - shadowWindowSize.x - pad,
                viewport->WorkPos.y + shadowWindowSize.y + 132.0f + 252.0f + pad * 4.0f
            );
            ImGui::SetNextWindowPos(vehiclePos, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Vehicle Lights"))
            {
                int activeVehicle = FindActiveVehicleLightIndex();
                ImGui::Text("Active vehicle: %d", activeVehicle);
                if (activeVehicle >= 0)
                {
                    const auto& item = m_scene.vehicleLightTestItems[static_cast<std::size_t>(activeVehicle)];
                    VehicleLightRig& rig = m_vehicleLightRigs[item.assetPath];
                    int slot = static_cast<int>(m_vehicleLightSlot);
                    const char* slotNames[] = {"Head A", "Head B", "Rear A", "Rear B"};
                    debugSettingsChanged |= ImGui::Combo("Edit slot", &slot, slotNames, 4);
                    m_vehicleLightSlot = static_cast<VehicleLightSlot>(slot);
                    if (m_vehicleLightSlot == VehicleLightSlot::HeadA || m_vehicleLightSlot == VehicleLightSlot::HeadB)
                    {
                        VehicleFrontLightConfig& light = m_vehicleLightSlot == VehicleLightSlot::HeadA ? rig.headA : rig.headB;
                        debugSettingsChanged |= ImGui::DragFloat3("Offset", &light.offset.x, 0.01f, -4.0f, 4.0f, "%.3f");
                        debugSettingsChanged |= ImGui::SliderFloat("Yaw", &light.yawDegrees, -180.0f, 180.0f, "%.1f");
                        debugSettingsChanged |= ImGui::SliderFloat("Pitch", &light.pitchDegrees, -89.0f, 89.0f, "%.1f");
                        debugSettingsChanged |= ImGui::SliderFloat("Range", &light.range, 1.0f, 24.0f, "%.1f");
                    }
                    else
                    {
                        VehicleRearLightConfig& light = m_vehicleLightSlot == VehicleLightSlot::RearA ? rig.rearA : rig.rearB;
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

        if (debugSettingsChanged)
        {
            SaveDebugSettings();
            SaveVehicleLightRigs();
        }
    }

    DrawLightDebugOverlay(*this);
    ImGui::Render();
}
