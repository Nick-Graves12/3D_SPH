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

void updateCameraControls(Camera3D& camera, float deltaTime)
{
    const float orbitSpeed = 60.0f;
    float orbitAmount =
        orbitSpeed * DEG2RAD * deltaTime;

    if (IsKeyDown(KEY_RIGHT))
    {
        orbitCameraYaw(camera, orbitAmount);
    }

    if (IsKeyDown(KEY_LEFT))
    {
        orbitCameraYaw(camera, -orbitAmount);
    }
}

Color densityToColor(float density, float restDensity)
{
    float q = density / restDensity;

    if (q < 1.0f)
    {
        float t = std::clamp((q - 0.5f) / 0.5f, 0.0f, 1.0f);

        unsigned char red =
            static_cast<unsigned char>(0);

        unsigned char green = 
            static_cast<unsigned char>(200.0f * (1.0f - t));

        unsigned char blue =
            static_cast<unsigned char>(255);

        return Color{red, green, blue, 255};
    }

    float t = std::clamp((q - 1.0f) / 0.5f, 0.0f, 1.0f);
   
    unsigned char red =
    static_cast<unsigned char>(255.0f * t);

    unsigned char green = 0;

    unsigned char blue =
        static_cast<unsigned char>(255.0f * (1.0f - t));
    
    return Color{red, green, blue, 255};
}

void renderScene(
    const Camera3D& camera,
    const BoundingBox& tank,
    const Emitter& emitter,
    const std::vector<FluidParticle>& particles,
    float particleRadius,
    float restDensity,
    bool showDensityColors,
    bool showPressureColors,
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
    int sceneTextureLocation)
{
    BeginTextureMode(fluidTarget);
    ClearBackground(Color{255, 255, 255, 0});
    BeginMode3D(camera);

    drawParticles(
        particles,
        particleRadius,
        restDensity,
        false,
        particleModel,
        fluidDepthMaterial,
        particleTransforms);

    EndMode3D();
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

    SetShaderValueTexture(
        fluidSurfaceShader,
        sceneTextureLocation,
        sceneTarget.texture);

    BeginShaderMode(fluidSurfaceShader);

    DrawTextureRec(
        fluidTarget.texture,
        source,
        Vector2{0.0f, 0.0f},
        WHITE);
    
    EndShaderMode();

    EndDrawing();
}

void drawParticles(
    const std::vector<FluidParticle>& particles,
    float particleRadius,
    float restDensity,
    bool showDensityColors,
    const Model& particleModel,
    const Material& instancedParticleMaterial,
    std::vector<Matrix>& particleTransforms)
{
    const float renderRadius = particleRadius * 1.55f;
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
    if (showDensityColors)
    {
        for (const auto& particle : particles)
        {
            Color particleColor =
                densityToColor(
                    particle.density,
                    restDensity);
            DrawModelEx(
                particleModel,
                toRaylib(particle.position),
                Vector3{0.0f, 0.0f, 1.0f}, // rotation axis
                0.0f,                       // no rotation
                Vector3{
                    renderRadius,
                    renderRadius,
                    renderRadius
                },
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
