#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;

out vec3 worldPosition;
out vec3 worldNormal;

void main()
{
    vec4 transformedPosition =
        instanceTransform *
        vec4(vertexPosition, 1.0);

    worldPosition =
        transformedPosition.xyz;

    worldNormal =
        normalize(
            mat3(instanceTransform) *
            vertexNormal);

    gl_Position =
        mvp * transformedPosition;
}