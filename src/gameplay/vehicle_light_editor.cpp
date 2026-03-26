#include "app.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::filesystem::path VehicleLightRigPath()
{
    return std::filesystem::path(".cache") / "vehicle_light_rigs.txt";
}

VehicleLightRig MakeDefaultRig()
{
    return {};
}

bool ParseFloat(std::string_view text, std::size_t& cursor, float& outValue)
{
    while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t'))
    {
        ++cursor;
    }
    std::size_t end = cursor;
    while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != '\n')
    {
        ++end;
    }
    if (end == cursor)
    {
        return false;
    }
    try
    {
        outValue = std::stof(std::string(text.substr(cursor, end - cursor)));
        cursor = end;
        return true;
    }
    catch (...)
    {
        return false;
    }
}
}

int App::FindActiveVehicleLightIndex() const
{
    const auto& core = m_state.core;
    const auto& lighting = m_state.lighting;
    if (lighting.sceneKind != SceneKind::VehicleLightTest)
    {
        return -1;
    }

    int bestIndex = -1;
    float bestDist2 = 1e30f;
    for (std::size_t i = 0; i < core.scene.vehicleLightTestItems.size(); ++i)
    {
        const SceneData::VehicleLightTestItem& item = core.scene.vehicleLightTestItems[i];
        Vec3 delta = Vec3Sub(core.player.position, item.origin);
        float dist2 = Vec3Dot(delta, delta);
        if (dist2 <= item.selectionRadius * item.selectionRadius && dist2 < bestDist2)
        {
            bestIndex = static_cast<int>(i);
            bestDist2 = dist2;
        }
    }
    return bestIndex;
}

void App::TryPlaceVehicleLight(int mouseX, int mouseY)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    auto& vehicle = m_state.vehicleLights;
    if (lighting.sceneKind != SceneKind::VehicleLightTest)
    {
        return;
    }

    int activeIndex = FindActiveVehicleLightIndex();
    if (activeIndex < 0 || runtime.windowWidth == 0 || runtime.windowHeight == 0)
    {
        return;
    }

    float pixelX = static_cast<float>(mouseX) + 0.5f;
    float pixelY = static_cast<float>(mouseY) + 0.5f;
    float ndcX = pixelX / static_cast<float>(runtime.windowWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - pixelY / static_cast<float>(runtime.windowHeight) * 2.0f;
    float aspect = static_cast<float>(runtime.windowWidth) / static_cast<float>(runtime.windowHeight);

    constexpr float kFovYRadians = 1.0471975512f;
    float tanHalfFov = std::tan(kFovYRadians * 0.5f);
    Vec3 forward = CameraForward(core.camera);
    Vec3 right = CameraRight(core.camera);
    Vec3 up = Vec3Normalize(Vec3Cross(right, forward));
    Vec3 rayDir = Vec3Normalize(Vec3Add(
        forward,
        Vec3Add(
            Vec3Scale(right, ndcX * aspect * tanHalfFov),
            Vec3Scale(up, ndcY * tanHalfFov)
        )
    ));

    TriangleMeshCollider::RayHit hit = core.worldCollider.Raycast(core.camera.position, rayDir, 500.0f);
    if (!hit.hit)
    {
        return;
    }

    const SceneData::VehicleLightTestItem& item = core.scene.vehicleLightTestItems[static_cast<std::size_t>(activeIndex)];
    VehicleLightRig& rig = vehicle.rigs[item.assetPath];
    Vec3 local = Vec3Scale(Vec3Sub(hit.position, item.origin), 1.0f / std::max(item.scale, 0.0001f));
    switch (vehicle.slot)
    {
    case VehicleLightSlot::HeadA: rig.headA.offset = local; break;
    case VehicleLightSlot::HeadB: rig.headB.offset = local; break;
    case VehicleLightSlot::RearA: rig.rearA.offset = local; break;
    case VehicleLightSlot::RearB: rig.rearB.offset = local; break;
    }
    SaveVehicleLightRigs();
}

void App::LoadVehicleLightRigs()
{
    auto& rigs = m_state.vehicleLights.rigs;
    rigs.clear();
    std::ifstream in(VehicleLightRigPath());
    if (!in)
    {
        return;
    }

    std::string line;
    std::string currentAsset;
    while (std::getline(in, line))
    {
        if (line.empty())
        {
            continue;
        }
        if (line.rfind("asset ", 0) == 0)
        {
            currentAsset = line.substr(6);
            rigs.emplace(currentAsset, MakeDefaultRig());
            continue;
        }
        if (currentAsset.empty())
        {
            continue;
        }

        VehicleLightRig& rig = rigs[currentAsset];
        std::string_view view(line);
        auto parseTriple = [&](Vec3& value) {
            std::size_t cursor = line.find(' ');
            if (cursor == std::string::npos) return;
            ++cursor;
            ParseFloat(view, cursor, value.x);
            ParseFloat(view, cursor, value.y);
            ParseFloat(view, cursor, value.z);
        };
        auto parsePair = [&](float& a, float& b) {
            std::size_t cursor = line.find(' ');
            if (cursor == std::string::npos) return;
            ++cursor;
            ParseFloat(view, cursor, a);
            ParseFloat(view, cursor, b);
        };

        if (line.rfind("headA_offset ", 0) == 0) parseTriple(rig.headA.offset);
        else if (line.rfind("headB_offset ", 0) == 0) parseTriple(rig.headB.offset);
        else if (line.rfind("rearA_offset ", 0) == 0) parseTriple(rig.rearA.offset);
        else if (line.rfind("rearB_offset ", 0) == 0) parseTriple(rig.rearB.offset);
        else if (line.rfind("headA_angles ", 0) == 0) parsePair(rig.headA.yawDegrees, rig.headA.pitchDegrees);
        else if (line.rfind("headB_angles ", 0) == 0) parsePair(rig.headB.yawDegrees, rig.headB.pitchDegrees);
        else if (line.rfind("headA_range ", 0) == 0) { std::size_t c = 12; ParseFloat(view, c, rig.headA.range); }
        else if (line.rfind("headB_range ", 0) == 0) { std::size_t c = 12; ParseFloat(view, c, rig.headB.range); }
        else if (line.rfind("rearA_range_intensity ", 0) == 0) { std::size_t c = 22; ParseFloat(view, c, rig.rearA.range); ParseFloat(view, c, rig.rearA.intensity); }
        else if (line.rfind("rearB_range_intensity ", 0) == 0) { std::size_t c = 22; ParseFloat(view, c, rig.rearB.range); ParseFloat(view, c, rig.rearB.intensity); }
    }
}

void App::SaveVehicleLightRigs() const
{
    std::filesystem::create_directories(VehicleLightRigPath().parent_path());
    std::ofstream out(VehicleLightRigPath(), std::ios::trunc);
    if (!out)
    {
        return;
    }

    for (const auto& [asset, rig] : m_state.vehicleLights.rigs)
    {
        out << "asset " << asset << "\n";
        out << "headA_offset " << rig.headA.offset.x << " " << rig.headA.offset.y << " " << rig.headA.offset.z << "\n";
        out << "headB_offset " << rig.headB.offset.x << " " << rig.headB.offset.y << " " << rig.headB.offset.z << "\n";
        out << "rearA_offset " << rig.rearA.offset.x << " " << rig.rearA.offset.y << " " << rig.rearA.offset.z << "\n";
        out << "rearB_offset " << rig.rearB.offset.x << " " << rig.rearB.offset.y << " " << rig.rearB.offset.z << "\n";
        out << "headA_angles " << rig.headA.yawDegrees << " " << rig.headA.pitchDegrees << "\n";
        out << "headB_angles " << rig.headB.yawDegrees << " " << rig.headB.pitchDegrees << "\n";
        out << "headA_range " << rig.headA.range << "\n";
        out << "headB_range " << rig.headB.range << "\n";
        out << "rearA_range_intensity " << rig.rearA.range << " " << rig.rearA.intensity << "\n";
        out << "rearB_range_intensity " << rig.rearB.range << " " << rig.rearB.intensity << "\n";
    }
}
