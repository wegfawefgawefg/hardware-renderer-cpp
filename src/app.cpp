#include "app.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr std::string_view kWindowTitle = "hardware-renderer-cpp";
constexpr std::string_view kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";
constexpr int kInitialWindowWidth = 1440;
constexpr int kInitialWindowHeight = 900;
constexpr float kTitleRefreshPeriod = 0.20f;

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
}

App::~App()
{
    Shutdown();
}

void App::Run()
{
    Initialize();

    auto previousTime = std::chrono::steady_clock::now();
    while (m_running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            HandleEvent(event);
        }

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = currentTime - previousTime;
        previousTime = currentTime;

        float deltaSeconds = delta.count();
        Update(deltaSeconds);
    }
}

void App::Initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error("SDL_Init failed");
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

    CenterWindowOnPrimaryDisplay(m_window);
    m_scene = LoadSampleScene();
    m_renderer.Initialize(m_window, m_scene);
    SyncRendererSize();
    UpdateWindowTitle();
}

void App::Shutdown()
{
    m_renderer.Shutdown();

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void App::HandleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        m_running = false;
        break;

    case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE)
        {
            m_running = false;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            ResetMouseCapture(true);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_RIGHT)
        {
            ResetMouseCapture(false);
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (m_mouseCaptured)
        {
            CameraAddMouseLook(
                m_camera,
                static_cast<float>(event.motion.xrel),
                static_cast<float>(event.motion.yrel)
            );
        }
        break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESIZED:
        m_windowResized = true;
        break;

    default:
        break;
    }
}

void App::Update(float dtSeconds)
{
    m_elapsedSeconds += dtSeconds;

    if (dtSeconds > 0.0f)
    {
        float instantFps = 1.0f / dtSeconds;
        if (m_smoothedFps <= 0.0f)
        {
            m_smoothedFps = instantFps;
        }
        else
        {
            m_smoothedFps = m_smoothedFps * 0.95f + instantFps * 0.05f;
        }
    }

    CameraUpdateFromInput(m_camera, dtSeconds, m_mouseCaptured);

    if (m_windowResized)
    {
        SyncRendererSize();
        m_windowResized = false;
    }

    m_titleRefreshSeconds -= dtSeconds;
    if (m_titleRefreshSeconds <= 0.0f)
    {
        UpdateWindowTitle();
        m_titleRefreshSeconds = kTitleRefreshPeriod;
    }

    SceneUniforms uniforms{};

    Mat4 roomRotation = Mat4RotateX(DegreesToRadians(-90.0f));
    Mat4 roomScale = Mat4Scale(1.0f);
    Mat4 roomTransform = Mat4Mul(roomRotation, roomScale);
    uniforms.model = roomTransform;
    uniforms.view = CameraViewMatrix(m_camera);
    uniforms.proj = Mat4Perspective(
        DegreesToRadians(60.0f),
        static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight),
        0.1f,
        200.0f
    );
    uniforms.proj.m[5] *= -1.0f;
    uniforms.cameraPosition = Vec4Make(
        m_camera.position.x,
        m_camera.position.y,
        m_camera.position.z,
        1.0f
    );

    float lightRadius = 2.8f;
    float lightAngle = m_elapsedSeconds * 0.7f;
    uniforms.pointLightPosition = Vec4Make(
        std::cos(lightAngle) * lightRadius,
        2.1f,
        std::sin(lightAngle) * lightRadius,
        1.0f
    );
    uniforms.pointLightColor = Vec4Make(8.0f, 7.7f, 7.4f, 1.0f);
    uniforms.ambientColor = Vec4Make(0.11f, 0.12f, 0.14f, 1.0f);

    m_renderer.Render(uniforms);
}

void App::SyncRendererSize()
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    m_windowWidth = static_cast<std::uint32_t>(width > 0 ? width : 1);
    m_windowHeight = static_cast<std::uint32_t>(height > 0 ? height : 1);
    m_renderer.Resize(m_windowWidth, m_windowHeight);
}

void App::UpdateWindowTitle()
{
    char title[256];
    float ms = m_smoothedFps > 0.0f ? 1000.0f / m_smoothedFps : 0.0f;
    std::snprintf(
        title,
        sizeof(title),
        "%s  %.2f ms  %.0f fps  %ux%u",
        kWindowTitle.data(),
        ms,
        m_smoothedFps,
        m_windowWidth,
        m_windowHeight
    );
    SDL_SetWindowTitle(m_window, title);
}

void App::ResetMouseCapture(bool captured)
{
    m_mouseCaptured = captured;
    SDL_SetWindowRelativeMouseMode(m_window, captured);
}
