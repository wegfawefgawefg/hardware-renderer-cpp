#include "app.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

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

    ImGui::Render();
}
