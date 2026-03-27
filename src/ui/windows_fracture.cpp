#include "app.h"

#include "imgui.h"

void App::BuildFractureWindow(bool& debugSettingsChanged)
{
    auto& lighting = m_state.lighting;
    auto& fracture = m_state.fracture;
    auto& core = m_state.core;
    if (lighting.sceneKind != SceneKind::FractureTest)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(16.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Fracture"))
    {
        ImGui::Text("Chunks: %u", core.fracture.ActiveChunkCount());
        ImGui::Text("Debris: %u", core.fracture.ActiveDebrisCount());
        if (ImGui::Button("Reset building"))
        {
            core.fracture.Reset();
        }
        debugSettingsChanged |= ImGui::SliderFloat("Chunk half", &fracture.settings.chunkHalfExtent, 0.45f, 1.20f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Blast radius", &fracture.settings.blastRadius, 0.6f, 3.5f, "%.2f");
        debugSettingsChanged |= ImGui::SliderFloat("Fire rate", &fracture.settings.fireRate, 1.0f, 20.0f, "%.1f / s");
        debugSettingsChanged |= ImGui::SliderFloat("Debris speed", &fracture.settings.debrisSpeed, 2.0f, 18.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Debris gravity", &fracture.settings.debrisGravity, 4.0f, 32.0f, "%.1f");
        debugSettingsChanged |= ImGui::SliderFloat("Debris life", &fracture.settings.debrisLifetime, 0.5f, 8.0f, "%.2f");
        int debrisPerBlast = static_cast<int>(fracture.settings.maxDebrisPerBlast);
        if (ImGui::SliderInt("Debris per blast", &debrisPerBlast, 0, 96))
        {
            fracture.settings.maxDebrisPerBlast = static_cast<std::uint32_t>(debrisPerBlast);
            debugSettingsChanged = true;
        }
        if (ImGui::Button("Rebuild chunks"))
        {
            core.fracture.InitializeFromAsset(
                core.assetRegistry,
                "kenney/kenney_city-kit-commercial_2.1/Models/FBX format/building-skyscraper-b.fbx",
                fracture.settings
            );
        }
        ImGui::TextUnformatted("Left click fractures the first chunk hit by the camera ray.");
        ImGui::TextUnformatted("This scene uses cheap cube chunks and debris on purpose.");
    }
    ImGui::End();
}
