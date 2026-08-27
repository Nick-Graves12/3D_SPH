#version 330

out vec4 finalColor;

void main()
{
    float contribution = 0.04;

    finalColor =
        vec4(
            contribution,
            contribution,
            contribution,
            1.0);
}