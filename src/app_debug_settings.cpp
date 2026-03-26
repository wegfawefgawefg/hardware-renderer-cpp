#include "app.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
std::filesystem::path DebugSettingsPath()
{
    return std::filesystem::path(".cache") / "lighting_debug.json";
}

bool ExtractFloat(std::string_view text, std::string_view key, float& outValue)
{
    std::size_t keyPos = text.find(key);
    if (keyPos == std::string_view::npos)
    {
        return false;
    }

    std::size_t colonPos = text.find(':', keyPos);
    if (colonPos == std::string_view::npos)
    {
        return false;
    }

    std::size_t valueStart = text.find_first_of("-0123456789.", colonPos + 1);
    if (valueStart == std::string_view::npos)
    {
        return false;
    }

    std::size_t valueEnd = text.find_first_not_of("0123456789+-.eE", valueStart);
    std::string value(text.substr(valueStart, valueEnd - valueStart));
    try
    {
        outValue = std::stof(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ExtractUInt(std::string_view text, std::string_view key, std::uint32_t& outValue)
{
    float value = 0.0f;
    if (!ExtractFloat(text, key, value))
    {
        return false;
    }
    outValue = static_cast<std::uint32_t>(value);
    return true;
}
}

void App::LoadDebugSettings()
{
    std::ifstream in(DebugSettingsPath());
    if (!in)
    {
        return;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();
    float boolValue = 0.0f;

    std::uint32_t sceneKind = static_cast<std::uint32_t>(m_sceneKind);
    if (ExtractUInt(text, "\"scene_kind\"", sceneKind) && sceneKind <= static_cast<std::uint32_t>(SceneKind::VehicleLightTest))
    {
        m_sceneKind = static_cast<SceneKind>(sceneKind);
    }

    ExtractFloat(text, "\"cycle_day_night\"", boolValue);
    m_cycleDayNight = boolValue != 0.0f;
    if (ExtractFloat(text, "\"animate_sun_azimuth\"", boolValue)) m_animateSunAzimuth = boolValue != 0.0f;
    ExtractFloat(text, "\"time_of_day\"", m_timeOfDay);
    ExtractFloat(text, "\"day_night_speed\"", m_dayNightSpeed);
    ExtractFloat(text, "\"sun_azimuth_degrees\"", m_sunAzimuthDegrees);
    ExtractFloat(text, "\"orbit_distance_scale\"", m_orbitDistanceScale);
    ExtractFloat(text, "\"sun_intensity\"", m_sunIntensity);
    ExtractFloat(text, "\"moon_intensity\"", m_moonIntensity);
    ExtractFloat(text, "\"ambient_intensity\"", m_ambientIntensity);
    ExtractFloat(text, "\"point_light_intensity\"", m_pointLightIntensity);
    ExtractFloat(text, "\"shadow_cascade_split\"", m_shadowCascadeSplit);
    ExtractUInt(text, "\"shadow_map_size\"", m_shadowMapSize);
    if (ExtractFloat(text, "\"shadow_blur\"", boolValue)) m_shadowBlur = boolValue != 0.0f;
    ExtractFloat(text, "\"main_draw_distance\"", m_mainDrawDistance);
    ExtractFloat(text, "\"shadow_draw_distance\"", m_shadowDrawDistance);
    ExtractFloat(text, "\"spot_light_intensity_scale\"", m_spotLightIntensityScale);
    ExtractFloat(text, "\"spot_light_range_scale\"", m_spotLightRangeScale);
    ExtractFloat(text, "\"spot_light_inner_angle_degrees\"", m_spotLightInnerAngleDegrees);
    ExtractFloat(text, "\"spot_light_outer_angle_degrees\"", m_spotLightOuterAngleDegrees);
    ExtractUInt(text, "\"spot_light_max_active\"", m_spotLightMaxActive);
    ExtractFloat(text, "\"spot_light_activation_distance\"", m_spotLightActivationDistance);
    ExtractFloat(text, "\"spot_light_activation_forward_offset\"", m_spotLightActivationForwardOffset);
    ExtractUInt(text, "\"shadowed_spot_light_max_active\"", m_shadowedSpotLightMaxActive);
    ExtractFloat(text, "\"shadowed_spot_light_activation_distance\"", m_shadowedSpotLightActivationDistance);
    ExtractFloat(text, "\"shadowed_spot_light_activation_forward_offset\"", m_shadowedSpotLightActivationForwardOffset);
    ExtractFloat(text, "\"source_offset_x\"", m_spotLightSourceOffset.x);
    ExtractFloat(text, "\"source_offset_y\"", m_spotLightSourceOffset.y);
    ExtractFloat(text, "\"source_offset_z\"", m_spotLightSourceOffset.z);
    if (ExtractFloat(text, "\"draw_light_proxies\"", boolValue)) m_drawLightProxies = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_activation_volumes\"", boolValue)) m_debugDrawActivationVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_scene_light_gizmos\"", boolValue)) m_debugDrawSceneLightGizmos = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_directions\"", boolValue)) m_debugDrawLightDirections = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_volumes\"", boolValue)) m_debugDrawLightVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_labels\"", boolValue)) m_debugDrawLightLabels = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_vehicle_volumes\"", boolValue)) m_debugDrawVehicleVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_vehicle_light_ranges\"", boolValue)) m_debugDrawVehicleLightRanges = boolValue != 0.0f;
    ExtractUInt(text, "\"paint_ball_bounce_limit\"", m_paintBallSettings.bounceLimit);
    ExtractFloat(text, "\"paint_ball_color_r\"", m_paintBallSettings.baseColor.x);
    ExtractFloat(text, "\"paint_ball_color_g\"", m_paintBallSettings.baseColor.y);
    ExtractFloat(text, "\"paint_ball_color_b\"", m_paintBallSettings.baseColor.z);
    if (ExtractFloat(text, "\"paint_ball_cycle_color\"", boolValue)) m_paintBallSettings.cycleColorOnShoot = boolValue != 0.0f;
}

void App::SaveDebugSettings() const
{
    std::filesystem::create_directories(DebugSettingsPath().parent_path());
    std::FILE* file = std::fopen(DebugSettingsPath().string().c_str(), "wb");
    if (file == nullptr)
    {
        return;
    }

    std::fprintf(
        file,
        "{\n"
        "  \"scene_kind\": %u,\n"
        "  \"cycle_day_night\": %d,\n"
        "  \"animate_sun_azimuth\": %d,\n"
        "  \"time_of_day\": %.6f,\n"
        "  \"day_night_speed\": %.6f,\n"
        "  \"sun_azimuth_degrees\": %.6f,\n"
        "  \"orbit_distance_scale\": %.6f,\n"
        "  \"sun_intensity\": %.6f,\n"
        "  \"moon_intensity\": %.6f,\n"
        "  \"ambient_intensity\": %.6f,\n"
        "  \"point_light_intensity\": %.6f,\n"
        "  \"shadow_cascade_split\": %.6f,\n"
        "  \"shadow_map_size\": %u,\n"
        "  \"shadow_blur\": %d,\n"
        "  \"main_draw_distance\": %.6f,\n"
        "  \"shadow_draw_distance\": %.6f,\n"
        "  \"spot_light_intensity_scale\": %.6f,\n"
        "  \"spot_light_range_scale\": %.6f,\n"
        "  \"spot_light_inner_angle_degrees\": %.6f,\n"
        "  \"spot_light_outer_angle_degrees\": %.6f,\n"
        "  \"spot_light_max_active\": %u,\n"
        "  \"spot_light_activation_distance\": %.6f,\n"
        "  \"spot_light_activation_forward_offset\": %.6f,\n"
        "  \"shadowed_spot_light_max_active\": %u,\n"
        "  \"shadowed_spot_light_activation_distance\": %.6f,\n"
        "  \"shadowed_spot_light_activation_forward_offset\": %.6f,\n"
        "  \"source_offset_x\": %.6f,\n"
        "  \"source_offset_y\": %.6f,\n"
        "  \"source_offset_z\": %.6f,\n"
        "  \"draw_light_proxies\": %d,\n"
        "  \"debug_draw_activation_volumes\": %d,\n"
        "  \"debug_draw_scene_light_gizmos\": %d,\n"
        "  \"debug_draw_light_directions\": %d,\n"
        "  \"debug_draw_light_volumes\": %d,\n"
        "  \"debug_draw_light_labels\": %d,\n"
        "  \"debug_draw_vehicle_volumes\": %d,\n"
        "  \"debug_draw_vehicle_light_ranges\": %d,\n"
        "  \"paint_ball_bounce_limit\": %u,\n"
        "  \"paint_ball_color_r\": %.6f,\n"
        "  \"paint_ball_color_g\": %.6f,\n"
        "  \"paint_ball_color_b\": %.6f,\n"
        "  \"paint_ball_cycle_color\": %d\n"
        "}\n",
        static_cast<std::uint32_t>(m_sceneKind),
        m_cycleDayNight ? 1 : 0,
        m_animateSunAzimuth ? 1 : 0,
        m_timeOfDay,
        m_dayNightSpeed,
        m_sunAzimuthDegrees,
        m_orbitDistanceScale,
        m_sunIntensity,
        m_moonIntensity,
        m_ambientIntensity,
        m_pointLightIntensity,
        m_shadowCascadeSplit,
        m_shadowMapSize,
        m_shadowBlur ? 1 : 0,
        m_mainDrawDistance,
        m_shadowDrawDistance,
        m_spotLightIntensityScale,
        m_spotLightRangeScale,
        m_spotLightInnerAngleDegrees,
        m_spotLightOuterAngleDegrees,
        m_spotLightMaxActive,
        m_spotLightActivationDistance,
        m_spotLightActivationForwardOffset,
        m_shadowedSpotLightMaxActive,
        m_shadowedSpotLightActivationDistance,
        m_shadowedSpotLightActivationForwardOffset,
        m_spotLightSourceOffset.x,
        m_spotLightSourceOffset.y,
        m_spotLightSourceOffset.z,
        m_drawLightProxies ? 1 : 0,
        m_debugDrawActivationVolumes ? 1 : 0,
        m_debugDrawSceneLightGizmos ? 1 : 0,
        m_debugDrawLightDirections ? 1 : 0,
        m_debugDrawLightVolumes ? 1 : 0,
        m_debugDrawLightLabels ? 1 : 0,
        m_debugDrawVehicleVolumes ? 1 : 0,
        m_debugDrawVehicleLightRanges ? 1 : 0,
        m_paintBallSettings.bounceLimit,
        m_paintBallSettings.baseColor.x,
        m_paintBallSettings.baseColor.y,
        m_paintBallSettings.baseColor.z,
        m_paintBallSettings.cycleColorOnShoot ? 1 : 0
    );

    std::fclose(file);
}
