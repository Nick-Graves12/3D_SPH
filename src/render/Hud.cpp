#include "Hud.h"

#include <cstdio>

namespace
{
    // Palette shared with the 2D CFD tool so both demos feel consistent.
    const Color panelBackground = {245, 247, 250, 240};
    const Color cardBorder = {190, 199, 210, 255};
    const Color headingColor = {42, 58, 73, 255};
    const Color mutedText = {92, 105, 118, 255};
    const Color accent = {45, 145, 225, 255};
    const Color accentBright = {75, 165, 235, 255};
    const Color accentDark = {30, 115, 190, 255};
    const Color inactiveButton = {222, 228, 235, 255};
    const Color inactiveHover = {210, 221, 232, 255};
    const Color inactivePressed = {190, 200, 210, 255};

    // Compact overlay panel pinned to the top-right of the 800x600 viewport.
    const float kPanelX = 612.0f;
    const float kPanelY = 8.0f;
    const float kPanelWidth = 180.0f;
    const float kContentX = kPanelX + 8.0f;
    const float kContentWidth = kPanelWidth - 16.0f;

    const float kButtonWidth = 76.0f;
    const float kButtonHeight = 22.0f;
    const float kColumnGap = 4.0f;
    const float kRowGap = 4.0f;
    const float kTextRowHeight = 14.0f;

    void drawText(const char* text, float x, float y, float size, Color color)
    {
        DrawTextEx(
            GetFontDefault(),
            text,
            Vector2{x, y},
            size,
            1.0f,
            color);
    }

    void drawCardHeader(const char* text, float y)
    {
        drawText(text, kContentX, y, 12.0f, headingColor);
    }

    Rectangle buttonAt(float x, float y, float width, float height)
    {
        return Rectangle{x, y, width, height};
    }

    // ---------------------------------------------------------------------
    // Layout: the panel grows/shrinks with the number of active reading
    // rows, so every widget position is computed once per frame and shared
    // by drawing and mouse hit testing.

    struct HudLayout
    {
        Rectangle panel;

        Rectangle displayButtons[4];
        Rectangle readingButtons[5];
        Rectangle orbitLeft;
        Rectangle orbitRight;

        float displayHeaderY = 0.0f;
        float displayDividerY = 0.0f;
        float readingsHeaderY = 0.0f;
        float readingsRowsY = 0.0f;
        float readingsDividerY = 0.0f;
        float cameraHeaderY = 0.0f;
        float hint1Y = 0.0f;
        float hint2Y = 0.0f;

        int readingRows = 0;
    };

    int countReadingRows(const HudControls& controls)
    {
        int rows = 0;
        if (controls.showParticleReadings) rows += 1;
        if (controls.showDensityReadings) rows += 3;
        if (controls.showPressureReadings) rows += 3;
        if (controls.showTemperatureReadings) rows += 3;
        if (controls.showPerformanceReadings) rows += 4;
        return rows;
    }

    HudLayout computeLayout(const HudControls& controls)
    {
        HudLayout layout;
        float y = kPanelY;

        // Display modes group.
        y += 6.0f;
        layout.displayHeaderY = y;
        y += 18.0f;

        layout.displayButtons[0] =
            buttonAt(kContentX, y, kButtonWidth, kButtonHeight);
        layout.displayButtons[1] = buttonAt(
            kContentX + kButtonWidth + kColumnGap,
            y,
            kButtonWidth,
            kButtonHeight);
        y += kButtonHeight + kRowGap;

        layout.displayButtons[2] =
            buttonAt(kContentX, y, kButtonWidth, kButtonHeight);
        layout.displayButtons[3] = buttonAt(
            kContentX + kButtonWidth + kColumnGap,
            y,
            kButtonWidth,
            kButtonHeight);
        y += kButtonHeight + 10.0f;

        layout.displayDividerY = y;
        y += 9.0f;

        // Readings group: header, category toggles, then numeric rows.
        layout.readingsHeaderY = y;
        y += 18.0f;

        layout.readingButtons[0] =
            buttonAt(kContentX, y, kButtonWidth, kButtonHeight);
        layout.readingButtons[1] = buttonAt(
            kContentX + kButtonWidth + kColumnGap,
            y,
            kButtonWidth,
            kButtonHeight);
        y += kButtonHeight + kRowGap;

        layout.readingButtons[2] =
            buttonAt(kContentX, y, kButtonWidth, kButtonHeight);
        layout.readingButtons[3] = buttonAt(
            kContentX + kButtonWidth + kColumnGap,
            y,
            kButtonWidth,
            kButtonHeight);
        y += kButtonHeight + kRowGap;

        layout.readingButtons[4] =
            buttonAt(kContentX, y, kContentWidth, kButtonHeight);
        y += kButtonHeight + 8.0f;

        layout.readingsRowsY = y;
        layout.readingRows = countReadingRows(controls);
        if (layout.readingRows > 0)
            y += layout.readingRows * kTextRowHeight;
        else
            y += kTextRowHeight; // hint text line

        y += 12.0f;
        layout.readingsDividerY = y;
        y += 9.0f;

        // Camera group.
        layout.cameraHeaderY = y;
        y += 18.0f;

        layout.orbitLeft = buttonAt(kContentX, y, 44.0f, 24.0f);
        layout.orbitRight = buttonAt(
            kContentX + 44.0f + kColumnGap,
            y,
            44.0f,
            24.0f);
        y += 24.0f + 6.0f;

        layout.hint1Y = y;
        y += 13.0f;
        layout.hint2Y = y;
        y += 13.0f + 6.0f;

        layout.panel =
            Rectangle{kPanelX, kPanelY, kPanelWidth, y - kPanelY};
        return layout;
    }

    void drawDivider(float y)
    {
        DrawLine(
            (int)(kPanelX + 8.0f),
            (int)y,
            (int)(kPanelX + kPanelWidth - 8.0f),
            (int)y,
            cardBorder);
    }

    void drawButton(
        const Rectangle& button,
        const char* label,
        bool selected)
    {
        bool hovered =
            CheckCollisionPointRec(GetMousePosition(), button);
        bool held = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        Color fill = inactiveButton;
        Color textColor = headingColor;

        if (selected)
        {
            fill = accent;
            textColor = WHITE;

            if (hovered) fill = accentBright;
            if (held) fill = accentDark;
        }
        else
        {
            if (hovered) fill = inactiveHover;
            if (held) fill = inactivePressed;
        }

        DrawRectangleRounded(button, 0.22f, 4, fill);
        DrawRectangleRoundedLines(button, 0.22f, 4, Fade(DARKGRAY, 0.35f));

        Vector2 textSize =
            MeasureTextEx(GetFontDefault(), label, 11.0f, 1.0f);

        drawText(
            label,
            button.x + (button.width - textSize.x) * 0.5f,
            button.y + (button.height - textSize.y) * 0.5f,
            11.0f,
            textColor);
    }

    void drawStatRow(float y, const char* label, const char* value)
    {
        drawText(label, kContentX, y, 10.0f, mutedText);

        float valueWidth =
            MeasureTextEx(GetFontDefault(), value, 10.0f, 1.0f).x;

        drawText(
            value,
            kContentX + kContentWidth - valueWidth,
            y,
            10.0f,
            accentDark);
    }

    void drawReadingsRows(const Hud& hud, HudLayout& layout)
    {
        char buffer[96];
        float rowY = layout.readingsRowsY;

        if (hud.controls.showParticleReadings)
        {
            snprintf(buffer, sizeof(buffer), "%d", hud.stats.particleCount);
            drawStatRow(rowY, "Particles", buffer);
            rowY += kTextRowHeight;
        }

        if (hud.controls.showDensityReadings)
        {
            snprintf(
                buffer, sizeof(buffer), "%.2f x rest",
                hud.stats.density.minRatio);
            drawStatRow(rowY, "Density min", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.2f x rest",
                hud.stats.density.avgRatio);
            drawStatRow(rowY, "Density avg", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.2f x rest",
                hud.stats.density.maxRatio);
            drawStatRow(rowY, "Density max", buffer);
            rowY += kTextRowHeight;
        }

        if (hud.controls.showPressureReadings)
        {
            snprintf(
                buffer, sizeof(buffer), "%.0f Pa",
                hud.stats.pressure.minPressure);
            drawStatRow(rowY, "Pressure min", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.0f Pa",
                hud.stats.pressure.avgPressure);
            drawStatRow(rowY, "Pressure avg", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.0f Pa",
                hud.stats.pressure.maxPressure);
            drawStatRow(rowY, "Pressure max", buffer);
            rowY += kTextRowHeight;
        }

        if (hud.controls.showTemperatureReadings)
        {
            snprintf(
                buffer, sizeof(buffer), "%.1f C",
                hud.stats.temperature.minTemperature);
            drawStatRow(rowY, "Temp min", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.1f C",
                hud.stats.temperature.avgTemperature);
            drawStatRow(rowY, "Temp avg", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.1f C",
                hud.stats.temperature.maxTemperature);
            drawStatRow(rowY, "Temp max", buffer);
            rowY += kTextRowHeight;
        }

        if (hud.controls.showPerformanceReadings)
        {
            snprintf(buffer, sizeof(buffer), "%d", hud.stats.fps);
            drawStatRow(rowY, "FPS", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.1f ms",
                hud.stats.physicsMilliseconds);
            drawStatRow(rowY, "Physics ms", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.1f ms",
                hud.stats.renderingMilliseconds);
            drawStatRow(rowY, "Render ms", buffer);
            rowY += kTextRowHeight;

            snprintf(
                buffer, sizeof(buffer), "%.1f s",
                hud.stats.simulationSeconds);
            drawStatRow(rowY, "Sim time", buffer);
            rowY += kTextRowHeight;
        }
    }
}

bool hudPointerOverPanel(const Hud& hud)
{
    HudLayout layout = computeLayout(hud.controls);
    return CheckCollisionPointRec(GetMousePosition(), layout.panel);
}

HudMouseInput readHudMouseInput(const Hud& hud)
{
    HudMouseInput input;
    HudLayout layout = computeLayout(hud.controls);
    Vector2 mouse = GetMousePosition();

    input.pointerOverPanel =
        CheckCollisionPointRec(mouse, layout.panel);

    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    input.densityPressed =
        clicked && CheckCollisionPointRec(mouse, layout.displayButtons[0]);
    input.pressurePressed =
        clicked && CheckCollisionPointRec(mouse, layout.displayButtons[1]);
    input.temperaturePressed =
        clicked && CheckCollisionPointRec(mouse, layout.displayButtons[2]);
    input.heatingPressed =
        clicked && CheckCollisionPointRec(mouse, layout.displayButtons[3]);

    input.particleReadingsPressed =
        clicked && CheckCollisionPointRec(mouse, layout.readingButtons[0]);
    input.densityReadingsPressed =
        clicked && CheckCollisionPointRec(mouse, layout.readingButtons[1]);
    input.pressureReadingsPressed =
        clicked && CheckCollisionPointRec(mouse, layout.readingButtons[2]);
    input.temperatureReadingsPressed =
        clicked && CheckCollisionPointRec(mouse, layout.readingButtons[3]);
    input.performanceReadingsPressed =
        clicked && CheckCollisionPointRec(mouse, layout.readingButtons[4]);

    bool held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.orbitLeftHeld =
        held && CheckCollisionPointRec(mouse, layout.orbitLeft);
    input.orbitRightHeld =
        held && CheckCollisionPointRec(mouse, layout.orbitRight);

    return input;
}

void drawHud(const Hud& hud)
{
    HudLayout layout = computeLayout(hud.controls);

    // Panel background separates the controls from the simulation.
    DrawRectangleRounded(layout.panel, 0.06f, 6, panelBackground);
    DrawRectangleRoundedLines(layout.panel, 0.06f, 6, cardBorder);

    // ---------------- Display modes ----------------
    drawCardHeader("Display modes", layout.displayHeaderY);

    drawButton(
        layout.displayButtons[0],
        "Density",
        hud.controls.showDensityColors);
    drawButton(
        layout.displayButtons[1],
        "Pressure",
        hud.controls.showPressureColors);
    drawButton(
        layout.displayButtons[2],
        "Temperature",
        hud.controls.showTemperatureColors);
    drawButton(
        layout.displayButtons[3],
        "Heating",
        hud.controls.heatingEnabled);

    drawDivider(layout.displayDividerY);

    // ---------------- Readings ----------------
    drawCardHeader("Readings", layout.readingsHeaderY);

    drawButton(
        layout.readingButtons[0],
        "Particles",
        hud.controls.showParticleReadings);
    drawButton(
        layout.readingButtons[1],
        "Density",
        hud.controls.showDensityReadings);
    drawButton(
        layout.readingButtons[2],
        "Pressure",
        hud.controls.showPressureReadings);
    drawButton(
        layout.readingButtons[3],
        "Temperature",
        hud.controls.showTemperatureReadings);
    drawButton(
        layout.readingButtons[4],
        "Performance",
        hud.controls.showPerformanceReadings);

    if (layout.readingRows > 0)
    {
        drawReadingsRows(hud, layout);
    }
    else
    {
        drawText(
            "Nothing selected",
            kContentX,
            layout.readingsRowsY,
            10.0f,
            mutedText);
    }

    drawDivider(layout.readingsDividerY);

    // ---------------- Camera ----------------
    drawCardHeader("Camera", layout.cameraHeaderY);

    drawButton(layout.orbitLeft, "<", false);
    drawButton(layout.orbitRight, ">", false);

    drawText(
        "Drag or arrows to orbit",
        kContentX,
        layout.hint1Y,
        10.0f,
        mutedText);
    drawText(
        "Scroll wheel to zoom",
        kContentX,
        layout.hint2Y,
        10.0f,
        mutedText);
}
