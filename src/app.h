#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "state.h"

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
    void InitializeImGui();
    void ShutdownImGui();
    void ProcessImGuiEvent(const SDL_Event& event);
    void BuildImGui();
    void ApplyLighting(SceneUniforms& uniforms, float dtSeconds);
    void TryPlaceShadowTestSpotlight(int mouseX, int mouseY);
    void UpdateOverlayText(const SceneUniforms* uniforms = nullptr);
    void ReloadScene();
    void LoadDebugSettings();
    void SaveDebugSettings() const;
    void LoadVehicleLightRigs();
    void SaveVehicleLightRigs() const;
    void TryPlaceVehicleLight(int mouseX, int mouseY);
    int FindActiveVehicleLightIndex() const;
    bool TryFirePaintBall();
    bool TryFireFractureShot();
    bool TryApplySurfaceMaskBrush();
    void UpdateFractureSandbox(float dtSeconds);
    void UpdatePaintBalls(float dtSeconds);
    void UpdateSurfaceMaskBrush(float dtSeconds);
    void AppendPaintSplat(const PaintSplatSpawn& splat);
    void AppendPersistentPaint(const PaintSplatSpawn& splat);
    std::uint32_t CountAccumulatedPaintStamps() const;
    void DrawLightDebugOverlay();
    void BuildLightingWindow(bool& debugSettingsChanged);
    void BuildProfilerWindow();
    void BuildSunViewWindow();
    void BuildShadowsWindow(bool& debugSettingsChanged);
    void BuildSpotlightsWindow(bool& debugSettingsChanged);
    void BuildPaintBallsWindow(bool& debugSettingsChanged);
    void BuildSurfaceMasksWindow(bool& debugSettingsChanged);
    void BuildFractureWindow(bool& debugSettingsChanged);
    void BuildVehicleLightsWindow(bool& debugSettingsChanged);

    SDL_Window* m_window = nullptr;
    TTF_Font* m_uiFont = nullptr;
    State m_state;
};
