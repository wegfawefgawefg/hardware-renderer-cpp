#pragma once

#include <array>
#include <cstdint>

#include "math_types.h"

struct PaintSplat
{
    Vec3 position = {};
    float radius = 0.35f;
    Vec3 normal = {0.0f, 1.0f, 0.0f};
    float pad0 = 0.0f;
    Vec3 color = {1.0f, 0.2f, 0.75f};
    float pad1 = 0.0f;
};

constexpr std::uint32_t kMaxAccumulatedPaintPerEntity = 64;

struct PersistentPaintStamp
{
    Vec3 localPosition = {};
    float radius = 0.35f;
    Vec3 localNormal = {0.0f, 1.0f, 0.0f};
    float seed = 0.0f;
    Vec3 color = {1.0f, 0.2f, 0.75f};
    float opacity = 1.0f;
};

struct EntityPaintLayer
{
    std::array<PersistentPaintStamp, kMaxAccumulatedPaintPerEntity> stamps = {};
    std::uint32_t stampCount = 0;
    std::uint32_t nextStampIndex = 0;
};
