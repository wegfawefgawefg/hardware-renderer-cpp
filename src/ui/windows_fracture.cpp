#include "app.h"

#include <algorithm>

#include "imgui.h"

void App::BuildFractureWindow(bool& debugSettingsChanged)
{
    auto& lighting = m_state.lighting;
    auto& fracture = m_state.fracture;
    if (lighting.sceneKind != SceneKind::FractureTest)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(16.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 220.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Fracture"))
    {
        ImGui::Text("Scene tris: %u", m_state.runtime.sceneTriangleCount);
        ImGui::Checkbox("Show tri wireframe", &fracture.showWireframe);
        static constexpr const char* kModeNames[] = {
            "Dent",
            "Punch",
            "Damage Decal",
        };
        int mode = static_cast<int>(fracture.settings.mesh.mode);
        if (ImGui::Combo("Damage mode", &mode, kModeNames, IM_ARRAYSIZE(kModeNames)))
        {
            fracture.settings.mesh.mode = static_cast<damage::Mode>(mode);
            debugSettingsChanged = true;
        }
        if (ImGui::Button("Reset scene"))
        {
            m_state.runtime.reloadSceneRequested = true;
        }
        ImGui::SeparatorText("Damage");
        debugSettingsChanged |= ImGui::SliderFloat("Radius", &fracture.settings.mesh.radius, 0.05f, 4.0f, "%.2f");
        if (fracture.settings.mesh.mode == damage::Mode::Dent)
        {
            debugSettingsChanged |= ImGui::Checkbox("Depth = radius", &fracture.dentDepthMatchesRadius);
            if (fracture.dentDepthMatchesRadius)
            {
                fracture.settings.mesh.punchDepth = fracture.settings.mesh.radius;
            }
            else
            {
                debugSettingsChanged |= ImGui::SliderFloat("Dent depth", &fracture.settings.mesh.punchDepth, 0.2f, 6.0f, "%.2f");
            }
        }
        else if (fracture.settings.mesh.mode == damage::Mode::Punch)
        {
            debugSettingsChanged |= ImGui::SliderFloat("Inner size", &fracture.settings.mesh.punchInnerRadiusScale, 0.10f, 0.95f, "%.2f");
            debugSettingsChanged |= ImGui::SliderFloat("Core size", &fracture.settings.mesh.punchCoreRadiusScale, 0.02f, 0.90f, "%.2f");
            fracture.settings.mesh.punchCoreRadiusScale = std::min(
                fracture.settings.mesh.punchCoreRadiusScale,
                fracture.settings.mesh.punchInnerRadiusScale - 0.01f);
            fracture.settings.mesh.punchCoreRadiusScale = std::max(fracture.settings.mesh.punchCoreRadiusScale, 0.02f);
        }
        else if (fracture.settings.mesh.mode == damage::Mode::DamageDecal)
        {
            ImGui::TextUnformatted("Radius sets decal size.");
            if (fracture.damageDecalTemplateCount > 0)
            {
                std::uint32_t selectedIndex = std::min(
                    fracture.selectedDamageDecalTemplate,
                    fracture.damageDecalTemplateCount - 1u);
                const char* preview = fracture.damageDecalTemplateNames[selectedIndex].c_str();
                if (ImGui::BeginCombo("Decal template", preview))
                {
                    for (std::uint32_t i = 0; i < fracture.damageDecalTemplateCount; ++i)
                    {
                        bool selected = (i == selectedIndex);
                        if (ImGui::Selectable(fracture.damageDecalTemplateNames[i].c_str(), selected))
                        {
                            fracture.selectedDamageDecalTemplate = i;
                            debugSettingsChanged = true;
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            debugSettingsChanged |= ImGui::SliderFloat("Roll variance", &fracture.settings.mesh.decalRollVarianceDegrees, 0.0f, 180.0f, "%.1f deg");
            debugSettingsChanged |= ImGui::SliderFloat("Shot spread", &fracture.settings.mesh.decalSpreadDegrees, 0.0f, 20.0f, "%.1f deg");
            int burstCount = static_cast<int>(fracture.settings.mesh.decalBurstCount);
            debugSettingsChanged |= ImGui::SliderInt("Shotgun count", &burstCount, 1, 16);
            fracture.settings.mesh.decalBurstCount = static_cast<std::uint32_t>(burstCount);
            debugSettingsChanged |= ImGui::SliderFloat("Fire rate", &fracture.settings.fireRate, 1.0f, 60.0f, "%.1f / s");
        }
        ImGui::SeparatorText("Generated Prism");
        ImGui::SliderFloat3("Half extents", &fracture.prism.halfExtents.x, 1.0f, 12.0f, "%.2f");
        int segX = static_cast<int>(fracture.prism.segX);
        int segY = static_cast<int>(fracture.prism.segY);
        int segZ = static_cast<int>(fracture.prism.segZ);
        ImGui::SliderInt("Face div X", &segX, 1, 40);
        ImGui::SliderInt("Face div Y", &segY, 1, 60);
        ImGui::SliderInt("Face div Z", &segZ, 1, 40);
        fracture.prism.segX = static_cast<std::uint32_t>(segX);
        fracture.prism.segY = static_cast<std::uint32_t>(segY);
        fracture.prism.segZ = static_cast<std::uint32_t>(segZ);
        if (ImGui::Button("Regenerate prism"))
        {
            m_state.runtime.reloadSceneRequested = true;
        }
        ImGui::TextUnformatted("Left click dents, punches, or stamps decals on the generated prism.");
    }
    ImGui::End();
}
