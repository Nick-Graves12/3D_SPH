#pragma once

#include "SimulationTypes.h"
#include "raylib.h"

struct PhysicsTimings
{
    double emitterMs = 0.0;
    double gridMs = 0.0;
    double densityMs = 0.0;
    double pressureMs = 0.0;
    double internalForcesMs = 0.0;
    double integrationMs = 0.0;
    double collisionsMs = 0.0;
};

SimulationConfig createDefaultConfig(const Emitter& emitter);

void simulateStep(
    SimulationState& state,
    const BoundingBox& bounds,
    const Vec3& gravity,
    const SimulationConfig& config,
    PhysicsTimings& timings);

ConservationQuantities computeConservationQuantities(
        const std::vector<FluidParticle>& particles,
        float particleMass,
        float gravityMagnitude);
