# 3D SPH Fluid Simulation

A real-time 3D liquid simulation written in C++ using [raylib](https://www.raylib.com/).

Particles are emitted from a circular pipe opening into a container. Their motion is calculated on the CPU using Smoothed Particle Hydrodynamics (SPH), while a screen-space rendering pipeline blends the particles into a smoother liquid surface.

This is an educational 3D SPH demonstration, not a validated engineering pipe-flow or CFD solver.

## Demo

![3D SPH Fluid Simulation](Screenshots/demo.gif)

## Features

- Circular disk particle emitter
- Gravity and axis-aligned container collisions
- Fixed-timestep physics simulation
- Custom 3D vector mathematics
- 3D SPH density estimation
- Pressure and viscosity forces
- Uniform spatial grid
- Search across 27 neighboring grid cells
- Configurable limit of approximately 5,000 particles
- Instanced particle rendering
- Screen-space fluid surface reconstruction
- Density and pressure visualization modes
- Physics, rendering, and conservation diagnostics
- Per-second report of performance, density/pressure ranges, conserved quantities (mass, momentum, kinetic and potential energy), and a hydrostatic pressure check
- OpenMP-parallel density and force evaluation

## Controls

- `D` — toggle density visualization (particles colored by density ratio)
- `P` — toggle pressure visualization (particles colored by pressure)
- Left / Right arrow — orbit the camera around the tank

## Simulation Pipeline

Each physics step performs the following operations:

1. Emit new particles from the pipe opening.
2. Rebuild the spatial grid.
3. Estimate particle density.
4. Calculate pressure from density.
5. Calculate pressure and viscosity accelerations.
6. Integrate velocity and position.
7. Resolve collisions against the container.

The spatial grid avoids testing every particle against every other particle. Each particle only examines nearby buckets and then rejects candidates outside the smoothing radius.

## SPH Model

Density is estimated from neighboring particle masses:

$$
\rho_i = \sum_j m_j W(\lVert x_i-x_j\rVert, h)
$$

Pressure is calculated from the difference between the measured density and rest density:

$$
p_i = k \max(\rho_i - \rho_0, 0)
$$

The simulation uses three-dimensional versions of:

- Poly6 density kernel
- Spiky pressure-gradient kernel
- Viscosity Laplacian kernel

Important simulation parameters include:

- Particle mass
- Particle spacing
- Particle radius
- Smoothing radius
- Rest density
- Pressure stiffness
- Viscosity strength
- Fixed timestep
- Emitter velocity and interval

## Rendering

Two rendering modes are available:

- Instanced spheres for inspecting individual particles
- Screen-space fluid rendering for a smoother connected surface

The screen-space path:

1. Renders particle depth.
2. Smooths the depth field.
3. Reconstructs surface normals.
4. Shades the reconstructed fluid surface.

Density and pressure color modes are also available for debugging the simulation rather than judging its final appearance.

## Requirements

- A C++17-compatible compiler
- raylib
- `pkg-config`
- OpenMP runtime (`libomp` on macOS)

On macOS with Homebrew:

```bash
brew install raylib pkg-config libomp
```

## Build

Clone the repository:

```bash
git clone https://github.com/Nick-Graves12/3D_SPH.git
cd 3D_SPH
```

The neighbor search in the density and force passes is parallelized with OpenMP. On macOS, Apple's clang requires the `libomp` runtime and does not link it automatically, so the flags below are needed.

Debug build:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Xpreprocessor -fopenmp -L$(brew --prefix libomp)/lib -lomp *.cpp -o main $(pkg-config --cflags --libs raylib)
```

Optimized build:

```bash
g++ -std=c++17 -O3 -DNDEBUG -Xpreprocessor -fopenmp -L$(brew --prefix libomp)/lib -lomp *.cpp -o main $(pkg-config --cflags --libs raylib)
```

On Linux with GCC, the OpenMP flags simplify to just `-fopenmp`.

Run the program from the project directory so its shader files can be found:

```bash
./main
```

## Project Structure

```text
main.cpp                 Application setup and main loop
SimulationTypes.h        Shared data structures (particles, emitter, config, state)
FluidSimulation.h/.cpp   Particle emission, SPH physics, and conservation checks
SPHKernels.h/.cpp        Three-dimensional SPH kernels
SpatialGrid.h/.cpp       Uniform spatial grid and neighbor queries
Rendering.h/.cpp         Particle and screen-space fluid rendering
Vec3.h/.cpp              Custom 3D vector type
StartupTests.h/.cpp      Mathematical and simulation checks
*.vs / *.fs              Vertex and fragment shaders
```

## Numerical Stability

SPH behavior depends on several coupled parameters. A result that looks plausible is not necessarily numerically stable or physically correct.

In particular:

- A smaller timestep generally improves stability.
- Particle spacing should be chosen relative to the smoothing radius.
- Excessive stiffness can create large pressure accelerations.
- Too little viscosity allows noisy relative motion.
- Too much viscosity suppresses visible fluid motion.
- The average density ratio should remain reasonably close to `1.0`.
- The conservation report (printed once per second) should show mass and momentum held steady, with total energy decaying or flat once emission stops.
- Rendering radius changes appearance but should not be treated as a substitute for correcting the physics.

## Hydrostatic Check

The per-second diagnostic ends with a hydrostatic check:

```text
hydrostatic: h=2.59, predicted=406.6, measured=605.3, ratio=1.49
```

- `h` — column height, the 99th percentile of particle heights (robust to a few splashing drops).
- `predicted` — bottom pressure a uniform column of height `h` should exert under this EOS, from `dp/dz = -rho*g` with `rho = rho0 + p/k`: `p_bottom = k*rho0*(exp(g*h/k) - 1)`.
- `measured` — mean pressure of the bottom 10% of the column.
- `ratio` — measured / predicted.

**How to read it:**

- Ignore it during filling — `h` is dominated by the incoming jet, not the pool.
- Once emission stops and the surface settles, `ratio` near `1.0` means the fluid is hydrostatic against its own equation of state.
- With the default settings (`k = 200`, `restDensity = 15`) the settled ratio parks around `1.5` (measured ≈ 600 vs predicted ≈ 400). This is the expected bias of the clamped EOS `p = k*max(rho - rho0, 0)`: under-dense fluctuations are clamped to zero pressure, so mean density and pressure at depth run high. The bottom lands ~20% above rest density instead of the theoretical ~12% for this column height.
- The elevated average density ratio at rest (~1.09) is mostly the hydrostatic compression of the column — the exact mean for this EOS is `(k/(g*h))*(exp(g*h/k) - 1)` ≈ 1.06 — plus a small clamping bias.
- Raising `restDensity` does not fix it (verified: 15 → 16.3 left the average density ratio and bottom compression essentially unchanged). The bias is a property of the formulation, not a rest-density calibration error.

Use `ratio` qualitatively: it should drop toward 1.0 as the fluid settles, and a steady ~1.5 at rest is the expected noise floor for this SPH formulation. Production solvers typically add δ-SPH density diffusion to remove it.

## Current Limitations

- CPU-only simulation
- Axis-aligned rectangular container
- No simulation inside the pipe
- The pipe is represented only by a circular disk emitter
- Simplified boundary collision model
- No engineering validation
- Screen-space rendering is an approximation of the fluid surface

## Possible Future Work

- Improved boundary-particle handling
- Surface tension
- Interactive parameter controls
- Additional emitters and obstacles
- GPU simulation
- More advanced fluid shading, reflections, and refraction

