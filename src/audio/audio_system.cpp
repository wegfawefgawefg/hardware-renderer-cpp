#include "audio/audio_system.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <vorbis/vorbisfile.h>

namespace audio
{
namespace
{
constexpr std::array<std::string_view, 3> kRicochetAssets = {
    "ricochet_low.ogg",
    "ricochet_mid.ogg",
    "ricochet_high.ogg",
};
constexpr std::string_view kAudioLogPath = "/tmp/hardware-renderer-audio.log";
constexpr std::array<std::string_view, 4> kPreferredAudioDrivers = {
    "pulseaudio",
    "pipewire",
    "alsa",
    "dummy",
};

std::uint32_t NextRandom(std::uint32_t& state)
{
    if (state == 0)
    {
        state = 0x9e3779b9u;
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

std::uint32_t PickRicochetClipIndex(System& audio)
{
    const std::uint32_t clipCount = static_cast<std::uint32_t>(audio.ricochetClips.size());
    if (clipCount <= 1)
    {
        return 0;
    }

    std::uint32_t index = NextRandom(audio.rngState) % clipCount;
    if (index == audio.lastRicochetClip)
    {
        index = (index + 1u + (NextRandom(audio.rngState) % (clipCount - 1u))) % clipCount;
    }
    audio.lastRicochetClip = index;
    return index;
}

void AppendAudioLog(const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::ofstream out(std::string(kAudioLogPath), std::ios::app);
    if (out.is_open())
    {
        out << buffer << '\n';
    }
}

bool DecodeVorbisPcm16(std::string_view path, Clip& outClip)
{
    OggVorbis_File vorbisFile{};
    if (ov_fopen(std::string(path).c_str(), &vorbisFile) < 0)
    {
        AppendAudioLog("Failed to open OGG '%s'", std::string(path).c_str());
        return false;
    }

    vorbis_info* info = ov_info(&vorbisFile, -1);
    if (info == nullptr || info->channels <= 0 || info->rate <= 0)
    {
        AppendAudioLog("Invalid OGG stream info for '%s'", std::string(path).c_str());
        ov_clear(&vorbisFile);
        return false;
    }

    outClip.spec.format = SDL_AUDIO_S16;
    outClip.spec.channels = static_cast<Uint8>(std::clamp(info->channels, 1, 2));
    outClip.spec.freq = static_cast<int>(info->rate);
    outClip.pcm.clear();

    std::array<char, 16 * 1024> decodeBuffer{};
    int bitstream = 0;
    for (;;)
    {
        long bytesRead = ov_read(&vorbisFile, decodeBuffer.data(), static_cast<int>(decodeBuffer.size()), 0, 2, 1, &bitstream);
        if (bytesRead <= 0)
        {
            break;
        }
        outClip.pcm.insert(
            outClip.pcm.end(),
            reinterpret_cast<const std::uint8_t*>(decodeBuffer.data()),
            reinterpret_cast<const std::uint8_t*>(decodeBuffer.data()) + bytesRead
        );
    }

    ov_clear(&vorbisFile);
    AppendAudioLog("Loaded ricochet clip '%s' (%d Hz, %u ch, %zu bytes)", std::string(path).c_str(), outClip.spec.freq, static_cast<unsigned>(outClip.spec.channels), outClip.pcm.size());
    return !outClip.pcm.empty();
}
}

void Shutdown(System& audio)
{
    for (SDL_AudioStream*& stream : audio.ricochetVoices)
    {
        if (stream != nullptr)
        {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
    }
    audio.ready = false;
    audio.nextRicochetVoice = 0;
    audio.lastRicochetClip = 0xffffffffu;
    audio.rngState = 0x9e3779b9u ^ static_cast<std::uint32_t>(SDL_GetTicks());

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void Initialize(System& audio, std::string_view assetsRoot)
{
    Shutdown(audio);
    {
        std::ofstream clear(std::string(kAudioLogPath), std::ios::trunc);
        if (clear.is_open())
        {
            clear << "InitializeAudio()\n";
        }
    }

    AppendAudioLog("Available SDL audio drivers:");
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
    {
        if (const char* driver = SDL_GetAudioDriver(i))
        {
            AppendAudioLog("  - %s", driver);
        }
    }

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
    {
        bool audioReady = false;
        for (std::string_view driver : kPreferredAudioDrivers)
        {
            SDL_SetHint(SDL_HINT_AUDIO_DRIVER, std::string(driver).c_str());
            if (SDL_InitSubSystem(SDL_INIT_AUDIO))
            {
                AppendAudioLog("SDL audio subsystem initialized with driver '%s'", driver.data());
                audioReady = true;
                break;
            }
            AppendAudioLog("SDL_InitSubSystem(SDL_INIT_AUDIO) failed for driver '%s': %s", driver.data(), SDL_GetError());
        }
        if (!audioReady)
        {
            AppendAudioLog("Audio disabled: could not initialize any preferred SDL audio driver");
            return;
        }
    }

    bool loadedAnyClip = false;
    for (std::size_t i = 0; i < kRicochetAssets.size(); ++i)
    {
        const std::filesystem::path path = std::filesystem::path(assetsRoot) / kRicochetAssets[i];
        loadedAnyClip |= DecodeVorbisPcm16(path.string(), audio.ricochetClips[i]);
    }
    if (!loadedAnyClip || audio.ricochetClips[0].pcm.empty())
    {
        AppendAudioLog("Audio disabled: no ricochet clips loaded");
        return;
    }

    std::size_t openedVoices = 0;
    for (std::size_t voiceIndex = 0; voiceIndex < audio.ricochetVoices.size(); ++voiceIndex)
    {
        SDL_AudioStream*& stream = audio.ricochetVoices[voiceIndex];
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio.ricochetClips[0].spec, nullptr, nullptr);
        if (stream != nullptr)
        {
            if (!SDL_ResumeAudioStreamDevice(stream))
            {
                AppendAudioLog("Failed to resume ricochet voice %zu: %s", voiceIndex, SDL_GetError());
                SDL_DestroyAudioStream(stream);
                stream = nullptr;
                continue;
            }
            AppendAudioLog("Opened ricochet voice %zu", voiceIndex);
            ++openedVoices;
        }
        else
        {
            AppendAudioLog("Failed to open ricochet voice %zu: %s", voiceIndex, SDL_GetError());
        }
    }

    audio.ready = audio.ricochetVoices[0] != nullptr;
    AppendAudioLog("Ricochet audio %s with %zu voices", audio.ready ? "ready" : "disabled", openedVoices);
}

void PlayRicochet(System& audio)
{
    if (!audio.ready)
    {
        return;
    }

    const std::uint32_t clipIndex = PickRicochetClipIndex(audio);
    const Clip& clip = audio.ricochetClips[clipIndex];
    if (clip.pcm.empty())
    {
        return;
    }

    SDL_AudioStream* stream = audio.ricochetVoices[audio.nextRicochetVoice % audio.ricochetVoices.size()];
    audio.nextRicochetVoice = (audio.nextRicochetVoice + 1u) % static_cast<std::uint32_t>(audio.ricochetVoices.size());
    if (stream == nullptr)
    {
        AppendAudioLog("Ricochet playback skipped: selected voice stream is null");
        return;
    }

    if (!SDL_ClearAudioStream(stream))
    {
        AppendAudioLog("SDL_ClearAudioStream failed: %s", SDL_GetError());
        return;
    }
    if (!SDL_PutAudioStreamData(stream, clip.pcm.data(), static_cast<int>(clip.pcm.size())))
    {
        AppendAudioLog("SDL_PutAudioStreamData failed: %s", SDL_GetError());
        return;
    }
    AppendAudioLog(
        "Queued ricochet clip %u on voice %u (%zu bytes)",
        clipIndex,
        (audio.nextRicochetVoice + audio.ricochetVoices.size() - 1u) % static_cast<std::uint32_t>(audio.ricochetVoices.size()),
        clip.pcm.size());
    if (!SDL_FlushAudioStream(stream))
    {
        AppendAudioLog("SDL_FlushAudioStream failed: %s", SDL_GetError());
    }
}
}
