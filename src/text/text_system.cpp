#include "text/text_system.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>

namespace text
{
namespace
{
void DumpAtlasToArtifacts(std::string_view assetRoot, const Atlas& atlas)
{
    std::filesystem::path repoRoot = std::filesystem::path(assetRoot).parent_path();
    std::filesystem::path dumpDir = repoRoot / "artifacts/text_atlas_cache";
    std::error_code ec;
    std::filesystem::create_directories(dumpDir, ec);
    if (ec)
    {
        return;
    }

    SDL_Surface* surface = SDL_CreateSurface(
        static_cast<int>(atlas.texture.width),
        static_cast<int>(atlas.texture.height),
        SDL_PIXELFORMAT_RGBA8888
    );
    if (surface == nullptr)
    {
        if (surface != nullptr)
        {
            SDL_DestroySurface(surface);
        }
        return;
    }

    if (SDL_LockSurface(surface))
    {
        std::memcpy(surface->pixels, atlas.texture.pixels.data(), atlas.texture.pixels.size());
        SDL_UnlockSurface(surface);
        std::filesystem::path outputPath =
            dumpDir / ("text_atlas_" + std::to_string(static_cast<int>(atlas.fontSize)) + ".bmp");
        SDL_SaveBMP(surface, outputPath.string().c_str());
    }
    SDL_DestroySurface(surface);
}

float QuantizeFontSize(float fontSize)
{
    float clamped = std::clamp(fontSize, 8.0f, 96.0f);
    return std::round(clamped);
}

bool BuildAtlas(System& system, Atlas& atlas, float fontSize, std::string* error)
{
    std::filesystem::path fontPath = std::filesystem::path(system.assetRoot) / "fonts/DejaVuSansMono.ttf";
    TTF_Font* font = TTF_OpenFont(fontPath.string().c_str(), fontSize);
    if (font == nullptr)
    {
        if (error != nullptr)
        {
            *error = SDL_GetError();
        }
        return false;
    }

    int maxAdvance = 0;
    int ascent = TTF_GetFontAscent(font);
    int lineHeight = TTF_GetFontLineSkip(font);
    for (std::uint32_t i = 0; i < kGlyphCount; ++i)
    {
        int minx = 0;
        int maxx = 0;
        int miny = 0;
        int maxy = 0;
        int advance = 0;
        if (!TTF_GetGlyphMetrics(font, kFirstGlyph + i, &minx, &maxx, &miny, &maxy, &advance))
        {
            continue;
        }
        maxAdvance = std::max(maxAdvance, advance);
    }

    std::uint32_t cellWidth = static_cast<std::uint32_t>(std::max(maxAdvance + 4, 8));
    std::uint32_t cellHeight = static_cast<std::uint32_t>(std::max(lineHeight + 4, 8));
    constexpr std::uint32_t atlasCols = 16;
    std::uint32_t atlasRows = (kGlyphCount + atlasCols - 1) / atlasCols;

    atlas = {};
    atlas.fontSize = fontSize;
    atlas.lineHeight = static_cast<float>(lineHeight);
    atlas.ascent = static_cast<float>(ascent);
    atlas.texture.width = cellWidth * atlasCols;
    atlas.texture.height = cellHeight * atlasRows;
    atlas.texture.pixels.assign(
        static_cast<std::size_t>(atlas.texture.width) * static_cast<std::size_t>(atlas.texture.height) * 4u,
        0u
    );
    std::string glyphLine;
    glyphLine.reserve(kGlyphCount);
    for (std::uint32_t i = 0; i < kGlyphCount; ++i)
    {
        std::uint32_t codepoint = kFirstGlyph + i;
        glyphLine.push_back(codepoint < 127 ? static_cast<char>(codepoint) : ' ');
    }

    const SDL_Color glyphColor{255, 255, 255, 255};
    SDL_Surface* lineSurface = TTF_RenderText_Blended(font, glyphLine.c_str(), glyphLine.size(), glyphColor);
    if (lineSurface == nullptr)
    {
        if (error != nullptr)
        {
            *error = SDL_GetError();
        }
        TTF_CloseFont(font);
        return false;
    }

    SDL_Surface* lineRgba = SDL_ConvertSurface(lineSurface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(lineSurface);
    if (lineRgba == nullptr)
    {
        if (error != nullptr)
        {
            *error = SDL_GetError();
        }
        TTF_CloseFont(font);
        return false;
    }

    int penX = 0;
    for (std::uint32_t i = 0; i < kGlyphCount; ++i)
    {
        std::uint32_t codepoint = kFirstGlyph + i;
        int minx = 0;
        int maxx = 0;
        int miny = 0;
        int maxy = 0;
        int advance = 0;
        if (!TTF_GetGlyphMetrics(font, codepoint, &minx, &maxx, &miny, &maxy, &advance))
        {
            continue;
        }

        std::uint32_t col = i % atlasCols;
        std::uint32_t row = i / atlasCols;
        std::uint32_t baseX = col * cellWidth;
        std::uint32_t baseY = row * cellHeight;
        std::uint32_t glyphWidth = static_cast<std::uint32_t>(std::max(maxx - minx, 0));
        std::uint32_t glyphHeight = static_cast<std::uint32_t>(std::max(maxy - miny, 0));
        std::uint32_t copyWidth = std::min<std::uint32_t>(glyphWidth, cellWidth);
        std::uint32_t copyHeight = std::min<std::uint32_t>(glyphHeight, cellHeight);
        std::uint32_t drawX = baseX + 2u + static_cast<std::uint32_t>(std::max(minx, 0));
        std::int32_t drawYSigned = static_cast<std::int32_t>(baseY) + 2 + ascent - maxy;
        std::uint32_t drawY = static_cast<std::uint32_t>(std::max(drawYSigned, 0));
        std::int32_t sourceXSigned = penX + minx;
        std::int32_t sourceYSigned = ascent - maxy;
        std::uint32_t sourceX = static_cast<std::uint32_t>(std::max(sourceXSigned, 0));
        std::uint32_t sourceY = static_cast<std::uint32_t>(std::max(sourceYSigned, 0));

        if (copyWidth > 0 && copyHeight > 0 && SDL_LockSurface(lineRgba))
        {
            for (std::uint32_t y = 0; y < copyHeight; ++y)
            {
                for (std::uint32_t x = 0; x < copyWidth; ++x)
                {
                    Uint8 r = 0;
                    Uint8 g = 0;
                    Uint8 b = 0;
                    Uint8 a = 0;
                    if (!SDL_ReadSurfacePixel(
                            lineRgba,
                            static_cast<int>(sourceX + x),
                            static_cast<int>(sourceY + y),
                            &r,
                            &g,
                            &b,
                            &a))
                    {
                        continue;
                    }
                    std::size_t dstIndex =
                        ((static_cast<std::size_t>(drawY + y) * atlas.texture.width) + (drawX + x)) * 4u;
                    atlas.texture.pixels[dstIndex + 0] = r;
                    atlas.texture.pixels[dstIndex + 1] = g;
                    atlas.texture.pixels[dstIndex + 2] = b;
                    atlas.texture.pixels[dstIndex + 3] = a;
                }
            }
            SDL_UnlockSurface(lineRgba);
        }

        Glyph& glyph = atlas.glyphs[i];
        glyph.u0 = static_cast<float>(drawX) / static_cast<float>(atlas.texture.width);
        glyph.v0 = static_cast<float>(drawY) / static_cast<float>(atlas.texture.height);
        glyph.u1 = static_cast<float>(drawX + copyWidth) / static_cast<float>(atlas.texture.width);
        glyph.v1 = static_cast<float>(drawY + copyHeight) / static_cast<float>(atlas.texture.height);
        glyph.width = static_cast<float>(copyWidth);
        glyph.height = static_cast<float>(copyHeight);
        glyph.minX = static_cast<float>(minx);
        glyph.maxY = static_cast<float>(maxy);
        glyph.advance = static_cast<float>(advance);
        penX += advance;
    }

    atlas.dirty = true;
    atlas.valid = true;
    DumpAtlasToArtifacts(system.assetRoot, atlas);
    SDL_DestroySurface(lineRgba);
    TTF_CloseFont(font);
    return true;
}
}

bool Initialize(System& system, std::string_view assetRoot, std::string* error)
{
    Shutdown(system);
    system.assetRoot = std::string(assetRoot);
    bool ok = FindOrCreateAtlas(system, system.baseFontSize) != std::numeric_limits<std::uint32_t>::max();
    if (!ok && error != nullptr && error->empty())
    {
        *error = "failed to build base font atlas";
    }
    return ok;
}

void Shutdown(System& system)
{
    system.assetRoot.clear();
    system.atlases = {};
    system.atlasCount = 0;
    system.entries = {};
    system.entryCount = 0;
}

void BeginFrame(System& system)
{
    system.entryCount = 0;
}

std::uint32_t FindOrCreateAtlas(System& system, float fontSize)
{
    float quantized = QuantizeFontSize(fontSize);
    for (std::uint32_t i = 0; i < system.atlasCount; ++i)
    {
        if (system.atlases[i].valid && std::fabs(system.atlases[i].fontSize - quantized) < 0.5f)
        {
            return i;
        }
    }

    std::uint32_t slot = 0;
    if (system.atlasCount < kMaxAtlases)
    {
        slot = system.atlasCount++;
    }
    else
    {
        slot = 0;
        for (std::uint32_t i = 1; i < system.atlasCount; ++i)
        {
            if (system.atlases[i].fontSize > system.atlases[slot].fontSize)
            {
                slot = i;
            }
        }
    }

    std::string error;
    if (!BuildAtlas(system, system.atlases[slot], quantized, &error))
    {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return slot;
}

void DrawText(System& system, float x, float y, float fontSize, Vec4 color, std::string_view text)
{
    if (system.entryCount >= system.entries.size() || text.empty())
    {
        return;
    }

    std::uint32_t atlasIndex = FindOrCreateAtlas(system, fontSize);
    if (atlasIndex == std::numeric_limits<std::uint32_t>::max())
    {
        return;
    }

    Entry& entry = system.entries[system.entryCount++];
    entry.text.fill('\0');
    std::snprintf(entry.text.data(), entry.text.size(), "%.*s", static_cast<int>(text.size()), text.data());
    entry.length = static_cast<std::uint32_t>(std::strlen(entry.text.data()));
    entry.atlasIndex = atlasIndex;
    entry.x = x;
    entry.y = y;
    entry.color = color;
}
}
