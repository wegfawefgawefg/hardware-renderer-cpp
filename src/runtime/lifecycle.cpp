#include "app.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string_view>

#include "audio/audio_system.h"
#include "text/text_system.h"

namespace
{
constexpr std::string_view kWindowTitle = "hardware-renderer-cpp";
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";
constexpr std::string_view kCharacterModelAsset = "kenney/animated-characters-1/Model/characterMedium.fbx";
constexpr std::string_view kCharacterIdleAsset = "kenney/animated-characters-1/Animations/idle.fbx";
constexpr std::string_view kCharacterRunAsset = "kenney/animated-characters-1/Animations/run.fbx";
constexpr std::string_view kCharacterJumpAsset = "kenney/animated-characters-1/Animations/jump.fbx";
constexpr std::string_view kCharacterTextureAsset = "kenney/animated-characters-1/Skins/survivorMaleB.png";
constexpr int kInitialWindowWidth = 1440;
constexpr int kInitialWindowHeight = 900;

void CenterWindowOnPrimaryDisplay(SDL_Window* window)
{
    SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay == 0)
    {
        return;
    }

    SDL_Rect bounds{};
    if (!SDL_GetDisplayBounds(primaryDisplay, &bounds))
    {
        return;
    }

    int x = bounds.x + (bounds.w - kInitialWindowWidth) / 2;
    int y = bounds.y + (bounds.h - kInitialWindowHeight) / 2;
    SDL_SetWindowPosition(window, x, y);
}

std::filesystem::path MakeAssetPath(std::string_view relativePath)
{
    return std::filesystem::path(HARDWARE_RENDERER_ASSETS_ROOT) / std::filesystem::path(relativePath);
}
}

App::~App()
{
    Shutdown();
}

void App::Run()
{
    auto& runtime = m_state.runtime;
    Initialize();

    auto previousTime = std::chrono::steady_clock::now();
    while (runtime.running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            HandleEvent(event);
        }

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = currentTime - previousTime;
        previousTime = currentTime;
        Update(delta.count());
    }
}

void App::Initialize()
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    if (!TTF_Init())
    {
        throw std::runtime_error(std::string("TTF_Init failed: ") + SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, kX11DialogWindowType.data());
    m_window = SDL_CreateWindow(
        kWindowTitle.data(),
        kInitialWindowWidth,
        kInitialWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_window == nullptr)
    {
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    std::string textError;
    if (!text::Initialize(m_state.text, HARDWARE_RENDERER_ASSETS_ROOT, &textError))
    {
        throw std::runtime_error(std::string("text::Initialize failed: ") + textError);
    }

    audio::Initialize(m_audio, HARDWARE_RENDERER_ASSETS_ROOT);

    CenterWindowOnPrimaryDisplay(m_window);
    core.assetRegistry.ScanFbx(HARDWARE_RENDERER_ASSETS_ROOT);
    LoadDebugSettings();
    LoadVehicleLightRigs();

    const std::filesystem::path* characterPath = core.assetRegistry.FindByRelativePath(kCharacterModelAsset);
    const std::filesystem::path* idlePath = core.assetRegistry.FindByRelativePath(kCharacterIdleAsset);
    const std::filesystem::path* runPath = core.assetRegistry.FindByRelativePath(kCharacterRunAsset);
    const std::filesystem::path* jumpPath = core.assetRegistry.FindByRelativePath(kCharacterJumpAsset);
    const std::filesystem::path texturePath = MakeAssetPath(kCharacterTextureAsset);
    if (characterPath != nullptr &&
        idlePath != nullptr &&
        runPath != nullptr &&
        jumpPath != nullptr &&
        std::filesystem::exists(texturePath))
    {
        core.characterSet = LoadKenneyCharacterAnimationSet(
            characterPath->string(),
            texturePath.string(),
            idlePath->string(),
            runPath->string(),
            jumpPath->string()
        );
        runtime.hasCharacter = true;
    }

    ReloadScene();
}

void App::Shutdown()
{
    auto& core = m_state.core;
    ShutdownImGui();
    core.renderer.Shutdown();
    audio::Shutdown(m_audio);

    text::Shutdown(m_state.text);
    TTF_Quit();

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void App::SyncRendererSize()
{
    auto& runtime = m_state.runtime;
    auto& core = m_state.core;
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    runtime.windowWidth = static_cast<std::uint32_t>(width > 0 ? width : 1);
    runtime.windowHeight = static_cast<std::uint32_t>(height > 0 ? height : 1);
    core.renderer.Resize(runtime.windowWidth, runtime.windowHeight);
}

void App::UpdateWindowTitle()
{
    auto& runtime = m_state.runtime;
    char title[256];
    float ms = runtime.smoothedFps > 0.0f ? 1000.0f / runtime.smoothedFps : 0.0f;
    std::snprintf(
        title,
        sizeof(title),
        "%s  %.2f ms  %.0f fps  %ux%u",
        kWindowTitle.data(),
        ms,
        runtime.smoothedFps,
        runtime.windowWidth,
        runtime.windowHeight
    );
    SDL_SetWindowTitle(m_window, title);
}

void App::ResetMouseCapture(bool captured)
{
    m_state.runtime.mouseCaptured = captured;
    SDL_SetWindowRelativeMouseMode(m_window, captured);
}

void App::UpdateOverlayText(const SceneUniforms* uniforms)
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& text = m_state.text;
    (void)uniforms;

    float ms = runtime.smoothedFps > 0.0f ? 1000.0f / runtime.smoothedFps : 0.0f;
    char buffer[256] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.2f ms   %.0f fps   %ux%u   %u ents   %u tris   %s   %s",
        ms,
        runtime.smoothedFps,
        runtime.windowWidth,
        runtime.windowHeight,
        static_cast<std::uint32_t>(core.scene.entities.size()),
        runtime.sceneTriangleCount,
        core.player.onGround ? "ground" : "air",
        runtime.hasCharacter ? (runtime.activeCharacterAnim == 2 ? "jump" : (runtime.activeCharacterAnim == 1 ? "run" : "idle")) : "nochar"
    );
    text::DrawText(text, 16.0f, 16.0f, 22.0f, Vec4Make(0.9f, 0.95f, 1.0f, 1.0f), buffer);
}
