#pragma once

#include "../SimulationTypes.h"
#include "Hud.h"
#include "raylib.h"

void updateCameraControls(
    Camera3D& camera,
    float deltaTime,
    int buttonOrbitDirection,
    bool pointerOverPanel);

void renderScene(
    const Camera3D& camera,
    const BoundingBox& tank,
    const Emitter& emitter,
    const std::vector<FluidParticle>& particles,
    float particleRadius,
    float restDensity,
    const Model& particleModel,
    const Material& instancedParticleMaterial,
    std::vector<Matrix>& particleTransforms,
    const RenderTexture2D& sceneTarget,
    const RenderTexture2D& fluidTarget,
    const Material& fluidDepthMaterial,
    const RenderTexture2D& blurTarget,
    const Shader& fluidBlurShader,
    int texelDirectionLocation,
    const Shader& fluidSurfaceShader,
    int sceneTextureLocation,
    const RenderTexture2D& thicknessTarget,
    const Material& fluidThicknessMaterial,
    int thicknessTextureLocation,
    const Hud& hud);

void drawParticles(
    const std::vector<FluidParticle>& particles,
    float particleRadius,
    float restDensity,
    bool showDensityColors,
    bool showPressureColors,
    bool showTemperatureColors,
    const Model& particleModel,
    const Material& instancedParticleMaterial,
    std::vector<Matrix>& particleTransforms);

RenderTexture2D loadFloatRenderTexture(int width, int height);