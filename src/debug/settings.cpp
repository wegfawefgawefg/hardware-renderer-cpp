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
    auto& lighting = m_state.lighting;
    auto& vehicle = m_state.vehicleLights;
    auto& paint = m_state.paint;
    std::ifstream in(DebugSettingsPath());
    if (!in)
    {
        return;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();
    float boolValue = 0.0f;

    std::uint32_t sceneKind = static_cast<std::uint32_t>(lighting.sceneKind);
    if (ExtractUInt(text, "\"scene_kind\"", sceneKind) && sceneKind <= static_cast<std::uint32_t>(SceneKind::VehicleLightTest))
    {
        lighting.sceneKind = static_cast<SceneKind>(sceneKind);
    }

    ExtractFloat(text, "\"cycle_day_night\"", boolValue);
    lighting.cycleDayNight = boolValue != 0.0f;
    if (ExtractFloat(text, "\"animate_sun_azimuth\"", boolValue)) lighting.animateSunAzimuth = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_visualize_uv\"", boolValue)) lighting.debugVisualizeUv = boolValue != 0.0f;
    ExtractFloat(text, "\"time_of_day\"", lighting.timeOfDay);
    ExtractFloat(text, "\"day_night_speed\"", lighting.dayNightSpeed);
    ExtractFloat(text, "\"uv_debug_scale\"", lighting.uvDebugScale);
    ExtractUInt(text, "\"uv_debug_mode\"", lighting.uvDebugMode);
    ExtractFloat(text, "\"sun_azimuth_degrees\"", lighting.sunAzimuthDegrees);
    ExtractFloat(text, "\"orbit_distance_scale\"", lighting.orbitDistanceScale);
    ExtractFloat(text, "\"sun_intensity\"", lighting.sunIntensity);
    ExtractFloat(text, "\"moon_intensity\"", lighting.moonIntensity);
    ExtractFloat(text, "\"ambient_intensity\"", lighting.ambientIntensity);
    ExtractFloat(text, "\"point_light_intensity\"", lighting.pointLightIntensity);
    ExtractFloat(text, "\"shadow_cascade_split\"", lighting.shadowCascadeSplit);
    ExtractUInt(text, "\"shadow_map_size\"", lighting.shadowMapSize);
    if (ExtractFloat(text, "\"shadow_blur\"", boolValue)) lighting.shadowBlur = boolValue != 0.0f;
    ExtractFloat(text, "\"main_draw_distance\"", lighting.mainDrawDistance);
    ExtractFloat(text, "\"shadow_draw_distance\"", lighting.shadowDrawDistance);
    ExtractFloat(text, "\"spot_light_intensity_scale\"", lighting.spotLightIntensityScale);
    ExtractFloat(text, "\"spot_light_range_scale\"", lighting.spotLightRangeScale);
    ExtractFloat(text, "\"spot_light_inner_angle_degrees\"", lighting.spotLightInnerAngleDegrees);
    ExtractFloat(text, "\"spot_light_outer_angle_degrees\"", lighting.spotLightOuterAngleDegrees);
    ExtractUInt(text, "\"spot_light_max_active\"", lighting.spotLightMaxActive);
    ExtractFloat(text, "\"spot_light_activation_distance\"", lighting.spotLightActivationDistance);
    ExtractFloat(text, "\"spot_light_activation_forward_offset\"", lighting.spotLightActivationForwardOffset);
    ExtractUInt(text, "\"shadowed_spot_light_max_active\"", lighting.shadowedSpotLightMaxActive);
    ExtractFloat(text, "\"shadowed_spot_light_activation_distance\"", lighting.shadowedSpotLightActivationDistance);
    ExtractFloat(text, "\"shadowed_spot_light_activation_forward_offset\"", lighting.shadowedSpotLightActivationForwardOffset);
    ExtractFloat(text, "\"source_offset_x\"", lighting.spotLightSourceOffset.x);
    ExtractFloat(text, "\"source_offset_y\"", lighting.spotLightSourceOffset.y);
    ExtractFloat(text, "\"source_offset_z\"", lighting.spotLightSourceOffset.z);
    if (ExtractFloat(text, "\"draw_light_proxies\"", boolValue)) lighting.drawLightProxies = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_activation_volumes\"", boolValue)) lighting.debugDrawActivationVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_scene_light_gizmos\"", boolValue)) lighting.debugDrawSceneLightGizmos = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_directions\"", boolValue)) lighting.debugDrawLightDirections = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_volumes\"", boolValue)) lighting.debugDrawLightVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_labels\"", boolValue)) lighting.debugDrawLightLabels = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_vehicle_volumes\"", boolValue)) vehicle.debugDrawVehicleVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_vehicle_light_ranges\"", boolValue)) vehicle.debugDrawVehicleLightRanges = boolValue != 0.0f;
    ExtractUInt(text, "\"paint_ball_bounce_limit\"", paint.ballSettings.bounceLimit);
    ExtractFloat(text, "\"paint_ball_shoot_speed\"", paint.ballSettings.shootSpeed);
    ExtractFloat(text, "\"paint_ball_fire_rate\"", paint.ballSettings.fireRate);
    ExtractFloat(text, "\"paint_ball_gravity\"", paint.ballSettings.gravity);
    ExtractFloat(text, "\"paint_ball_restitution\"", paint.ballSettings.restitution);
    ExtractFloat(text, "\"paint_ball_radius\"", paint.ballSettings.radius);
    ExtractFloat(text, "\"paint_blob_radius\"", paint.ballSettings.blobRadius);
    std::uint32_t maskChannel = static_cast<std::uint32_t>(paint.ballSettings.maskChannel);
    if (ExtractUInt(text, "\"paint_mask_channel\"", maskChannel) && maskChannel <= 3u)
    {
        paint.ballSettings.maskChannel = static_cast<SurfaceMaskChannel>(maskChannel);
    }
    ExtractFloat(text, "\"paint_mask_strength\"", paint.ballSettings.maskStrength);
    ExtractFloat(text, "\"paint_ball_color_r\"", paint.ballSettings.baseColor.x);
    ExtractFloat(text, "\"paint_ball_color_g\"", paint.ballSettings.baseColor.y);
    ExtractFloat(text, "\"paint_ball_color_b\"", paint.ballSettings.baseColor.z);
    if (ExtractFloat(text, "\"paint_ball_cycle_color\"", boolValue)) paint.ballSettings.cycleColorOnShoot = boolValue != 0.0f;
}

void App::SaveDebugSettings() const
{
    const auto& lighting = m_state.lighting;
    const auto& vehicle = m_state.vehicleLights;
    const auto& paint = m_state.paint;
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
        "  \"debug_visualize_uv\": %d,\n"
        "  \"time_of_day\": %.6f,\n"
        "  \"day_night_speed\": %.6f,\n"
        "  \"uv_debug_scale\": %.6f,\n"
        "  \"uv_debug_mode\": %u,\n"
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
        "  \"paint_ball_shoot_speed\": %.6f,\n"
        "  \"paint_ball_fire_rate\": %.6f,\n"
        "  \"paint_ball_gravity\": %.6f,\n"
        "  \"paint_ball_restitution\": %.6f,\n"
        "  \"paint_ball_radius\": %.6f,\n"
        "  \"paint_blob_radius\": %.6f,\n"
        "  \"paint_mask_channel\": %u,\n"
        "  \"paint_mask_strength\": %.6f,\n"
        "  \"paint_ball_color_r\": %.6f,\n"
        "  \"paint_ball_color_g\": %.6f,\n"
        "  \"paint_ball_color_b\": %.6f,\n"
        "  \"paint_ball_cycle_color\": %d\n"
        "}\n",
        static_cast<std::uint32_t>(lighting.sceneKind),
        lighting.cycleDayNight ? 1 : 0,
        lighting.animateSunAzimuth ? 1 : 0,
        lighting.debugVisualizeUv ? 1 : 0,
        lighting.timeOfDay,
        lighting.dayNightSpeed,
        lighting.uvDebugScale,
        lighting.uvDebugMode,
        lighting.sunAzimuthDegrees,
        lighting.orbitDistanceScale,
        lighting.sunIntensity,
        lighting.moonIntensity,
        lighting.ambientIntensity,
        lighting.pointLightIntensity,
        lighting.shadowCascadeSplit,
        lighting.shadowMapSize,
        lighting.shadowBlur ? 1 : 0,
        lighting.mainDrawDistance,
        lighting.shadowDrawDistance,
        lighting.spotLightIntensityScale,
        lighting.spotLightRangeScale,
        lighting.spotLightInnerAngleDegrees,
        lighting.spotLightOuterAngleDegrees,
        lighting.spotLightMaxActive,
        lighting.spotLightActivationDistance,
        lighting.spotLightActivationForwardOffset,
        lighting.shadowedSpotLightMaxActive,
        lighting.shadowedSpotLightActivationDistance,
        lighting.shadowedSpotLightActivationForwardOffset,
        lighting.spotLightSourceOffset.x,
        lighting.spotLightSourceOffset.y,
        lighting.spotLightSourceOffset.z,
        lighting.drawLightProxies ? 1 : 0,
        lighting.debugDrawActivationVolumes ? 1 : 0,
        lighting.debugDrawSceneLightGizmos ? 1 : 0,
        lighting.debugDrawLightDirections ? 1 : 0,
        lighting.debugDrawLightVolumes ? 1 : 0,
        lighting.debugDrawLightLabels ? 1 : 0,
        vehicle.debugDrawVehicleVolumes ? 1 : 0,
        vehicle.debugDrawVehicleLightRanges ? 1 : 0,
        paint.ballSettings.bounceLimit,
        paint.ballSettings.shootSpeed,
        paint.ballSettings.fireRate,
        paint.ballSettings.gravity,
        paint.ballSettings.restitution,
        paint.ballSettings.radius,
        paint.ballSettings.blobRadius,
        static_cast<std::uint32_t>(paint.ballSettings.maskChannel),
        paint.ballSettings.maskStrength,
        paint.ballSettings.baseColor.x,
        paint.ballSettings.baseColor.y,
        paint.ballSettings.baseColor.z,
        paint.ballSettings.cycleColorOnShoot ? 1 : 0
    );

    std::fclose(file);
}
