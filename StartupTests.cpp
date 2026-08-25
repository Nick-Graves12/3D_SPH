#include "StartupTests.h"
#include "SPHKernels.h"
#include "SpatialGrid.h"
#include <cassert>

void runStartupTests(
    const UniformGrid& grid,
    const BoundingBox& bounds)
{
    GridCoord testCell = worldToCell(
        Vec3{-4.8f, 3.0f, 3.0f},
        bounds,
        grid.cellSize
    );

    assert(testCell.x == 0);
    assert(testCell.y == 6);
    assert(testCell.z == 3);

    assert(grid.countX == 13);
    assert(grid.countY == 13);
    assert(grid.countZ == 8);
    assert(grid.buckets.size() == 1352);

    assert(flattenCell(GridCoord{0, 0, 0}, grid) == 0);
    assert(flattenCell(GridCoord{0, 6, 3}, grid) == 585);
    assert(flattenCell(GridCoord{12, 12, 7}, grid) == 1351);

    assert(countValidNeighborCells(GridCoord{6, 6, 3}, grid) == 27);
    assert(countValidNeighborCells(GridCoord{0, 0, 0}, grid) == 8);

    testPoly6Kernel();
    testSpikyGradient();
    testViscosityLaplacian();
}