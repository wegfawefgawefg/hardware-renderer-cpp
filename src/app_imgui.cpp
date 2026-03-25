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
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Lighting"))
        {
            int sceneKind = static_cast<int>(m_sceneKind);
            const char* sceneNames[] = {"City", "Shadow Test"};
            if (ImGui::Combo("Scene", &sceneKind, sceneNames, 2))
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
            ImGui::SliderFloat("Cascade split", &m_shadowCascadeSplit, 4.0f, 96.0f, "%.1f");
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

            ImGui::Checkbox("Blur (3x3 PCF)", &m_shadowBlur);
            ImGui::Text("Current: %u x %u", m_shadowMapSize, m_shadowMapSize);
        }
        ImGui::End();
    }

    ImGui::Render();
}
