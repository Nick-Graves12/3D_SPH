#include "raylib.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include "Vec3.h"
#include "SimulationTypes.h"
#include "SpatialGrid.h"
#include "FluidSimulation.h"
#include "Rendering.h"
#include "StartupTests.h"

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

    bool showDensityColors = false;

    InitWindow(windowWidth, windowHeight, "SPH");
    SetTargetFPS(60);

    RenderTexture2D fluidTarget =
        LoadRenderTexture(windowWidth, windowHeight);

    RenderTexture2D blurTarget =
        LoadRenderTexture(windowWidth, windowHeight);

    Shader lightingShader = LoadShader(
        "lighting.vs",
        "lighting.fs");
    
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
        "lighting_instanced.vs",
        "lighting.fs");
    
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
    
    Shader fluidDepthShader = LoadShader(
        "fluid_depth_instanced.vs",
        "fluid_depth.fs");
    
    fluidDepthShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(
            fluidDepthShader,
            "instanceTransform");
    
    int maximumDepthLocation =
    GetShaderLocation(
        fluidDepthShader,
        "maximumDepth");

    float maximumDepth = 30.0f;

    Material fluidDepthMaterial =
    LoadMaterialDefault();

    fluidDepthMaterial.shader =
        fluidDepthShader;

    SetShaderValue(
        fluidDepthShader,
        maximumDepthLocation,
        &maximumDepth,
        SHADER_UNIFORM_FLOAT);

    Shader fluidBlurShader =
        LoadShader("fluid_blur.vs", "fluid_blur.fs");

    int texelDirectionLocation =
        GetShaderLocation(fluidBlurShader, "texelDirection");

    int depthFalloffLocation =
        GetShaderLocation(fluidBlurShader, "depthFalloff");

    float depthFalloff = 20.0f;

    SetShaderValue(
        fluidBlurShader,
        depthFalloffLocation,
        &depthFalloff,
        SHADER_UNIFORM_FLOAT);

    Shader fluidSurfaceShader =
        LoadShader("fluid_surface.vs", "fluid_surface.fs");

    int texelSizeLocation =
        GetShaderLocation(fluidSurfaceShader, "texelSize");

    int normalStrengthLocation =
        GetShaderLocation(fluidSurfaceShader, "normalStrength");
    
    float texelSize[2]{
        1.0f / static_cast<float>(windowWidth),
        1.0f / static_cast<float>(windowHeight)
    };

    float normalStrength = 40.0f;

    SetShaderValue(
        fluidSurfaceShader,
        texelSizeLocation,
        texelSize,
        SHADER_UNIFORM_VEC2);

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
            
    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_D))
        {
            showDensityColors = !showDensityColors;
        }
        float frameTime = GetFrameTime();
        frameTime = std::min(frameTime, 0.1f);
        physicsAccumulator += frameTime;
        diagnosticFrameCounter++;

        updateCameraControls(camera, frameTime);

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
            showDensityColors,
            particleModel,
            instancedParticleMaterial,
            particleTransforms,
            fluidTarget,
            fluidDepthMaterial,
            blurTarget,
            fluidBlurShader,
            texelDirectionLocation,
            fluidSurfaceShader);

        double renderingMilliseconds =
            (GetTime() - renderingStart) * 1000.0;

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

                for (const FluidParticle& particle : state.particles)
                {
                    float q = particle.density / config.restDensity;

                    minimumQ = std::min(minimumQ, q);
                    maximumQ = std::max(maximumQ, q);
                    sumQ += q;
                }

                double averageQ =
                    sumQ / static_cast<double>(state.particles.size());

                std::cout
                    << "density ratio: min=" << minimumQ
                    << ", average=" << averageQ
                    << ", max=" << maximumQ
                    << '\n';
            }
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

    UnloadShader(fluidSurfaceShader);

    CloseWindow();

    return 0;
}