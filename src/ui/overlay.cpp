#include "app.h"

#include "imgui.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "text/text_system.h"

namespace
{
void AppendOverlayLabel(App& app, float x, float y, ImU32 color, const char* text)
{
    if (text == nullptr)
    {
        return;
    }
    Vec4 textColor = Vec4Make(
        static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f,
        static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f,
        static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f,
        static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xff) / 255.0f
    );
    text::DrawText(
        app.m_state.text,
        x,
        y,
        18.0f,
        textColor,
        text
    );
}

Vec3 RotateYOffset(Vec3 v, float yawDegrees)
{
    float radians = DegreesToRadians(yawDegrees);
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Make(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

bool ProjectWorldToScreen(const App& app, Vec3 world, ImVec2& outScreen)
{
    if (app.m_state.runtime.windowWidth == 0 || app.m_state.runtime.windowHeight == 0)
    {
        return false;
    }

    Mat4 view = CameraViewMatrix(app.m_state.core.camera);
    Mat4 proj = Mat4Perspective(
        DegreesToRadians(60.0f),
        static_cast<float>(app.m_state.runtime.windowWidth) / static_cast<float>(app.m_state.runtime.windowHeight),
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

    outScreen.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(app.m_state.runtime.windowWidth);
    outScreen.y = (ndcY * 0.5f + 0.5f) * static_cast<float>(app.m_state.runtime.windowHeight);
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

    std::uint32_t shadowedBudget = std::min(app.m_state.lighting.shadowedSpotLightMaxActive, kMaxShadowedSpotLights);
    std::uint32_t unshadowedBudget = std::min(app.m_state.lighting.spotLightMaxActive, kMaxSceneSpotLights);
    Vec3 forward = CameraForward(app.m_state.core.camera);
    Vec3 shadowedCenter = Vec3Add(app.m_state.core.camera.position, Vec3Scale(forward, app.m_state.lighting.shadowedSpotLightActivationForwardOffset));
    Vec3 unshadowedCenter = Vec3Add(app.m_state.core.camera.position, Vec3Scale(forward, app.m_state.lighting.spotLightActivationForwardOffset));
    float shadowedDist2Max = app.m_state.lighting.shadowedSpotLightActivationDistance * app.m_state.lighting.shadowedSpotLightActivationDistance;
    float unshadowedDist2Max = app.m_state.lighting.spotLightActivationDistance * app.m_state.lighting.spotLightActivationDistance;

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

    for (std::uint32_t i = 0; i < app.m_state.core.scene.spotLights.size(); ++i)
    {
        const SpotLightData& light = app.m_state.core.scene.spotLights[i];
        Vec3 lightPos = Vec3Add(light.position, RotateYOffset(app.m_state.lighting.spotLightSourceOffset, light.yawDegrees));
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

    std::vector<bool> used(app.m_state.core.scene.spotLights.size(), false);
    for (std::uint32_t i = 0; i < shadowedBudget; ++i)
    {
        if (shadowed[i].index < app.m_state.core.scene.spotLights.size())
        {
            result.shadowed.push_back(shadowed[i].index);
            used[shadowed[i].index] = true;
        }
    }
    for (std::uint32_t i = 0; i < unshadowedBudget; ++i)
    {
        if (unshadowed[i].index < app.m_state.core.scene.spotLights.size() && !used[unshadowed[i].index])
        {
            result.unshadowed.push_back(unshadowed[i].index);
        }
    }
    return result;
}
}

void App::DrawLightDebugOverlay()
{
    auto& core = m_state.core;
    auto& lighting = m_state.lighting;
    auto& vehicle = m_state.vehicleLights;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (lighting.sceneKind == SceneKind::VehicleLightTest)
    {
        int activeVehicle = FindActiveVehicleLightIndex();
        if (activeVehicle >= 0)
        {
            const auto& item = core.scene.vehicleLightTestItems[static_cast<std::size_t>(activeVehicle)];
            VehicleLightRig rig = {};
            if (auto found = vehicle.rigs.find(item.assetPath); found != vehicle.rigs.end())
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
                ImU32 color;
            };
            std::array<LightViz, 4> lights = {{
                {"HA", worldPoint(rig.headA.offset), IM_COL32(255, 244, 160, 255)},
                {"HB", worldPoint(rig.headB.offset), IM_COL32(255, 220, 120, 255)},
                {"RA", worldPoint(rig.rearA.offset), IM_COL32(255, 90, 90, 255)},
                {"RB", worldPoint(rig.rearB.offset), IM_COL32(255, 50, 50, 255)},
            }};

            for (const LightViz& light : lights)
            {
                ImVec2 sourceScreen{};
                if (!ProjectWorldToScreen(*this, light.position, sourceScreen))
                {
                    continue;
                }
                drawList->AddCircleFilled(sourceScreen, 4.0f, light.color, 12);
                if (lighting.debugDrawLightLabels)
                {
                    AppendOverlayLabel(*this, sourceScreen.x + 8.0f, sourceScreen.y - 10.0f, light.color, light.label);
                }
            }
        }
        return;
    }

    if (core.scene.spotLights.empty() || !lighting.debugDrawLightLabels)
    {
        return;
    }

    DebugSpotSelection selection = CollectDebugSpotSelection(*this);
    std::vector<std::uint8_t> state(core.scene.spotLights.size(), 0);
    for (std::uint32_t index : selection.unshadowed) state[index] = 1;
    for (std::uint32_t index : selection.shadowed) state[index] = 2;

    for (std::uint32_t i = 0; i < core.scene.spotLights.size(); ++i)
    {
        const SpotLightData& light = core.scene.spotLights[i];
        Vec3 source = Vec3Add(light.position, RotateYOffset(lighting.spotLightSourceOffset, light.yawDegrees));
        ImU32 color = IM_COL32(110, 110, 110, 220);
        if (state[i] == 1) color = IM_COL32(0, 255, 255, 255);
        if (state[i] == 2) color = IM_COL32(255, 0, 255, 255);
        if (state[i] == 0)
        {
            continue;
        }

        ImVec2 sourceScreen{};
        if (!ProjectWorldToScreen(*this, source, sourceScreen))
        {
            continue;
        }

        const char* type = state[i] == 2 ? "SH" : (state[i] == 1 ? "SP" : "IN");
        char label[64];
        std::snprintf(label, sizeof(label), "%s %u", type, i);
        AppendOverlayLabel(*this, sourceScreen.x + 8.0f, sourceScreen.y - 8.0f, color, label);
    }
}
