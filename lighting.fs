#version 330

in vec3 fragNormal;
in vec4 fragColor;
uniform vec3 lightDirection;
out vec4 finalColor;
uniform vec4 colDiffuse;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 light = normalize(lightDirection);
    float diffuse = max(dot(normal, light), 0.0);
    float brightness = 0.25 + 0.75 * diffuse;

    vec4 baseColor = fragColor * colDiffuse;

    finalColor = vec4(
        baseColor.rgb * brightness,
        baseColor.a);
}
