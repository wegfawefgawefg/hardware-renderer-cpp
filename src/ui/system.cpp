#include "app.h"

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include <cfloat>
void App::InitializeImGui()
{
    auto& core = m_state.core;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL3_InitForVulkan(m_window);
    core.renderer.InitializeImGuiBackend();
}

void App::ShutdownImGui()
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    m_state.core.renderer.ShutdownImGuiBackend();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void App::ProcessImGuiEvent(const SDL_Event& event)
{
    auto& runtime = m_state.runtime;
    if (ImGui::GetCurrentContext() != nullptr)
    {
        if (runtime.mouseCaptured)
        {
            switch (event.type)
            {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_TEXT_INPUT:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_WHEEL:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                return;
            default:
                break;
            }
        }
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void App::BuildImGui()
{
    auto& runtime = m_state.runtime;
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (runtime.mouseCaptured)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        for (bool& down : io.MouseDown)
        {
            down = false;
        }
        io.MouseWheel = 0.0f;
        io.MouseWheelH = 0.0f;
        io.WantCaptureMouse = false;
        io.WantCaptureKeyboard = false;
    }

    if (runtime.showImGui)
    {
        bool debugSettingsChanged = false;
        BuildLightingWindow(debugSettingsChanged);
        BuildProfilerWindow();
        BuildSunViewWindow();
        BuildShadowsWindow(debugSettingsChanged);
        BuildSpotlightsWindow(debugSettingsChanged);
        BuildPaintBallsWindow(debugSettingsChanged);
        BuildSurfaceMasksWindow(debugSettingsChanged);
        BuildFractureWindow(debugSettingsChanged);
        BuildVehicleLightsWindow(debugSettingsChanged);

        if (debugSettingsChanged)
        {
            SaveDebugSettings();
            SaveVehicleLightRigs();
        }
    }

    DrawLightDebugOverlay();
    ImGui::Render();
}
