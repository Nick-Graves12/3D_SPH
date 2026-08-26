#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform sampler2D sceneTexture;
uniform vec2 texelSize;
uniform vec3 fluidColor;
uniform vec3 lightDirection;
uniform float normalStrength;
uniform float refractionStrength;

out vec4 finalColor;

void main()
{
    vec4 centerSample =
        texture(texture0, fragTexCoord);

    if (centerSample.a < 0.5)
    {
        finalColor = vec4(0.0);
        return;
    }

    float centerDepth = centerSample.r;

    vec4 leftSample =
        texture(texture0, fragTexCoord - vec2(texelSize.x, 0.0));

    vec4 rightSample =
        texture(texture0, fragTexCoord + vec2(texelSize.x, 0.0));

    vec4 downSample =
        texture(texture0, fragTexCoord - vec2(0.0, texelSize.y));

    vec4 upSample =
        texture(texture0, fragTexCoord + vec2(0.0, texelSize.y));

    float leftDepth =
        leftSample.a >= 0.5 ? leftSample.r : centerDepth;

    float rightDepth =
        rightSample.a >= 0.5 ? rightSample.r : centerDepth;

    float downDepth =
        downSample.a >= 0.5 ? downSample.r : centerDepth;

    float upDepth =
        upSample.a >= 0.5 ? upSample.r : centerDepth;

    float depthDx =
        (rightDepth - leftDepth) * normalStrength;

    float depthDy =
        (upDepth - downDepth) * normalStrength;

    vec3 surfaceNormal =
        normalize(vec3(-depthDx, -depthDy, 1.0));

    vec2 refractionOffset =
        surfaceNormal.xy * refractionStrength;

    vec2 sceneUV =
        vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);

    sceneUV += vec2(refractionOffset.x, -refractionOffset.y);
    sceneUV = clamp(sceneUV, vec2(0.0), vec2(1.0));

    vec3 sceneColor =
        texture(sceneTexture, sceneUV).rgb;

    vec3 normalizedLight =
        normalize(lightDirection);

    float diffuse =
        max(dot(surfaceNormal, normalizedLight), 0.0);

    vec3 viewDirection =
        vec3(0.0, 0.0, 1.0);

    vec3 halfwayDirection =
        normalize(normalizedLight + viewDirection);

    float specular =
        pow(
            max(dot(surfaceNormal, halfwayDirection), 0.0),
            48.0);

    float ambient = 0.25;

    float fresnel =
        pow(1.0 - max(surfaceNormal.z, 0.0), 3.0);

    vec3 shadedColor =
        fluidColor * (ambient + 0.75 * diffuse);
        
    shadedColor +=
        vec3(0.85, 0.92, 1.0) * specular * 0.45;

    shadedColor +=
        vec3(0.25, 0.45, 0.65) * fresnel;

    float fluidOpacity = 0.65;

    vec3 compositeColor =
        mix(sceneColor, shadedColor, fluidOpacity);

    finalColor = vec4(compositeColor, 1.0);
}