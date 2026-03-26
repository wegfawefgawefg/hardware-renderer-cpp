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

float ProjectSphereRadius(const App& app, Vec3 center, float radius)
{
    ImVec2 centerScreen{};
    ImVec2 edgeScreen{};
    Vec3 right = CameraRight(app.m_camera);
    if (!ProjectWorldToScreen(app, center, centerScreen) ||
        !ProjectWorldToScreen(app, Vec3Add(center, Vec3Scale(right, radius)), edgeScreen))
    {
        return 0.0f;
    }
    float dx = edgeScreen.x - centerScreen.x;
    float dy = edgeScreen.y - centerScreen.y;
    return std::sqrt(dx * dx + dy * dy);
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
    if (app.m_scene.spotLights.empty())
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    DebugSpotSelection selection = CollectDebugSpotSelection(app);
    std::vector<std::uint8_t> state(app.m_scene.spotLights.size(), 0);
    for (std::uint32_t index : selection.unshadowed) state[index] = 1;
    for (std::uint32_t index : selection.shadowed) state[index] = 2;

    if (app.m_debugDrawActivationVolumes)
    {
        Vec3 forward = CameraForward(app.m_camera);
        Vec3 centerA = Vec3Add(app.m_camera.position, Vec3Scale(forward, app.m_spotLightActivationForwardOffset));
        Vec3 centerB = Vec3Add(app.m_camera.position, Vec3Scale(forward, app.m_shadowedSpotLightActivationForwardOffset));
        ImVec2 screenCenter{};
        if (ProjectWorldToScreen(app, centerA, screenCenter))
        {
            float radius = ProjectSphereRadius(app, centerA, app.m_spotLightActivationDistance);
            if (radius > 1.0f)
            {
                drawList->AddCircle(screenCenter, radius, IM_COL32(0, 255, 255, 160), 64, 1.5f);
                drawList->AddCircle(screenCenter, std::max(radius - 2.0f, 1.0f), IM_COL32(0, 255, 255, 80), 64, 1.0f);
                drawList->AddCircleFilled(screenCenter, 3.0f, IM_COL32(0, 255, 255, 200), 12);
                drawList->AddText(ImVec2(screenCenter.x + 8.0f, screenCenter.y - 10.0f), IM_COL32(0, 255, 255, 220), "ACT");
            }
        }
        if (ProjectWorldToScreen(app, centerB, screenCenter))
        {
            float radius = ProjectSphereRadius(app, centerB, app.m_shadowedSpotLightActivationDistance);
            if (radius > 1.0f)
            {
                drawList->AddCircle(screenCenter, radius, IM_COL32(255, 0, 255, 180), 64, 1.75f);
                drawList->AddCircle(screenCenter, std::max(radius - 2.0f, 1.0f), IM_COL32(255, 0, 255, 96), 64, 1.0f);
                drawList->AddCircleFilled(screenCenter, 3.0f, IM_COL32(255, 0, 255, 220), 12);
                drawList->AddText(ImVec2(screenCenter.x + 8.0f, screenCenter.y - 10.0f), IM_COL32(255, 0, 255, 220), "SHD");
            }
        }
    }

    if (!app.m_debugDrawSceneLightGizmos && !app.m_debugDrawLightDirections && !app.m_debugDrawLightLabels)
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

        ImVec2 sourceScreen{};
        if (!ProjectWorldToScreen(app, source, sourceScreen))
        {
            continue;
        }

        if (app.m_debugDrawLightDirections)
        {
            ImVec2 endScreen{};
            if (ProjectWorldToScreen(app, Vec3Add(source, Vec3Scale(direction, 3.0f)), endScreen))
            {
                drawList->AddLine(sourceScreen, endScreen, color, 2.0f);
            }
        }

        if (app.m_debugDrawSceneLightGizmos)
        {
            drawList->AddCircleFilled(sourceScreen, 4.5f, color, 12);
            drawList->AddCircle(sourceScreen, 6.5f, IM_COL32(0, 0, 0, 200), 12, 1.0f);
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
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Lighting"))
        {
            int sceneKind = static_cast<int>(m_sceneKind);
            const char* sceneNames[] = {"City", "Shadow Test", "Spot Shadow Test"};
            if (ImGui::Combo("Scene", &sceneKind, sceneNames, 3))
            {
                m_sceneKind = static_cast<SceneKind>(sceneKind);
                m_reloadSceneRequested = true;
            }
            float ms = m_smoothedFps > 0.0f ? 1000.0f / m_smoothedFps : 0.0f;
            ImGui::Text("%.2f ms  %.0f fps", ms, m_smoothedFps);
            ImGui::Text("%u ents  %u tris", static_cast<std::uint32_t>(m_scene.entities.size()), m_sceneTriangleCount);
            ImGui::Separator();
            ImGui::Checkbox("Cycle day/night", &m_cycleDayNight);
            ImGui::SliderFloat("Time of day", &m_timeOfDay, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Cycle speed", &m_dayNightSpeed, 0.0f, 0.20f, "%.3f");
            ImGui::SliderFloat("Sun azimuth", &m_sunAzimuthDegrees, -180.0f, 180.0f, "%.1f deg");
            ImGui::SliderFloat("Orbit distance", &m_orbitDistanceScale, 0.5f, 3.0f, "%.2f");
            ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Moon intensity", &m_moonIntensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Ambient intensity", &m_ambientIntensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Point lights", &m_pointLightIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Cascade split", &m_shadowCascadeSplit, 8.0f, 96.0f, "%.1f");
            ImGui::Separator();
            ImGui::Text("RMB capture camera");
            ImGui::Text("Sun  %.1f %.1f %.1f", m_sunWorldPosition.x, m_sunWorldPosition.y, m_sunWorldPosition.z);
            ImGui::Text("Moon %.1f %.1f %.1f", m_moonWorldPosition.x, m_moonWorldPosition.y, m_moonWorldPosition.z);
        }
        ImGui::End();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float pad = 16.0f;
        const ImVec2 shadowWindowSize(300.0f, 348.0f);
        ImVec2 shadowWindowPos(
            viewport->WorkPos.x + viewport->WorkSize.x - shadowWindowSize.x - pad,
            viewport->WorkPos.y + pad
        );
        ImGui::SetNextWindowPos(shadowWindowPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(shadowWindowSize, ImGuiCond_Always);
        if (ImGui::Begin("Sun View", nullptr, ImGuiWindowFlags_NoResize))
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
        ImGui::SetNextWindowPos(shadowControlsPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 132.0f), ImGuiCond_Always);
        if (ImGui::Begin("Shadows", nullptr, ImGuiWindowFlags_NoResize))
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
            }

            debugSettingsChanged |= ImGui::Checkbox("Blur (3x3 PCF)", &m_shadowBlur);
            ImGui::Text("Current: %u x %u", m_shadowMapSize, m_shadowMapSize);
        }
        ImGui::End();

        ImVec2 spotControlsPos(
            viewport->WorkPos.x + viewport->WorkSize.x - shadowWindowSize.x - pad,
            viewport->WorkPos.y + shadowWindowSize.y + 132.0f + pad * 3.0f
        );
        ImGui::SetNextWindowPos(spotControlsPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 252.0f), ImGuiCond_Always);
        if (ImGui::Begin("Spotlights", nullptr, ImGuiWindowFlags_NoResize))
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
            debugSettingsChanged |= ImGui::Checkbox("Show activation", &m_debugDrawActivationVolumes);
            debugSettingsChanged |= ImGui::Checkbox("Show light gizmos", &m_debugDrawSceneLightGizmos);
            debugSettingsChanged |= ImGui::Checkbox("Show direction lines", &m_debugDrawLightDirections);
            debugSettingsChanged |= ImGui::Checkbox("Show labels", &m_debugDrawLightLabels);
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

        if (debugSettingsChanged)
        {
            SaveDebugSettings();
        }
    }

    DrawLightDebugOverlay(*this);
    ImGui::Render();
}
