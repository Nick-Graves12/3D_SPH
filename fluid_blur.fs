#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 texelDirection;
uniform float depthFalloff;

out vec4 finalColor;

void main()
{
    vec4 centerSample = texture(texture0, fragTexCoord);

    if (centerSample.a < 0.5)
    {
        finalColor = vec4(1.0, 1.0, 1.0, 0.0);
        return;
    }

    float centerDepth = centerSample.r;
    float weightedDepthSum = 0.0;
    float totalWeight = 0.0;

    const int blurRadius = 8;
    const float spatialSigma = 4.0;

    for (int offset = -blurRadius; offset <= blurRadius; offset++)
    {
        vec2 sampleCoordinates =
            fragTexCoord + texelDirection * float(offset);
        vec4 neighborSample =
            texture(texture0, sampleCoordinates);
        if (neighborSample.a < 0.5)
        {
            continue;
        }
        float spatialWeight = exp(
            -0.5 * float(offset * offset) /
            (spatialSigma * spatialSigma));

        float depthWeight = exp(
            -abs(neighborSample.r - centerDepth) *
            depthFalloff);

        float weight = spatialWeight * depthWeight;

        weightedDepthSum += neighborSample.r * weight;
        totalWeight += weight;
    }
    float blurredDepth =
        weightedDepthSum / totalWeight;

    finalColor =
        vec4(vec3(blurredDepth), 1.0);
}