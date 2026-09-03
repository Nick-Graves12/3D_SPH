#include "SpatialGrid.h"
#include <cassert>
#include <cmath>

GridCoord worldToCell(
    const Vec3& position,
    const BoundingBox& bounds,
    float cellSize)
{
    assert(cellSize > 0.0f);

    int cellX = static_cast<int>(
        std::floor((position.x - bounds.min.x) / cellSize)
    );
    int cellY = static_cast<int>(
        std::floor((position.y - bounds.min.y) / cellSize)
    );
    int cellZ = static_cast<int>(
        std::floor((position.z - bounds.min.z) / cellSize)
    );

    return GridCoord{cellX, cellY, cellZ};
    
}

bool isValidCell(
    const GridCoord& cell,
    const UniformGrid& grid)
{
    return
        cell.x >= 0 && cell.x < grid.countX &&
        cell.y >= 0 && cell.y < grid.countY &&
        cell.z >= 0 && cell.z < grid.countZ;
}

std::size_t flattenCell(
    const GridCoord& cell,
    const UniformGrid& grid)
{
    assert(isValidCell(cell, grid));

    std::size_t index =
        (static_cast<std::size_t>(cell.z) *
            static_cast<std::size_t>(grid.countY) +
        static_cast<std::size_t>(cell.y)) *
            static_cast<std::size_t>(grid.countX) +
        static_cast<std::size_t>(cell.x);

    return index;
} 

void buildSpatialGrid(
    UniformGrid& grid,
    const BoundingBox& bounds,
    const std::vector<FluidParticle>& particles)
{
    for (auto& bucket : grid.buckets)
    {
        bucket.clear();
    }
    for (std::size_t particleIndex = 0;
        particleIndex < particles.size();
        particleIndex++)
    {
        const Vec3& position =
            particles[particleIndex].position;
        
        GridCoord cell =
            worldToCell(position, bounds, grid.cellSize);
        
        if (!isValidCell(cell, grid))
        {
            continue;
        }
        std::size_t bucketIndex =
            flattenCell(cell, grid);
        grid.buckets[bucketIndex].push_back(particleIndex);
    }
}

std::size_t countValidNeighborCells(
    const GridCoord& center,
    const UniformGrid& grid)
{
    std::size_t validCount = 0;

    for (int offsetZ = -1; offsetZ <= 1; offsetZ++)
    {
        for (int offsetY = -1; offsetY <= 1; offsetY++)
        {
            for (int offsetX = -1; offsetX <= 1; offsetX++)
            {
                GridCoord neighborCell{
                    center.x + offsetX,
                    center.y + offsetY,
                    center.z + offsetZ
                };

                if (!isValidCell(neighborCell, grid))
                {
                    continue;
                }
                validCount++;
            }
        }
    }
    return validCount;
}

UniformGrid createUniformGrid(
    const BoundingBox& bounds,
    float cellSize)
{
    UniformGrid grid{};
    grid.cellSize = cellSize;
    
    grid.countX = static_cast<int>(std::ceil(
        (bounds.max.x - bounds.min.x) / grid.cellSize
    ));

    grid.countY = static_cast<int>(std::ceil(
        (bounds.max.y - bounds.min.y) / grid.cellSize
    ));

    grid.countZ = static_cast<int>(std::ceil(
        (bounds.max.z - bounds.min.z) / grid.cellSize
    ));

    std::size_t totalBucketCount =
        static_cast<std::size_t>(grid.countX) *
        static_cast<std::size_t>(grid.countY) *
        static_cast<std::size_t>(grid.countZ);
    
    grid.buckets.resize(totalBucketCount);

    return grid;
}