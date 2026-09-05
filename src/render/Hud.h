#pragma once

#include "raylib.h"

// Mirror of the runtime toggles that the HUD can also flip with the mouse.
struct HudControls
{
    // Display modes (also driven by the D/P/T/H keys).
    bool showDensityColors = false;
    bool showPressureColors = false;
    bool showTemperatureColors = false;
    bool heatingEnabled = true;

    // Optional reading groups. Clicking the matching HUD button toggles its
    // numbers on and off; this is independent of the display-mode toggles.
    bool showParticleReadings = true;
    bool showDensityReadings = false;
    bool showPressureReadings = false;
    bool showTemperatureReadings = false;
    bool showPerformanceReadings = false;
};

// Min / average / max captured from the particle field each frame.
struct DensityReadings
{
    float minRatio = 0.0f;
    float avgRatio = 0.0f;
    float maxRatio = 0.0f;
};

struct PressureReadings
{
    float minPressure = 0.0f;
    float avgPressure = 0.0f;
    float maxPressure = 0.0f;
};

struct TemperatureReadings
{
    float minTemperature = 0.0f;
    float avgTemperature = 0.0f;
    float maxTemperature = 0.0f;
};

// Latest live readings, refreshed once per frame by the caller.
struct HudStats
{
    int particleCount = 0;
    int fps = 0;
    float physicsMilliseconds = 0.0f;
    float renderingMilliseconds = 0.0f;
    float simulationSeconds = 0.0f;

    DensityReadings density;
    PressureReadings pressure;
    TemperatureReadings temperature;
};

struct Hud
{
    HudControls controls;
    HudStats stats;
};

// One frame of mouse interaction with the HUD widgets.
struct HudMouseInput
{
    bool pointerOverPanel = false;

    // Display-mode buttons.
    bool densityPressed = false;
    bool pressurePressed = false;
    bool temperaturePressed = false;
    bool heatingPressed = false;

    // Readings-group buttons.
    bool particleReadingsPressed = false;
    bool densityReadingsPressed = false;
    bool pressureReadingsPressed = false;
    bool temperatureReadingsPressed = false;
    bool performanceReadingsPressed = false;

    bool orbitLeftHeld = false;
    bool orbitRightHeld = false;
};

// Reads the mouse against the HUD widgets (call once per frame, before the
// camera update so held orbit buttons and UI clicks are not swallowed).
HudMouseInput readHudMouseInput(const Hud& hud);

// True when the cursor is over the HUD panel. The camera should ignore
// drag / scroll input in that case.
bool hudPointerOverPanel(const Hud& hud);

// Draws the HUD. Must be called inside BeginDrawing()/EndDrawing() after the
// 3D scene has been composited.
void drawHud(const Hud& hud);
