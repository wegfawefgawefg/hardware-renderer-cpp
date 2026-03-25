#pragma once

#include <chrono>
#include <string>

#include <SDL3/SDL.h>

#include "camera.h"
#include "scene.h"
#include "vulkan_renderer.h"

struct App
{
    ~App();

    void Run();
    void Initialize();
    void Shutdown();
    void HandleEvent(const SDL_Event& event);
    void Update(float dtSeconds);
    void SyncRendererSize();
    void UpdateWindowTitle();
    void ResetMouseCapture(bool captured);

    SDL_Window* m_window = nullptr;
    VulkanRenderer m_renderer;
    SceneData m_scene;
    Camera m_camera;

    bool m_running = true;
    bool m_mouseCaptured = false;
    bool m_windowResized = false;
    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    float m_elapsedSeconds = 0.0f;
    float m_smoothedFps = 0.0f;
    float m_titleRefreshSeconds = 0.0f;
};
