#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "math_types.h"

namespace decals
{
using FlatDecalTemplateId = std::uint16_t;
constexpr FlatDecalTemplateId kInvalidFlatDecalTemplateId = 0xffffu;

struct FlatDecalTemplate
{
    std::string name;
    std::string albedoAssetPath;
    std::string normalAssetPath;
    bool flipNormalY = true;
};

struct FlatDecalInstance
{
    Mat4 transform = {};
    FlatDecalTemplateId templateId = kInvalidFlatDecalTemplateId;
    bool active = false;
};

struct FlatDecalSystem
{
    static constexpr std::uint32_t kMaxTemplates = 16;
    static constexpr std::uint32_t kMaxInstances = 2048;

    std::array<FlatDecalTemplate, kMaxTemplates> templates = {};
    std::uint32_t templateCount = 0;
    std::array<FlatDecalInstance, kMaxInstances> instances = {};
    std::uint32_t nextInstance = 0;
};

void ResetFlatDecalTemplates(FlatDecalSystem& system);
void ClearFlatDecals(FlatDecalSystem& system);
FlatDecalTemplateId RegisterFlatDecalTemplate(FlatDecalSystem& system, FlatDecalTemplate decalTemplate);
bool SpawnFlatDecal(
    FlatDecalSystem& system,
    FlatDecalTemplateId templateId,
    Vec3 hitPosition,
    Vec3 hitNormal,
    float size,
    float rollRadians);
}
