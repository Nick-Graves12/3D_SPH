#pragma once

#include "math/Vec3.h"
#include <cstddef>
#include <vector>

struct GridCoord
{
    int x;
    int y;
    int z;
};

struct UniformGrid
{
    float cellSize;
    int countX;
    int countY;
    int countZ;
    std::vector<std::vector<std::size_t>> buckets;
};

struct FluidParticle
{
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;
    float density = 0.0f;
    float pressure = 0.0f;
    float temperature = 0.0f;
};

struct Emitter
{
    Vec3 center;
    Vec3 normal;
    float radius;
    float speed;
    float spawnAccumulator;
    float temperature = 0.0f;
};

struct SimulationConfig
{
    float smoothingRadius;
    float particleMass;
    float particleRadius;
    float restDensity;
    float stiffness;
    float viscosityStrength;
    float restitution;
    float fixedDeltaTime;
    float layerInterval;
    std::size_t maxParticleCount;
    std::size_t particlesPerLayer;
    float referenceTemperature;
    float heatDiffusionCoefficient;
    float thermalExpansionCoefficient;
    float viscosityTemperatureSensitivity;
    float heaterHeight;
    float heaterTemperature;
    float heaterTransferRate;
};

struct SimulationState
{
    Emitter emitter;
    std::vector<FluidParticle> particles;
    UniformGrid grid;
    // Runtime toggle (H key): off reverts to the original cold solver --
    // no heater, no diffusion, no buoyancy, temperature-independent viscosity.
    bool heatingEnabled = true;
};

struct ConservationQuantities
{
    float mass = 0.0f;
    Vec3 momentum{0.0f, 0.0f, 0.0f};
    float kineticEnergy = 0.0f;
    float potentialEnergy = 0.0f;
};

struct HydrostaticCheck
{
    float fluidHeight = 0.0f;
    float predictedBottomPressure = 0.0f;
    float measuredBottomPressure = 0.0f;
    float ratio = 0.0f;
};