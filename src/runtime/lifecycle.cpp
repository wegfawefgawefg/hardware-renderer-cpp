#include "app.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr std::string_view kWindowTitle = "hardware-renderer-cpp";
constexpr std::string_view kUiFontAsset = "fonts/DejaVuSansMono.ttf";
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";
constexpr std::string_view kCharacterModelAsset = "kenney/animated-characters-1/Model/characterMedium.fbx";
constexpr std::string_view kCharacterIdleAsset = "kenney/animated-characters-1/Animations/idle.fbx";
constexpr std::string_view kCharacterRunAsset = "kenney/animated-characters-1/Animations/run.fbx";
constexpr std::string_view kCharacterJumpAsset = "kenney/animated-characters-1/Animations/jump.fbx";
constexpr std::string_view kCharacterTextureAsset = "kenney/animated-characters-1/Skins/survivorMaleB.png";
constexpr int kInitialWindowWidth = 1440;
constexpr int kInitialWindowHeight = 900;
constexpr std::uint32_t kOverlayBufferWidth = 512;
constexpr std::uint32_t kOverlayBufferHeight = 128;

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
        throw std::runtime_error("SDL_Init failed");
    }
    if (!TTF_Init())
    {
        throw std::runtime_error("TTF_Init failed");
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

    const std::filesystem::path uiFontPath = MakeAssetPath(kUiFontAsset);
    m_uiFont = TTF_OpenFont(uiFontPath.string().c_str(), 22.0f);
    if (m_uiFont == nullptr)
    {
        throw std::runtime_error(std::string("TTF_OpenFont failed: ") + SDL_GetError());
    }

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

    if (m_uiFont != nullptr)
    {
        TTF_CloseFont(m_uiFont);
        m_uiFont = nullptr;
    }

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
    auto& overlay = m_state.overlay;
    (void)uniforms;
    overlay.pixels.fill(0);

    if (m_uiFont == nullptr)
    {
        return;
    }

    float ms = runtime.smoothedFps > 0.0f ? 1000.0f / runtime.smoothedFps : 0.0f;
    char text[160] = {};
    std::snprintf(
        text,
        sizeof(text),
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

    SDL_Color color = {230, 242, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(m_uiFont, text, std::strlen(text), color);
    if (surface == nullptr)
    {
        return;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (rgbaSurface == nullptr)
    {
        return;
    }

    overlay.width = std::min(static_cast<std::uint32_t>(rgbaSurface->w), kOverlayBufferWidth);
    overlay.height = std::min(static_cast<std::uint32_t>(rgbaSurface->h), kOverlayBufferHeight);

    if (overlay.width > 0 && overlay.height > 0 && SDL_LockSurface(rgbaSurface))
    {
        const auto* sourceRows = static_cast<const std::uint8_t*>(rgbaSurface->pixels);
        for (std::uint32_t row = 0; row < overlay.height; ++row)
        {
            const auto* source = sourceRows + row * rgbaSurface->pitch;
            auto* destination =
                reinterpret_cast<std::uint8_t*>(overlay.pixels.data() + row * kOverlayBufferWidth);
            std::memcpy(destination, source, overlay.width * sizeof(std::uint32_t));
        }
        SDL_UnlockSurface(rgbaSurface);
    }

    SDL_DestroySurface(rgbaSurface);
}
