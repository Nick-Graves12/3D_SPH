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
    bool showPressureColors = false;

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
    
    Shader fluidDepthShader = 
    LoadShader(
        "fluid_depth_instanced.vs",
        "fluid_depth.fs");

    Shader fluidThicknessShader =
    LoadShader(
        "fluid_depth_instanced.vs",
        "fluid_thickness.fs");

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
        LoadShader("fluid_blur.vs", "fluid_blur.fs");

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
        LoadShader("fluid_surface.vs", "fluid_surface.fs");

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
        if (IsKeyPressed(KEY_D))
        {
            showDensityColors = !showDensityColors;

            if (showDensityColors)
            {
                showPressureColors = false;
            }
        }
        if (IsKeyPressed(KEY_P))
        {
            showPressureColors = !showPressureColors;

            if (showPressureColors)
            {
                showDensityColors = false;
            }
        }
        float frameTime = GetFrameTime();
        frameTime = std::min(frameTime, 0.1f);
        physicsAccumulator += frameTime;
        diagnosticFrameCounter++;

        updateCameraControls(camera, frameTime);

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
            showPressureColors,
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
            thicknessTextureLocation);

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

                float minimumPressure = state.particles.front().pressure;
                float maximumPressure = minimumPressure;

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
                }

                double averageQ =
                    sumQ / static_cast<double>(state.particles.size());

                std::cout
                    << "density ratio: min=" << minimumQ
                    << ", average=" << averageQ
                    << ", max=" << maximumQ
                    << '\n';
                std::cout
                    << "pressure range: min=" << minimumPressure
                    << ", max=" << maximumPressure
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

    UnloadRenderTexture(thicknessTarget);

    fluidThicknessMaterial.shader = Shader{};
    UnloadMaterial(fluidThicknessMaterial);

    UnloadShader(fluidThicknessShader);

    UnloadShader(fluidSurfaceShader);

    UnloadRenderTexture(sceneTarget);

    CloseWindow();

    return 0;
}