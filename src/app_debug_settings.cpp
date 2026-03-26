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
    float boolValue = 0.0f;
    if (ExtractFloat(text, "\"debug_draw_activation_volumes\"", boolValue)) m_debugDrawActivationVolumes = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_scene_light_gizmos\"", boolValue)) m_debugDrawSceneLightGizmos = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_directions\"", boolValue)) m_debugDrawLightDirections = boolValue != 0.0f;
    if (ExtractFloat(text, "\"debug_draw_light_labels\"", boolValue)) m_debugDrawLightLabels = boolValue != 0.0f;
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
        "  \"debug_draw_activation_volumes\": %d,\n"
        "  \"debug_draw_scene_light_gizmos\": %d,\n"
        "  \"debug_draw_light_directions\": %d,\n"
        "  \"debug_draw_light_labels\": %d\n"
        "}\n",
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
        m_debugDrawActivationVolumes ? 1 : 0,
        m_debugDrawSceneLightGizmos ? 1 : 0,
        m_debugDrawLightDirections ? 1 : 0,
        m_debugDrawLightLabels ? 1 : 0
    );

    std::fclose(file);
}
