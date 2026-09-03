#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 texelDirection;
uniform float depthFalloff;

out vec4 finalColor;

void main()
{
    vec4 centerSample = texture(texture0, fragTexCoord);

    float centerDepth = centerSample.r;
    float weightedDepthSum = 0.0;
    float totalWeight = 0.0;
    float weightedAlphaSum = 0.0;
    float totalSpatialWeight = 0.0;

    const int blurRadius = 4;
    const float spatialSigma = 3.0;

    for (int offset = -blurRadius; offset <= blurRadius; offset++)
    {
        vec2 sampleCoordinates =
            fragTexCoord + texelDirection * float(offset);
        vec4 neighborSample =
            texture(texture0, sampleCoordinates);
      
        float spatialWeight = exp(
            -0.5 * float(offset * offset) /
            (spatialSigma * spatialSigma));

        if (abs(offset) <= 3)
        {
            weightedAlphaSum += neighborSample.a * spatialWeight;
            totalSpatialWeight += spatialWeight;
        }

       if (neighborSample.a > 0.001)
        {
            float depthWeight = 1.0;

            if (centerSample.a > 0.05)
            {
                depthWeight = exp(
                    -abs(neighborSample.r - centerDepth) *
                    depthFalloff);
            }

            float weight =
                spatialWeight *
                depthWeight *
                neighborSample.a;

            weightedDepthSum +=
                neighborSample.r * weight;

            totalWeight += weight;
        }
    }
    if (totalSpatialWeight <= 0.0)
    {
        finalColor = vec4(0.0);
        return;
    }
    float blurredDepth =
        totalWeight > 0.0
            ? weightedDepthSum / totalWeight
            : 0.0;

   float neighborAlpha =
        weightedAlphaSum / totalSpatialWeight;

    float blurredAlpha =
        mix(centerSample.a, neighborAlpha, 0.50);


    finalColor =
        vec4(vec3(blurredDepth), blurredAlpha);
}