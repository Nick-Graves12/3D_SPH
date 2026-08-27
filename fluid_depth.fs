#version 330

in vec3 worldPosition;
in vec3 worldNormal;

uniform float maximumDepth;
uniform vec3 cameraPosition;

out vec4 finalColor;


void main()
{
    float viewDepth = 1.0 / gl_FragCoord.w;

    float normalizedDepth =
        clamp(viewDepth / maximumDepth, 0.0, 1.0);

    vec3 viewDirection =
        normalize(cameraPosition - worldPosition);

    float facing =
        max(dot(normalize(worldNormal), viewDirection), 0.0);

    float particleAlpha =
        smoothstep(0.0, 0.6, facing);

    finalColor =
        vec4(vec3(normalizedDepth), 1.0);
}