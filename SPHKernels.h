#pragma once
#include "Vec3.h"

float poly6Kernel(float distanceSquared, float smoothingRadius);
float poly6Kernel(
    float distanceSquared,
    float smoothingRadiusSquared,
    float coefficient);
float poly6Coefficient(float smoothingRadius);
void testPoly6Kernel();
Vec3 spikyGradient(const Vec3& displacement, float smoothingRadius);
Vec3 spikyGradient(
    const Vec3& displacement,
    float distance,
    float smoothingRadius,
    float coefficient);
float spikyCoefficient(float smoothingRadius);
void testSpikyGradient();
float viscosityLaplacian(float distance, float smoothingRadius);
float viscosityLaplacian(
    float distance,
    float smoothingRadius,
    float coefficient);
float viscosityCoefficient(float smoothingRadius);
void testViscosityLaplacian();