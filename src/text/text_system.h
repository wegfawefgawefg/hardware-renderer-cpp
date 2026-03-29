#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "math_types.h"
#include "scene.h"

namespace text
{
constexpr std::uint32_t kFirstGlyph = 32;
constexpr std::uint32_t kGlyphCount = 96;
constexpr std::uint32_t kMaxEntries = 128;
constexpr std::uint32_t kMaxAtlases = 8;

struct Glyph
{
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minX = 0.0f;
    float maxY = 0.0f;
    float advance = 0.0f;
};

struct Entry
{
    std::array<char, 128> text = {};
    std::uint32_t length = 0;
    std::uint32_t atlasIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct Atlas
{
    TextureData texture = {};
    std::array<Glyph, kGlyphCount> glyphs = {};
    float fontSize = 22.0f;
    float lineHeight = 24.0f;
    float ascent = 18.0f;
    bool dirty = false;
    bool valid = false;
};

struct System
{
    float baseFontSize = 22.0f;
    std::string assetRoot;
    std::array<Atlas, kMaxAtlases> atlases = {};
    std::uint32_t atlasCount = 0;
    std::array<Entry, kMaxEntries> entries = {};
    std::uint32_t entryCount = 0;
};

bool Initialize(System& system, std::string_view assetRoot, std::string* error = nullptr);
void Shutdown(System& system);
void BeginFrame(System& system);
void DrawText(System& system, float x, float y, float fontSize, Vec4 color, std::string_view text);
std::uint32_t FindOrCreateAtlas(System& system, float fontSize);
}
