# 3D SPH Fluid Simulation
# Run `make` from the project root so shaders load from ./shaders/.

CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib)

# OpenMP on macOS: Apple clang ignores #pragma omp unless given these flags.
# Requires libomp (brew install libomp).
OMP_PREFIX  := $(shell brew --prefix libomp 2>/dev/null)
ifneq ($(strip $(OMP_PREFIX)),)
    CXXFLAGS += -Xpreprocessor -fopenmp -I$(OMP_PREFIX)/include
    RAYLIB_LIBS += -L$(OMP_PREFIX)/lib -lomp
endif

SRCS := \
    src/main.cpp \
    src/math/Vec3.cpp \
    src/physics/FluidSimulation.cpp \
    src/physics/SPHKernels.cpp \
    src/physics/SpatialGrid.cpp \
    src/physics/StartupTests.cpp \
    src/render/Rendering.cpp \
    src/render/Hud.cpp

main: $(SRCS) Makefile
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) $(SRCS) -o $@ $(RAYLIB_LIBS)
	@echo "built with OpenMP: $$(echo $(CXXFLAGS) | grep -c fopenmp) of 1"

run: main
	./main

clean:
	rm -f main

.PHONY: run clean
