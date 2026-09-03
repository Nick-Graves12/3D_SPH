#pragma once

#include "../SimulationTypes.h"
#include "raylib.h"

GridCoord worldToCell(const Vec3& position,
    const BoundingBox& bounds,
    float cellSize);

bool isValidCell(const GridCoord& cell, const UniformGrid& grid);

std::size_t flattenCell(const GridCoord& cell, const UniformGrid& grid);

void buildSpatialGrid(
    UniformGrid& grid,
    const BoundingBox& bounds,
    const std::vector<FluidParticle>& particles);

std::size_t countValidNeighborCells(const GridCoord& center, const UniformGrid& grid);

UniformGrid createUniformGrid(const BoundingBox& bounds, float cellSize);