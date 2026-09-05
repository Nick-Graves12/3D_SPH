#include "Rendering.h"
#include <cmath>
#include <algorithm>
#include "raymath.h"
#include "rlgl.h"

Vector3 toRaylib(const Vec3& value)
{
    return Vector3{value.x, value.y, value.z};
}

void orbitCameraYaw(Camera3D& camera, float angleRadians)
{
    float offsetX = camera.position.x - camera.target.x;
    float offsetY = camera.position.y - camera.target.y;
    float offsetZ = camera.position.z - camera.target.z;

    float cosine = std::cos(angleRadians);
    float sine = std::sin(angleRadians);

    float rotatedX = offsetX * cosine - offsetY * sine;
    float rotatedY = offsetX * sine + offsetY * cosine;

    camera.position = {
        camera.target.x + rotatedX,
        camera.target.y + rotatedY,
        camera.target.z + offsetZ
    };
}

void updateCameraControls(
    Camera3D& camera,
    float deltaTime,
    int buttonOrbitDirection,
    bool pointerOverPanel)
{
    const float orbitSpeed = 60.0f;
    float orbitAmount =
        orbitSpeed * DEG2RAD * deltaTime;

    // HUD < / > buttons contribute +/-1; arrow keys add on top so both
    // input paths work together.
    int direction = buttonOrbitDirection;

    if (IsKeyDown(KEY_RIGHT)) direction += 1;
    if (IsKeyDown(KEY_LEFT)) direction -= 1;

    if (direction != 0)
    {
        orbitCameraYaw(
            camera,
            orbitAmount * static_cast<float>(direction));
    }

    // Left-drag orbits, unless the cursor is over a HUD widget.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !pointerOverPanel)
    {
        orbitCameraYaw(camera, -GetMouseDelta().x * 0.005f);
    }

    // Scroll wheel zooms in/out (ignored over the HUD panel).
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && !pointerOverPanel)
    {
        Vector3 offset =
            Vector3Subtract(camera.position, camera.target);

        Vector3 newOffset =
            Vector3Scale(offset, 1.0f - wheel * 0.08f);

        float newDistance = Vector3Length(newOffset);
        const float minDistance = 5.0f;
        const float maxDistance = 35.0f;

        if (newDistance < minDistance)
        {
            newOffset = Vector3Scale(
                Vector3Normalize(offset),
                minDistance);
        }
        else if (newDistance > maxDistance)
        {
            newOffset = Vector3Scale(
                Vector3Normalize(offset),
                maxDistance);
        }

        camera.position =
            Vector3Add(camera.target, newOffset);
    }
}

Color densityToColor(float density, float restDensity)
{
    float q = density / restDensity;

    if (q < 1.0f)
    {
        float t = std::clamp((q - 0.8f) / 0.2f, 0.0f, 1.0f);

        unsigned char red =
            static_cast<unsigned char>(0);

        unsigned char green = 
            static_cast<unsigned char>(200.0f * (1.0f - t));

        unsigned char blue =
            static_cast<unsigned char>(255);

        return Color{red, green, blue, 255};
    }

    float t = std::clamp((q - 1.0f) / 0.25f, 0.0f, 1.0f);
   
    unsigned char red =
    static_cast<unsigned char>(255.0f * t);

    unsigned char green = 0;

    unsigned char blue =
        static_cast<unsigned char>(255.0f * (1.0f - t));
    
    return Color{red, green, blue, 255};
}

Color pressureToColor(float pressure, float pressureScale)
{
    float q = std::clamp(
        pressure / pressureScale,
        -1.0f,
        1.0f);

    if (q < 0.0f)
    {
        float t = -q;

        return Color{
            0,
            static_cast<unsigned char>(200.0f * (1.0f - t)),
            255,
            255
        };
    }

    float t = q;

    return Color{
        static_cast<unsigned char>(255.0f * t),
        0,
        static_cast<unsigned char>(255.0f * (1.0f - t)),
        255
    };
}

// Absolute heat scale matching the physics defaults:
// 0 = reference temperature, 100 = heater temperature.
// Cold: deep blue -> white-hot -> red at the plate.
Color temperatureToColor(float temperature,
    float minimumTemperature,
    float maximumTemperature)
{
    float t = std::clamp(
        (temperature - minimumTemperature) /
        (maximumTemperature - minimumTemperature),
        0.0f,
        1.0f);

    unsigned char red, green, blue;

    if (t < 0.5f)
    {
        // deep blue -> white
        float s = t / 0.5f;
        red   = static_cast<unsigned char>(40.0f + 215.0f * s);
        green = static_cast<unsigned char>(90.0f + 165.0f * s);
        blue  = 255;
    }
    else
    {
        // white -> red
        float s = (t - 0.5f) / 0.5f;
        red   = 255;
        green = static_cast<unsigned char>(255.0f * (1.0f - s));
        blue  = static_cast<unsigned char>(255.0f - 235.0f * s);
    }

    return Color{red, green, blue, 255};
}

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
    const Hud& hud)
{
    bool showDebugColors =
        hud.controls.showDensityColors ||
        hud.controls.showPressureColors ||
        hud.controls.showTemperatureColors;
    BeginTextureMode(fluidTarget);
    ClearBackground(Color{255, 255, 255, 0});
    BeginMode3D(camera);

    drawParticles(
        particles,
        particleRadius,
        restDensity,
        false,
        false,
        false,
        particleModel,
        fluidDepthMaterial,
        particleTransforms);

    EndMode3D();
    EndTextureMode();

    BeginTextureMode(thicknessTarget);
    ClearBackground(Color{0, 0, 0, 0});

    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthTest();
    BeginMode3D(camera);

    drawParticles(
        particles,
        particleRadius,
        restDensity,
        false,
        false,
        false,
        particleModel,
        fluidThicknessMaterial,
        particleTransforms);

    EndMode3D();
    rlEnableDepthTest();
    EndBlendMode();

    EndTextureMode();   

    float horizontalDirection[2]{
        1.0f / static_cast<float>(fluidTarget.texture.width),
        0.0f
    };
    SetShaderValue(
        fluidBlurShader,
        texelDirectionLocation,
        horizontalDirection,
        SHADER_UNIFORM_VEC2);

    BeginTextureMode(blurTarget);
    ClearBackground(Color{255, 255, 255, 0});
    BeginShaderMode(fluidBlurShader);

    Rectangle depthSource{
        0.0f,
        0.0f,
        static_cast<float>(fluidTarget.texture.width),
        -static_cast<float>(fluidTarget.texture.height)
    };

    DrawTextureRec(
        fluidTarget.texture,
        depthSource,
        Vector2{0.0f, 0.0f},
        WHITE);

    EndShaderMode();
    EndTextureMode();

    float verticalDirection[2]{
        0.0f,
        1.0f / static_cast<float>(blurTarget.texture.height)
    };

    SetShaderValue(
        fluidBlurShader,
        texelDirectionLocation,
        verticalDirection,
        SHADER_UNIFORM_VEC2);
    if (!showDebugColors)
    {
        BeginTextureMode(fluidTarget);
        ClearBackground(Color{255, 255, 255, 0});
        BeginShaderMode(fluidBlurShader);

        Rectangle blurSource{
            0.0f,
            0.0f,
            static_cast<float>(blurTarget.texture.width),
            -static_cast<float>(blurTarget.texture.height)
        };

        DrawTextureRec(
            blurTarget.texture,
            blurSource,
            Vector2{0.0f, 0.0f},
            WHITE);

        EndShaderMode();
        EndTextureMode();
        
        SetShaderValue(
            fluidBlurShader,
            texelDirectionLocation,
            horizontalDirection,
            SHADER_UNIFORM_VEC2);

        BeginTextureMode(blurTarget);
        ClearBackground(Color{255, 255, 255, 0});
        BeginShaderMode(fluidBlurShader);

        DrawTextureRec(
            fluidTarget.texture,
            depthSource,
            Vector2{0.0f, 0.0f},
            WHITE);

        EndShaderMode();
        EndTextureMode();

        SetShaderValue(
            fluidBlurShader,
            texelDirectionLocation,
            verticalDirection,
            SHADER_UNIFORM_VEC2);

        BeginTextureMode(fluidTarget);
        ClearBackground(Color{255, 255, 255, 0});
        BeginShaderMode(fluidBlurShader);

        DrawTextureRec(
            blurTarget.texture,
            blurSource,
            Vector2{0.0f, 0.0f},
            WHITE);

        EndShaderMode();
        EndTextureMode();
    }

    BeginTextureMode(sceneTarget);
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);

    DrawBoundingBox(tank, BLACK);

    float pipeLength = 0.4f;
    Vec3 backCenter{
        emitter.center.x - pipeLength,
        emitter.center.y,
        emitter.center.z
    };
    Vec3 frontCenter = emitter.center;
    DrawCylinderEx(
        toRaylib(backCenter),
        toRaylib(frontCenter),
        emitter.radius,
        emitter.radius,
        16,
        GRAY
    );
    DrawCircle3D(toRaylib(emitter.center),
            emitter.radius,
            Vector3{0.0f, 1.0f, 0.0f},
            90.0f,
            RED);

    if (showDebugColors)
    {
        drawParticles(
            particles,
            particleRadius,
            restDensity,
            hud.controls.showDensityColors,
            hud.controls.showPressureColors,
            hud.controls.showTemperatureColors,
            particleModel,
            instancedParticleMaterial,
            particleTransforms);
    }

    EndMode3D();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(RAYWHITE);

    Rectangle sceneSource{
        0.0f,
        0.0f,
        static_cast<float>(sceneTarget.texture.width),
        -static_cast<float>(sceneTarget.texture.height)
    };

    DrawTextureRec(
        sceneTarget.texture,
        sceneSource,
        Vector2{0.0f, 0.0f},
        WHITE
    );

    Rectangle source{
        0.0f,
        0.0f,
        static_cast<float>(fluidTarget.texture.width),
        -static_cast<float>(fluidTarget.texture.height)
    };

    if (!showDebugColors)
    {
        SetShaderValueTexture(
            fluidSurfaceShader,
            sceneTextureLocation,
            sceneTarget.texture);

        SetShaderValueTexture(
            fluidSurfaceShader,
            thicknessTextureLocation,
            thicknessTarget.texture);

        BeginShaderMode(fluidSurfaceShader);

        DrawTextureRec(
            fluidTarget.texture,
            source,
            Vector2{0.0f, 0.0f},
            WHITE);

        EndShaderMode();
    }

    drawHud(hud);

    EndDrawing();
}

void drawParticles(
    const std::vector<FluidParticle>& particles,
    float particleRadius,
    float restDensity,
    bool showDensityColors,
    bool showPressureColors,
    bool showTemperatureColors,
    const Model& particleModel,
    const Material& instancedParticleMaterial,
    std::vector<Matrix>& particleTransforms)
{
    const float renderRadius = particleRadius * 1.75f;
    particleTransforms.clear();

    for (const FluidParticle& particle : particles)
    {
        Matrix transform = MatrixIdentity();

        transform.m0 = renderRadius;
        transform.m5 = renderRadius;
        transform.m10 = renderRadius;

        transform.m12 = particle.position.x;
        transform.m13 = particle.position.y;
        transform.m14 = particle.position.z;

        particleTransforms.push_back(transform);
    }
    if (showDensityColors || showPressureColors || showTemperatureColors)
    {
        for (const auto& particle : particles)
        {
            Color particleColor;

            if (showDensityColors)
            {
                particleColor =
                    densityToColor(
                        particle.density,
                        restDensity);
            }
            else if (showPressureColors)
            {
                particleColor =
                    pressureToColor(
                        particle.pressure,
                        300.0f);
            }
            else
            {
                particleColor =
                    temperatureToColor(
                        particle.temperature,
                        0.0f,     // referenceTemperature
                        100.0f);  // heaterTemperature
            }

            DrawSphere(
                toRaylib(particle.position),
                renderRadius,
                particleColor);
        }
    }
    else if (!particleTransforms.empty())
    {
        DrawMeshInstanced(
            particleModel.meshes[0],
            instancedParticleMaterial,
            particleTransforms.data(),
            static_cast<int>(particleTransforms.size()));
    }
}

RenderTexture2D loadFloatRenderTexture(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    target.texture.id = rlLoadTexture(
        nullptr,
        width,
        height,
        RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
        1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.mipmaps = 1;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
    rlFramebufferAttach(
        target.id,
        target.texture.id,
        RL_ATTACHMENT_COLOR_CHANNEL0,
        RL_ATTACHMENT_TEXTURE2D,
        0);

    target.depth.id = rlLoadTextureDepth(width, height, true);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.mipmaps = 1;
    target.depth.format = 0;

    rlFramebufferAttach(
        target.id,
        target.depth.id,
        RL_ATTACHMENT_DEPTH,
        RL_ATTACHMENT_RENDERBUFFER,
        0);
    if (!rlFramebufferComplete(target.id))
    {
        TraceLog(LOG_ERROR, "Float render texture framebuffer is incomplete");
    }
    return target;
}
