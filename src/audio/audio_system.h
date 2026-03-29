#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace audio
{
struct Clip
{
    SDL_AudioSpec spec = {};
    std::vector<std::uint8_t> pcm;
};

struct System
{
    static constexpr std::size_t kRicochetClipCount = 3;
    static constexpr std::size_t kRicochetVoiceCount = 8;
    std::array<Clip, kRicochetClipCount> ricochetClips = {};
    std::array<SDL_AudioStream*, kRicochetVoiceCount> ricochetVoices = {};
    std::uint32_t nextRicochetVoice = 0;
    std::uint32_t lastRicochetClip = 0xffffffffu;
    std::uint32_t rngState = 0x9e3779b9u;
    bool ready = false;
};

void Initialize(System& audio, std::string_view assetsRoot);
void Shutdown(System& audio);
void PlayRicochet(System& audio);
}
