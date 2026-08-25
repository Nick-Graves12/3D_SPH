#version 330

in vec3 vertexPosition;
in mat4 instanceTransform;

uniform mat4 mvp;

void main()
{
    gl_Position =
        mvp *
        instanceTransform *
        vec4(vertexPosition, 1.0);
}