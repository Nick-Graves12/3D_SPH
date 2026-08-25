#include "Vec3.h"
#include "SPHKernels.h"
#include <cassert>
#include <cmath>

constexpr float kPi = 3.14159265358979323846f;

float poly6Kernel(float distanceSquared, float smoothingRadiusSquared, float coefficient)
{
    assert(distanceSquared >= 0);
    assert(smoothingRadiusSquared > 0);
    
    if (distanceSquared >= smoothingRadiusSquared)
    {
        return 0.0f;
    }
    float q = smoothingRadiusSquared - distanceSquared;
    return coefficient * (q * q * q);
}

float poly6Kernel(float distanceSquared, float smoothingRadius)
{
    assert(distanceSquared >= 0.0f);
    assert(smoothingRadius > 0);
    float h2 = smoothingRadius * smoothingRadius;
    float coefficient = poly6Coefficient(smoothingRadius);
    
    return poly6Kernel(distanceSquared, h2, coefficient);
}

float poly6Coefficient(float smoothingRadius)
{
    assert(smoothingRadius > 0);
    float h2 = smoothingRadius * smoothingRadius;
    float h4 = h2 * h2;
    float h8 = h4 * h4;
    float h9 = h8 * smoothingRadius;

    return 315.0f / (64.0f * kPi * h9);
}

void testPoly6Kernel()
{
    assert(std::abs(poly6Kernel(0.0f, 1.0f) - 1.56668f) < 1e-4f);

    assert(std::abs(poly6Kernel(0.25f, 1.0f) - 0.660944f) < 1e-4f);

    assert(poly6Kernel(1.0f, 1.0f) == 0.0f);
    assert(poly6Kernel(4.0f, 1.0f) == 0.0f);
}

Vec3 spikyGradient(const Vec3& displacement, float smoothingRadius)
{
    assert(smoothingRadius > 0);
    
    float distanceSquared = squaredMagnitude(displacement);
    if (distanceSquared <= 1e-12f ||
        distanceSquared >= smoothingRadius * smoothingRadius)
    {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    float distance = std::sqrt(distanceSquared);

    float difference = smoothingRadius - distance;

    float coefficient =
        (-45.0f / (kPi * std::pow(smoothingRadius, 6.0f)))
        * difference * difference;
    
    Vec3 direction = divide(displacement, distance);
    return multiply(direction, coefficient);
}

float spikyCoefficient(float smoothingRadius)
{
    assert(smoothingRadius > 0.0f);
    float h2 = smoothingRadius * smoothingRadius;
    float h6 = h2 * h2 * h2;

    float coefficient =
        (-45.0f / (kPi * h6));

    return coefficient;
}

Vec3 spikyGradient(
    const Vec3& displacement,
    float distance,
    float smoothingRadius,
    float coefficient)
{
    assert(smoothingRadius > 0.0f);
    if (distance <= 0.0f || distance >= smoothingRadius)
    {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    float difference = smoothingRadius - distance;
    float gradientMagnitude =
        coefficient * difference * difference;
    Vec3 direction = divide(displacement, distance);

    return multiply(direction, gradientMagnitude);
}

void testSpikyGradient()
{
    const float tolerance = 1e-5f;
    const float h = 1.0f;

    Vec3 zero = spikyGradient(Vec3{0.0f, 0.0f, 0.0f}, h);
    assert(std::abs(zero.x) < tolerance);
    assert(std::abs(zero.y) < tolerance);
    assert(std::abs(zero.z) < tolerance);

    Vec3 boundary = spikyGradient(Vec3{1.0f, 0.0f, 0.0f}, h);
    assert(std::abs(boundary.x) < tolerance);
    assert(std::abs(boundary.y) < tolerance);
    assert(std::abs(boundary.z) < tolerance);

    Vec3 positiveX = spikyGradient(Vec3{0.5f, 0.0f, 0.0f}, h);
    Vec3 negativeX = spikyGradient(Vec3{-0.5f, 0.0f, 0.0f}, h);

    assert(positiveX.x < 0.0f);
    assert(std::abs(positiveX.y) < tolerance);
    assert(std::abs(positiveX.z) < tolerance);

    assert(std::abs(positiveX.x + negativeX.x) < tolerance);
    assert(std::abs(positiveX.y + negativeX.y) < tolerance);
    assert(std::abs(positiveX.z + negativeX.z) < tolerance);
}

float viscosityLaplacian(float distance, float smoothingRadius)
{
    assert(smoothingRadius > 0);

    if (distance <= 0.0f || distance >= smoothingRadius)
    {
        return 0.0f;
    }
    float difference = smoothingRadius - distance;

    float coefficient =
        (45.0f / (kPi * std::pow(smoothingRadius, 6.0f)))
        * difference;
    return coefficient;
}

float viscosityCoefficient(float smoothingRadius)
{
    assert(smoothingRadius > 0.0f);
    float h2 = smoothingRadius * smoothingRadius;
    float h6 = h2 * h2 * h2;

    float coefficient =
        (45.0f / (kPi * h6));

    return coefficient;
}

float viscosityLaplacian(
    float distance,
    float smoothingRadius,
    float coefficient)
{
    assert(smoothingRadius > 0.0f);

    if (distance <= 0.0f || distance >= smoothingRadius)
    {
        return 0.0f;
    }
    float difference = smoothingRadius - distance;

    return coefficient * difference;
}

void testViscosityLaplacian()
{
    const float h = 1.0f;

    float atZero = viscosityLaplacian(0.0f, h);
    float atBoundary = viscosityLaplacian(h, h);
    float atHalf = viscosityLaplacian(0.5f * h, h);
    float atQuarter = viscosityLaplacian(0.25f * h, h);

    assert(atZero == 0.0f);
    assert(atBoundary == 0.0f);
    assert(atHalf > 0.0f);
    assert(atQuarter > atHalf);
}