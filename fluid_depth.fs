#version 330

uniform float maximumDepth;

out vec4 finalColor;

void main()
{
    float viewDepth = 1.0 / gl_FragCoord.w;

    float normalizedDepth =
        clamp(viewDepth / maximumDepth, 0.0, 1.0);

    finalColor =
        vec4(vec3(normalizedDepth), 1.0);
}