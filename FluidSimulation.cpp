#include "FluidSimulation.h"
#include "SPHKernels.h"
#include "SpatialGrid.h"
#include <algorithm>
#include <cassert>
#include <cmath>

void emitLayer(const Emitter& emitter, std::vector<FluidParticle>& particles, float particleRadius)
{
    float usableRadius = emitter.radius - particleRadius;
    float spacing = 2 * particleRadius;
    Vec3 wallOffset = multiply(emitter.normal, particleRadius);
    Vec3 initialOutwardVelocity = multiply(emitter.normal, emitter.speed);

    for (int iy = -1; iy <= 1; iy ++)
    {
        for (int iz = -1; iz <= 1; iz ++)
        {
            float dY = iy * spacing;
            float dZ = iz * spacing;
            Vec3 diskOffset{0.0f, dY, dZ};

            float distanceSquared = dY * dY + dZ * dZ;
            if (distanceSquared <= usableRadius * usableRadius)
            {
                Vec3 spawnPosition =
                    add(add(emitter.center, wallOffset), diskOffset);

                FluidParticle spawnedParticle{
                    spawnPosition,
                    initialOutwardVelocity,
                    {0.0f, 0.0f, 0.0f}
                };

                particles.push_back(spawnedParticle);
            }
            else
            {
                continue;
            }
        }
    }
}

void updateEmitter(
    Emitter& emitter,
    std::vector<FluidParticle>& particles,
    float deltaTime,
    float layerInterval,
    std::size_t maxParticleCount,
    std::size_t particlesPerLayer,
    float particleRadius)
{
    emitter.spawnAccumulator += deltaTime;

    while (emitter.spawnAccumulator >= layerInterval &&
            particles.size() + particlesPerLayer <= maxParticleCount)
    {
        emitLayer(emitter, particles, particleRadius);
        emitter.spawnAccumulator -= layerInterval;
    }
    if (particles.size() + particlesPerLayer > maxParticleCount)
    {
        emitter.spawnAccumulator = 0.0f;
    }
}

//Replaces the material derivative
//Advance v, then x, advection handled implicitly
void integrateParticles(
    std::vector<FluidParticle>& particles,
    float deltaTime)
{   
    for (FluidParticle& particle : particles)
    {
        Vec3 scaledAccel = multiply(particle.acceleration, deltaTime);
        particle.velocity = add(particle.velocity, scaledAccel);
        Vec3 scaledVel = multiply(particle.velocity, deltaTime);
        particle.position = add(particle.position, scaledVel);
    }
}

void resetAccelerations(std::vector<FluidParticle>& particles, const Vec3& gravity)
{
    for (FluidParticle& particle : particles)
    {
        particle.acceleration = gravity;
    }
}

void resolveTankCollisions(
    std::vector<FluidParticle>& particles,
    const BoundingBox& tank,
    float particleRadius,
    float restitution)
{
    for (FluidParticle& particle : particles)
    {
        if (particle.position.z < tank.min.z + particleRadius)
        {
            particle.position.z = tank.min.z + particleRadius;
            
            if (particle.velocity.z < 0)
            {
                particle.velocity.z = particle.velocity.z * -restitution;
            }
        }
        if (particle.position.x < tank.min.x + particleRadius)
        {
            particle.position.x = tank.min.x + particleRadius;

            if (particle.velocity.x < 0)
            {
                particle.velocity.x = particle.velocity.x * -restitution;
            }
        }
        if (particle.position.x > tank.max.x - particleRadius)
        {
            particle.position.x = tank.max.x - particleRadius;

            if (particle.velocity.x > 0)
            {
                particle.velocity.x = particle.velocity.x * -restitution;
            }
        }
        if (particle.position.y < tank.min.y + particleRadius)
        {
            particle.position.y = tank.min.y + particleRadius;

            if (particle.velocity.y < 0)
            {
                particle.velocity.y = particle.velocity.y * -restitution;
            }
        }
        if (particle.position.y > tank.max.y - particleRadius)
        {
            particle.position.y = tank.max.y - particleRadius;

            if (particle.velocity.y > 0)
            {
                particle.velocity.y = particle.velocity.y * -restitution;
            }
        }

    }
}

void computeDensity(
    std::vector<FluidParticle>& particles,
    const UniformGrid& grid,
    const BoundingBox& bounds,
    float smoothingRadius,
    float particleMass)
{   
    //h^2 = 0.64
    const float smoothingRadiusSquared =
        smoothingRadius * smoothingRadius;

    //Coefficient = 315.0f / (64.0f * kPi * h9 ~ 11.6727
    const float coefficient =
        poly6Coefficient(smoothingRadius);

    //Self Density = m * W(0) = 1.0 * c * h6 ~ 3.06
    const float selfDensity =
        particleMass * poly6Kernel(
            0.0f,
            smoothingRadiusSquared,
            coefficient);

    //Every particle starts at its own self density
    #pragma omp parallel for
    for (std::size_t particleIndex = 0;
        particleIndex < particles.size();
        particleIndex++)
    {

        particles[particleIndex].density = selfDensity;
    }
            
    #pragma omp parallel for
    for (std::size_t particleIndex = 0;
        particleIndex < particles.size();
        particleIndex++)
    {
        FluidParticle& particle = particles[particleIndex];
        GridCoord centerCell =
            worldToCell(
                particle.position,
                bounds,
                grid.cellSize
            );

        if (!isValidCell(centerCell, grid))
        {
            continue;
        }
        for (int offsetZ = -1; offsetZ <= 1; offsetZ++)
        {
            for (int offsetY = -1; offsetY <= 1; offsetY++)
            {
                for (int offsetX = -1; offsetX <= 1; offsetX++)
                {
                    GridCoord neighborCell{
                        centerCell.x + offsetX,
                        centerCell.y + offsetY,
                        centerCell.z + offsetZ
                    };

                    //Skip cells near tank wall or out of bounds
                    if (!isValidCell(neighborCell, grid))
                    {
                        continue;
                    }
                    std::size_t bucketIndex =
                        flattenCell(neighborCell, grid);
                    
                    const auto& bucket =
                        grid.buckets[bucketIndex];
                    
                    for (std::size_t neighborIndex : bucket)
                    {
                        //Dont contribute particles own density
                        if (neighborIndex == particleIndex)
                        {
                            continue;
                        }

                        const FluidParticle& neighbor =
                            particles[neighborIndex];

                        Vec3 displacement =
                            subtract(particle.position, neighbor.position);

                        float distanceSquared =
                            squaredMagnitude(displacement);
                        
                        //Outside Kernel --> No Contribution
                        if (distanceSquared >=
                            smoothingRadiusSquared)
                        {
                            continue;
                        }

                        //Contribution = m * W
                        float contribution =
                            particleMass * poly6Kernel(
                                distanceSquared,
                                smoothingRadiusSquared,
                                coefficient);

                        particle.density += contribution;

                    }
                }
            }
        }
    }
}

//replaces the pressure Poisson Equation
void computePressure(
    std::vector<FluidParticle>& particles,
    float restDensity,
    float stiffness)
{
    for (FluidParticle& particle : particles)
    {
        //density - restDensity: how compressed is this particle?
        //density > restDensity - compressed --> positive --> pushes neighbors away
        //density < restDensity - under dense --> negative --> would pull neighbors towards itself
        //std::max(..., 0.0f) - clamps negative case to zero
        particle.pressure = 
            stiffness * std::max(particle.density - restDensity, 0.0f);
    }
}

//viscous diffusion
void addInternalAccelerations(
    std::vector<FluidParticle>& particles,
    const UniformGrid& grid,
    const BoundingBox& bounds,
    float smoothingRadius,
    float particleMass,
    float viscosityStrength)
{
    const float smoothingRadiusSquared =
        smoothingRadius * smoothingRadius;

    const float cachedSpikyCoefficient =
        spikyCoefficient(smoothingRadius);

    const float cachedViscosityCoefficient =
        viscosityCoefficient(smoothingRadius);


    
    #pragma omp parallel for
    for (std::size_t particleIndex = 0;
        particleIndex < particles.size();
        particleIndex++)
    {
        FluidParticle& particle = particles[particleIndex];

        GridCoord centerCell =
            worldToCell(
                particle.position,
                bounds,
                grid.cellSize
            );

        if (!isValidCell(centerCell, grid))
        {
            continue;
        }
        
        const float particlePressureTerm =
            particle.pressure /
            (particle.density * particle.density);

        for (int offsetZ = -1; offsetZ <= 1; offsetZ++)
        {
            for (int offsetY = -1; offsetY <= 1; offsetY++)
            {
                for (int offsetX = -1; offsetX <= 1; offsetX++)
                {
                    GridCoord neighborCell{
                        centerCell.x + offsetX,
                        centerCell.y + offsetY,
                        centerCell.z + offsetZ
                    };

                    if (!isValidCell(neighborCell, grid))
                    {
                        continue;
                    }
                    std::size_t bucketIndex =
                        flattenCell(neighborCell, grid);
                    
                    const auto& bucket =
                        grid.buckets[bucketIndex];
                    
                    for (std::size_t neighborIndex : bucket)
                    {
                        const FluidParticle& neighbor =
                            particles[neighborIndex];
                        
                        Vec3 displacement =
                            subtract(particle.position, neighbor.position);
                        
                        float distanceSquared =
                            squaredMagnitude(displacement);
                        
                        if (distanceSquared <= 1e-12f ||
                            distanceSquared >= smoothingRadiusSquared)
                        {
                            continue;
                        }
                        float distance = std::sqrt(distanceSquared);

                        Vec3 gradient = spikyGradient(displacement,
                            distance,
                            smoothingRadius,
                            cachedSpikyCoefficient);

                        //Supplies a distant dependent weight
                        //r=0 weight is max
                        //r=h wieght is 0
                        float laplacian = viscosityLaplacian(distance,
                            smoothingRadius,
                            cachedViscosityCoefficient);
                        
                        float neighborTerm =
                            neighbor.pressure /
                            (neighbor.density * neighbor.density);

                        float pressureTerm =
                            particlePressureTerm + neighborTerm;

                        Vec3 pressureContribution =
                            multiply(gradient, -particleMass * pressureTerm);

                        particle.acceleration =
                            add(particle.acceleration, pressureContribution);
                        
                        //Calculate direction of force
                        Vec3 velocityDifference = subtract(neighbor.velocity, particle.velocity);

                        float viscosityWeight =
                            viscosityStrength
                            * particleMass
                            / neighbor.density
                            * laplacian;

                        Vec3 viscosityContribution =
                            multiply(velocityDifference, viscosityWeight);

                        particle.acceleration =
                            add(particle.acceleration, viscosityContribution);
                    }
                }
            }
        }
    }
}

void validateGridContents(
    const UniformGrid& grid,
    const BoundingBox& bounds,
    const std::vector<FluidParticle>& particles)
{
    std::size_t storedCount = 0;
    for (const auto& bucket : grid.buckets)
    {
        storedCount += bucket.size();
    }
    std::size_t expectedStoredCount = 0;
    for (const FluidParticle& particle : particles)
    {
        GridCoord cell =
            worldToCell(particle.position, bounds, grid.cellSize);

        if (isValidCell(cell, grid))
        {
            expectedStoredCount++;
        }
    }

    assert(storedCount == expectedStoredCount);
}

void validateDensities(
    const std::vector<FluidParticle>& particles,
    const UniformGrid& grid,
    const BoundingBox& bounds,
    float smoothingRadius,
    float particleMass)
{
    float minimumDensity =
        particleMass * poly6Kernel(0.0f, smoothingRadius);
            
    for (const FluidParticle& particle : particles)
    {
        GridCoord cell =
            worldToCell(particle.position, bounds, grid.cellSize);

        if (!isValidCell(cell, grid))
        {
            continue;
        }

        assert(std::isfinite(particle.density));
        assert(particle.density > 0.0f);
        assert(particle.density + 1e-5f >= minimumDensity);
    }
}

void validatePressures(
    const std::vector<FluidParticle>& particles,
    float restDensity)
{
    for (const FluidParticle& particle : particles)
    {
        assert(std::isfinite(particle.pressure));
        assert(particle.pressure >= 0.0f);

        if (particle.density <= restDensity)
        {
            assert(particle.pressure == 0.0f);
        }
        else
        {
            assert(particle.pressure > 0.0f);
        }
    }
}

void validateAccelerations(
    const std::vector<FluidParticle>& particles)
{
    for (const FluidParticle& particle : particles)
    {
        assert(std::isfinite(particle.acceleration.x));
        assert(std::isfinite(particle.acceleration.y));
        assert(std::isfinite(particle.acceleration.z));
    }
}

void simulateStep(
    SimulationState& state,
    const BoundingBox& bounds,
    const Vec3& gravity,
    const SimulationConfig& config,
    PhysicsTimings& timings)
{
    double emitterStart = GetTime();
    updateEmitter(
        state.emitter,
        state.particles,
        config.fixedDeltaTime,
        config.layerInterval,
        config.maxParticleCount,
        config.particlesPerLayer,
        config.particleRadius);
    timings.emitterMs += (GetTime() - emitterStart) * 1000.0;
    double gridStart = GetTime();
    buildSpatialGrid(state.grid, bounds, state.particles);
    timings.gridMs += (GetTime() - gridStart) * 1000.0;
    #ifndef NDEBUG
    validateGridContents(state.grid, bounds, state.particles);
    #endif
    double densityStart = GetTime();
    computeDensity(state.particles, state.grid, bounds, config.smoothingRadius, config.particleMass);
    timings.densityMs += (GetTime() - densityStart) * 1000.0;
    #ifndef NDEBUG
    validateDensities(
        state.particles,
        state.grid,
        bounds,
        config.smoothingRadius,
        config.particleMass
    );
    #endif
    double pressureStart = GetTime();
    computePressure(state.particles, config.restDensity, config.stiffness);
    timings.pressureMs += (GetTime() - pressureStart) * 1000.0;
    #ifndef NDEBUG
    validatePressures(state.particles, config.restDensity);
    #endif
    double internalForcesStart = GetTime();
    resetAccelerations(state.particles, gravity);
    addInternalAccelerations(
        state.particles,
        state.grid,
        bounds,
        config.smoothingRadius,
        config.particleMass,
        config.viscosityStrength
    );
    timings.internalForcesMs += (GetTime() - internalForcesStart) * 1000.0;
    #ifndef NDEBUG
    validateAccelerations(state.particles);
    #endif
    double integrateParticlesStart = GetTime();
    integrateParticles(state.particles, config.fixedDeltaTime);
    timings.integrationMs += (GetTime() - integrateParticlesStart) * 1000.0;
    double collisionsStart = GetTime();
    resolveTankCollisions(state.particles, bounds, config.particleRadius, config.restitution);
    timings.collisionsMs += (GetTime() - collisionsStart) * 1000.0;
}

SimulationConfig createDefaultConfig(const Emitter& emitter)
{
    SimulationConfig config{};
    config.smoothingRadius = 0.8f;
    config.particleMass = 1.0f;
    config.particleRadius = 0.2f;
    config.restDensity = 15.0f;
    config.stiffness = 200.0f;
    config.viscosityStrength = 0.1f;
    config.restitution = 0.05f;
    config.fixedDeltaTime = 1.0f / 120.0f;
    config.maxParticleCount = 5000;
    config.particlesPerLayer = 5;
    config.layerInterval =
        (2.0f * config.particleRadius) / emitter.speed;
    return config;
}

ConservationQuantities computeConservationQuantities(
    const std::vector<FluidParticle>& particles,
    float particleMass,
    float gravityMagnitude)
{
    ConservationQuantities conservationQuantities;
    conservationQuantities.mass = particles.size() * particleMass;

    for (const FluidParticle& particle : particles)
    {
        conservationQuantities.momentum = add(conservationQuantities.momentum,
            multiply(particle.velocity, particleMass));

        conservationQuantities.kineticEnergy += 
            0.5f * particleMass * squaredMagnitude(particle.velocity);

        conservationQuantities.potentialEnergy += 
            particleMass * gravityMagnitude * particle.position.z;
    }
    return conservationQuantities;
}