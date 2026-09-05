#include "raylib.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include "math/Vec3.h"
#include "SimulationTypes.h"
#include "physics/SpatialGrid.h"
#include "physics/FluidSimulation.h"
#include "render/Rendering.h"
#include "render/Hud.h"
#include "physics/StartupTests.h"

int main ()
{
    int windowWidth = 800;
    int windowHeight = 600;

    Camera3D camera{};
    camera.position = {0, -10, 3};
    camera.up = {0, 0, 1};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BoundingBox tank{};
    tank.min = {-5.0f, -2.0f, 0.0f};
    tank.max = {5.0f, 8.0f, 6.0f};

    SimulationState state{};

    state.emitter = {
        {-5.0f, 3.0f, 3.0f},
        {1.0f, 0.0f, 0.0f},
        0.6f,
        5.0f,
        0.0f
    };

    const SimulationConfig config =
        createDefaultConfig(state.emitter);

    std::vector<Matrix> particleTransforms;
    particleTransforms.reserve(config.maxParticleCount);

    state.grid =
        createUniformGrid(tank, config.smoothingRadius);

    runStartupTests(state.grid, tank);

    camera.target = {
        (tank.min.x + tank.max.x) * 0.5f,
        (tank.min.y + tank.max.y) * 0.5f,
        (tank.min.z + tank.max.z) * 0.5f
    };

    const Vec3 gravity{0.0f, 0.0f, -9.8f};

    int diagnosticFrameCounter = 0;

    const int maxSubstepsPerFrame = 4;
    float physicsAccumulator = 0.0f;

    // Accumulated simulated time, reported by the HUD performance readings.
    double simulationElapsed = 0.0;

    Hud hudState{};

    // Toggle helpers shared by the keyboard (D/P/T/H) and the HUD buttons so
    // both input paths always agree with each other.
    auto enableDensityColors = [&]()
    {
        hudState.controls.showDensityColors = true;
        hudState.controls.showPressureColors = false;
        hudState.controls.showTemperatureColors = false;
    };

    auto disableDensityColors = [&]()
    {
        hudState.controls.showDensityColors = false;
    };

    auto enablePressureColors = [&]()
    {
        hudState.controls.showPressureColors = true;
        hudState.controls.showDensityColors = false;
        hudState.controls.showTemperatureColors = false;
    };

    auto disablePressureColors = [&]()
    {
        hudState.controls.showPressureColors = false;
    };

    auto enableTemperatureColors = [&]()
    {
        hudState.controls.showTemperatureColors = true;
        hudState.controls.showDensityColors = false;
        hudState.controls.showPressureColors = false;
    };

    auto disableTemperatureColors = [&]()
    {
        hudState.controls.showTemperatureColors = false;
    };

    auto toggleHeating = [&]()
    {
        state.heatingEnabled = !state.heatingEnabled;
        hudState.controls.heatingEnabled = state.heatingEnabled;

        // Turning heating off reverts to the original cold solver; clear any
        // leftover heat field so colors reflect the cold version honestly.
        if (!state.heatingEnabled)
        {
            for (FluidParticle& particle : state.particles)
            {
                particle.temperature = config.referenceTemperature;
            }
        }

        std::cout
            << "[heating "
            << (state.heatingEnabled ? "ON" : "OFF")
            << "]\n";
    };

    InitWindow(windowWidth, windowHeight, "SPH");
    SetTargetFPS(60);

    RenderTexture2D fluidTarget =
        loadFloatRenderTexture(windowWidth, windowHeight);

    RenderTexture2D blurTarget =
        loadFloatRenderTexture(windowWidth, windowHeight);

    RenderTexture2D thicknessTarget =
        loadFloatRenderTexture(windowWidth, windowHeight);
    
    RenderTexture2D sceneTarget =
        LoadRenderTexture(windowWidth, windowHeight);

    Shader lightingShader = LoadShader(
        "shaders/lighting.vs",
        "shaders/lighting.fs");
    
    int lightDirectionLocation =
        GetShaderLocation(lightingShader, "lightDirection");

    float lightDirection[3] = {
        -0.4f,
        -0.6f,
        1.0f
    };

    SetShaderValue(
        lightingShader,
        lightDirectionLocation,
        lightDirection,
        SHADER_UNIFORM_VEC3);
    
    Shader instancedLightingShader = LoadShader(
        "shaders/lighting_instanced.vs",
        "shaders/lighting.fs");
    
    instancedLightingShader
    .locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(
            instancedLightingShader,
            "instanceTransform");
    
    int instancedLightDirectionLocation =
        GetShaderLocation(
            instancedLightingShader,
            "lightDirection");

    SetShaderValue(
        instancedLightingShader,
        instancedLightDirectionLocation,
        lightDirection,
        SHADER_UNIFORM_VEC3);

    Mesh particleMesh = GenMeshSphere(
        1.0f,
        8,
        8);

    Model particleModel =
        LoadModelFromMesh(particleMesh);

    particleModel.materials[0].shader =
        lightingShader;    

    Material instancedParticleMaterial =
        LoadMaterialDefault();

    instancedParticleMaterial.shader =
        instancedLightingShader;

    instancedParticleMaterial
        .maps[MATERIAL_MAP_DIFFUSE]
        .color = Color{20, 120, 230, 255};
    
    Shader fluidDepthShader = 
    LoadShader(
        "shaders/fluid_depth_instanced.vs",
        "shaders/fluid_depth.fs");

    Shader fluidThicknessShader =
    LoadShader(
        "shaders/fluid_depth_instanced.vs",
        "shaders/fluid_thickness.fs");

    fluidThicknessShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(
            fluidThicknessShader,
            "instanceTransform");

    fluidThicknessShader.locs[SHADER_LOC_VERTEX_NORMAL] =
        GetShaderLocationAttrib(
            fluidThicknessShader,
            "vertexNormal");
    
    fluidDepthShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(
            fluidDepthShader,
            "instanceTransform");
    
    fluidDepthShader.locs[SHADER_LOC_VERTEX_NORMAL] =
        GetShaderLocationAttrib(
            fluidDepthShader,
            "vertexNormal");
    
    int maximumDepthLocation =
    GetShaderLocation(
        fluidDepthShader,
        "maximumDepth");

    int cameraPositionLocation =
    GetShaderLocation(
        fluidDepthShader,
        "cameraPosition");

    float maximumDepth = 22.0f;

    Material fluidDepthMaterial =
    LoadMaterialDefault();

    fluidDepthMaterial.shader =
        fluidDepthShader;

    Material fluidThicknessMaterial =
        LoadMaterialDefault();

    fluidThicknessMaterial.shader =
        fluidThicknessShader;

    SetShaderValue(
        fluidDepthShader,
        maximumDepthLocation,
        &maximumDepth,
        SHADER_UNIFORM_FLOAT);

    Shader fluidBlurShader =
        LoadShader("shaders/fluid_blur.vs", "shaders/fluid_blur.fs");

    int texelDirectionLocation =
        GetShaderLocation(fluidBlurShader, "texelDirection");

    int depthFalloffLocation =
        GetShaderLocation(fluidBlurShader, "depthFalloff");

    float depthFalloff = 12.0f;

    SetShaderValue(
        fluidBlurShader,
        depthFalloffLocation,
        &depthFalloff,
        SHADER_UNIFORM_FLOAT);

    Shader fluidSurfaceShader =
        LoadShader("shaders/fluid_surface.vs", "shaders/fluid_surface.fs");

    int texelSizeLocation =
        GetShaderLocation(fluidSurfaceShader, "texelSize");

    int normalStrengthLocation =
        GetShaderLocation(fluidSurfaceShader, "normalStrength");
    
    float texelSize[2]{
        1.0f / static_cast<float>(windowWidth),
        1.0f / static_cast<float>(windowHeight)
    };

    float normalStrength = 25.0f;

    SetShaderValue(
        fluidSurfaceShader,
        texelSizeLocation,
        texelSize,
        SHADER_UNIFORM_VEC2);
    
    int sceneTextureLocation =
        GetShaderLocation(fluidSurfaceShader, "sceneTexture");

    int thicknessTextureLocation =
        GetShaderLocation(
            fluidSurfaceShader,
            "thicknessTexture");

    int refractionStrengthLocation =
        GetShaderLocation(fluidSurfaceShader, "refractionStrength");
    
    float refractionStrength = 0.02f;

    SetShaderValue(
        fluidSurfaceShader,
        refractionStrengthLocation,
        &refractionStrength,
        SHADER_UNIFORM_FLOAT);

    SetShaderValue(
        fluidSurfaceShader,
        normalStrengthLocation,
        &normalStrength,
        SHADER_UNIFORM_FLOAT);

    int fluidColorLocation =
        GetShaderLocation(fluidSurfaceShader, "fluidColor");

    int fluidLightDirectionLocation =
        GetShaderLocation(fluidSurfaceShader, "lightDirection");
    
    float fluidColor[3]{
        0.02f,
        0.30f,
        0.85f
    };

    float fluidLightDirection[3]{
        -0.4f,
        -0.6f,
        1.0f
    };

    SetShaderValue(
        fluidSurfaceShader,
        fluidColorLocation,
        fluidColor,
        SHADER_UNIFORM_VEC3);

    SetShaderValue(
        fluidSurfaceShader,
        fluidLightDirectionLocation,
        fluidLightDirection,
        SHADER_UNIFORM_VEC3);

    SetTextureFilter(fluidTarget.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(blurTarget.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(sceneTarget.texture, TEXTURE_FILTER_BILINEAR);  
            
    while (!WindowShouldClose())
    {
        // Keyboard toggles: D = density, P = pressure, T = temperature.
        if (IsKeyPressed(KEY_D))
        {
            if (hudState.controls.showDensityColors)
            {
                disableDensityColors();
            }
            else
            {
                enableDensityColors();
            }
        }
        if (IsKeyPressed(KEY_P))
        {
            if (hudState.controls.showPressureColors)
            {
                disablePressureColors();
            }
            else
            {
                enablePressureColors();
            }
        }
        if (IsKeyPressed(KEY_T))
        {
            if (hudState.controls.showTemperatureColors)
            {
                disableTemperatureColors();
            }
            else
            {
                enableTemperatureColors();
            }
        }
        if (IsKeyPressed(KEY_H))
        {
            toggleHeating();
        }

        // HUD buttons drive the same toggles as the keyboard.
        HudMouseInput hudMouse = readHudMouseInput(hudState);

        if (hudMouse.densityPressed)
        {
            if (hudState.controls.showDensityColors)
            {
                disableDensityColors();
            }
            else
            {
                enableDensityColors();
            }
        }
        if (hudMouse.pressurePressed)
        {
            if (hudState.controls.showPressureColors)
            {
                disablePressureColors();
            }
            else
            {
                enablePressureColors();
            }
        }
        if (hudMouse.temperaturePressed)
        {
            if (hudState.controls.showTemperatureColors)
            {
                disableTemperatureColors();
            }
            else
            {
                enableTemperatureColors();
            }
        }
        if (hudMouse.heatingPressed)
        {
            toggleHeating();
        }

        // Readings-group buttons toggle their numeric readouts. Unlike the
        // display modes, several readings groups can be open at once.
        if (hudMouse.particleReadingsPressed)
        {
            hudState.controls.showParticleReadings =
                !hudState.controls.showParticleReadings;
        }
        if (hudMouse.densityReadingsPressed)
        {
            hudState.controls.showDensityReadings =
                !hudState.controls.showDensityReadings;
        }
        if (hudMouse.pressureReadingsPressed)
        {
            hudState.controls.showPressureReadings =
                !hudState.controls.showPressureReadings;
        }
        if (hudMouse.temperatureReadingsPressed)
        {
            hudState.controls.showTemperatureReadings =
                !hudState.controls.showTemperatureReadings;
        }
        if (hudMouse.performanceReadingsPressed)
        {
            hudState.controls.showPerformanceReadings =
                !hudState.controls.showPerformanceReadings;
        }

        // Held < / > buttons orbit the camera while the button is down.
        int cameraButtonDirection = 0;
        if (hudMouse.orbitLeftHeld) cameraButtonDirection -= 1;
        if (hudMouse.orbitRightHeld) cameraButtonDirection += 1;

        float frameTime = GetFrameTime();
        frameTime = std::min(frameTime, 0.1f);
        physicsAccumulator += frameTime;
        diagnosticFrameCounter++;

        updateCameraControls(
            camera,
            frameTime,
            cameraButtonDirection,
            hudMouse.pointerOverPanel);

        float cameraPosition[3]{
            camera.position.x,
            camera.position.y,
            camera.position.z
        };

        SetShaderValue(
            fluidDepthShader,
            cameraPositionLocation,
            cameraPosition,
            SHADER_UNIFORM_VEC3);

        int substepCount = 0;

        double physicsStart = GetTime();

        PhysicsTimings physicsTiming{};

        while (physicsAccumulator >= config.fixedDeltaTime &&
                substepCount < maxSubstepsPerFrame)
        {
            simulateStep(state, tank, gravity, config, physicsTiming);

            physicsAccumulator -= config.fixedDeltaTime;
            substepCount++;
        }
        simulationElapsed +=
            static_cast<double>(substepCount) * config.fixedDeltaTime;

        double physicsMilliseconds =
            (GetTime() - physicsStart) * 1000.0;
        if (substepCount == maxSubstepsPerFrame &&
            physicsAccumulator >= config.fixedDeltaTime)
        {
            physicsAccumulator =
                std::fmod(physicsAccumulator, config.fixedDeltaTime);
        }
        double renderingStart = GetTime();
        
        renderScene(
            camera,
            tank,
            state.emitter,
            state.particles,
            config.particleRadius,
            config.restDensity,
            particleModel,
            instancedParticleMaterial,
            particleTransforms,
            sceneTarget,
            fluidTarget,
            fluidDepthMaterial,
            blurTarget,
            fluidBlurShader,
            texelDirectionLocation,
            fluidSurfaceShader,
            sceneTextureLocation,
            thicknessTarget,
            fluidThicknessMaterial,
            thicknessTextureLocation,
            hudState);

        double renderingMilliseconds =
            (GetTime() - renderingStart) * 1000.0;

        // Refresh the HUD readings every frame. The particle-field stats and
        // physics time are current; the render time shown is the frame that
        // was just composited (displayed from the next frame on).
        hudState.stats.particleCount =
            static_cast<int>(state.particles.size());
        hudState.stats.fps = GetFPS();
        hudState.stats.physicsMilliseconds =
            static_cast<float>(physicsMilliseconds);
        hudState.stats.renderingMilliseconds =
            static_cast<float>(renderingMilliseconds);
        hudState.stats.simulationSeconds =
            static_cast<float>(simulationElapsed);

        if (state.particles.empty())
        {
            hudState.stats.density = DensityReadings{};
            hudState.stats.pressure = PressureReadings{};
            hudState.stats.temperature = TemperatureReadings{};
        }
        else
        {
            const FluidParticle& first = state.particles.front();

            float minimumQ = first.density / config.restDensity;
            float maximumQ = minimumQ;
            double sumQ = 0.0;

            float minimumPressure = first.pressure;
            float maximumPressure = minimumPressure;
            double sumPressure = 0.0;

            float minimumTemperature = first.temperature;
            float maximumTemperature = minimumTemperature;
            double sumTemperature = 0.0;

            for (const FluidParticle& particle : state.particles)
            {
                float q = particle.density / config.restDensity;

                minimumQ = std::min(minimumQ, q);
                maximumQ = std::max(maximumQ, q);
                sumQ += q;

                minimumPressure =
                    std::min(minimumPressure, particle.pressure);
                maximumPressure =
                    std::max(maximumPressure, particle.pressure);
                sumPressure += particle.pressure;

                minimumTemperature =
                    std::min(minimumTemperature, particle.temperature);
                maximumTemperature =
                    std::max(maximumTemperature, particle.temperature);
                sumTemperature += particle.temperature;
            }

            double count = static_cast<double>(state.particles.size());

            hudState.stats.density.minRatio = minimumQ;
            hudState.stats.density.avgRatio =
                static_cast<float>(sumQ / count);
            hudState.stats.density.maxRatio = maximumQ;

            hudState.stats.pressure.minPressure = minimumPressure;
            hudState.stats.pressure.avgPressure =
                static_cast<float>(sumPressure / count);
            hudState.stats.pressure.maxPressure = maximumPressure;

            hudState.stats.temperature.minTemperature = minimumTemperature;
            hudState.stats.temperature.avgTemperature =
                static_cast<float>(sumTemperature / count);
            hudState.stats.temperature.maxTemperature = maximumTemperature;
        }

        if (diagnosticFrameCounter % 60 == 0)
        {
            std::cout
                << "particles: " << state.particles.size()
                << ", frameTime: " << frameTime
                << ", FPS: " << GetFPS()
                << ", physicsMs: " << physicsMilliseconds
                << ", renderingMs: " << renderingMilliseconds
                << '\n';
            std::cout
                << "stages: emitter=" << physicsTiming.emitterMs
                << ", grid=" << physicsTiming.gridMs
                << ", density=" << physicsTiming.densityMs
                << ", pressure=" << physicsTiming.pressureMs
                << ", internalForces=" << physicsTiming.internalForcesMs
                << ", integration=" << physicsTiming.integrationMs
                << ", collisions=" << physicsTiming.collisionsMs
                << '\n';
            
            if (!state.particles.empty())
            {
                float minimumQ =
                    state.particles.front().density / config.restDensity;
                float maximumQ = minimumQ;
                double sumQ = 0.0;

                float minimumPressure = state.particles.front().pressure;
                float maximumPressure = minimumPressure;

                float minimumTemperature =
                    state.particles.front().temperature;
                float maximumTemperature = minimumTemperature;
                double sumTemperature = 0.0;

                for (const FluidParticle& particle : state.particles)
                {
                    float q = particle.density / config.restDensity;

                    minimumQ = std::min(minimumQ, q);
                    maximumQ = std::max(maximumQ, q);
                    sumQ += q;

                    minimumPressure =
                        std::min(minimumPressure, particle.pressure);

                    maximumPressure =
                        std::max(maximumPressure, particle.pressure);

                    minimumTemperature =
                        std::min(minimumTemperature, particle.temperature);

                    maximumTemperature =
                        std::max(maximumTemperature, particle.temperature);

                    sumTemperature += particle.temperature;
                }

                double averageQ =
                    sumQ / static_cast<double>(state.particles.size());

                double averageTemperature =
                    sumTemperature /
                    static_cast<double>(state.particles.size());

                std::cout
                    << "density ratio: min=" << minimumQ
                    << ", average=" << averageQ
                    << ", max=" << maximumQ
                    << '\n';
                std::cout
                    << "pressure range: min=" << minimumPressure
                    << ", max=" << maximumPressure
                    << '\n';
                std::cout
                    << "temperature: min=" << minimumTemperature
                    << ", average=" << averageTemperature
                    << ", max=" << maximumTemperature
                    << '\n';
            }
            ConservationQuantities quantities =
                computeConservationQuantities(
                    state.particles,
                    config.particleMass,
                    length(gravity));

            std::cout
                << "conservation: mass=" << quantities.mass
                << ", momentum=(" << quantities.momentum.x
                << ", " << quantities.momentum.y
                << ", " << quantities.momentum.z << ")"
                << ", KE=" << quantities.kineticEnergy
                << ", PE=" << quantities.potentialEnergy
                << ", E=" << (quantities.kineticEnergy + quantities.potentialEnergy)
                << '\n';

            // Hydrostatic check: at rest, ratio ~1.0 means the settled fluid
            // is consistent with its own EOS. Ignore it during filling -- h
            // is dominated by the jet, not the pool. With default parameters
            // a settled ratio around 1.5 is the expected clamped-EOS pressure
            // bias (see README "Hydrostatic Check"), not a bug.
            HydrostaticCheck hydrostatic =
                computeHydrostaticCheck(
                    state.particles,
                    config.restDensity,
                    config.stiffness,
                    length(gravity));

            std::cout
                << "hydrostatic: h=" << hydrostatic.fluidHeight
                << ", predicted=" << hydrostatic.predictedBottomPressure
                << ", measured=" << hydrostatic.measuredBottomPressure
                << ", ratio=" << hydrostatic.ratio
                << "\n\n";
        }
    }
    UnloadRenderTexture(fluidTarget);

    particleModel.materials[0].shader = Shader{};
    UnloadModel(particleModel);
    UnloadShader(lightingShader);

    instancedParticleMaterial.shader = Shader{};
    UnloadMaterial(instancedParticleMaterial);
    UnloadShader(instancedLightingShader);

    fluidDepthMaterial.shader = Shader{};
    UnloadMaterial(fluidDepthMaterial);
    UnloadShader(fluidDepthShader);

    UnloadRenderTexture(blurTarget);

    UnloadShader(fluidBlurShader);

    UnloadRenderTexture(thicknessTarget);

    fluidThicknessMaterial.shader = Shader{};
    UnloadMaterial(fluidThicknessMaterial);

    UnloadShader(fluidThicknessShader);

    UnloadShader(fluidSurfaceShader);

    UnloadRenderTexture(sceneTarget);

    CloseWindow();

    return 0;
}