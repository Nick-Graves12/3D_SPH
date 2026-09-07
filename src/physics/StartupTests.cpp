#include "StartupTests.h"
#include "SPHKernels.h"
#include "SpatialGrid.h"
#include <cassert>
#include <cmath>

void runStartupTests(
    const UniformGrid& grid,
    const BoundingBox& bounds)
{

    const float cellSize = grid.cellSize;
    assert(cellSize > 0.0f);


    // The neighbor-search grid is sized from the smoothing radius
    // (cellSize == smoothingRadius in main.cpp), so compute every expected
    // value from the actual bounds/cellSize instead of hard-coding one
    // resolution (these used to be fixed for smoothingRadius == 0.8).
    const int expectedCountX = static_cast<int>(std::ceil(
        (bounds.max.x - bounds.min.x) / cellSize));
    const int expectedCountY = static_cast<int>(std::ceil(
        (bounds.max.y - bounds.min.y) / cellSize));
    const int expectedCountZ = static_cast<int>(std::ceil(
        (bounds.max.z - bounds.min.z) / cellSize));


    assert(grid.countX == expectedCountX);
    assert(grid.countY == expectedCountY);
    assert(grid.countZ == expectedCountZ);
    assert(grid.buckets.size() ==
        static_cast<std::size_t>(expectedCountX) *
        static_cast<std::size_t>(expectedCountY) *
        static_cast<std::size_t>(expectedCountZ));

    // An interior sample point must land in the cell geometry implies.
    const Vec3 probe{-4.8f, 3.0f, 3.0f};
    const int probeCellX = static_cast<int>(std::floor(
        (probe.x - bounds.min.x) / cellSize));
    const int probeCellY = static_cast<int>(std::floor(
        (probe.y - bounds.min.y) / cellSize));
    const int probeCellZ = static_cast<int>(std::floor(
        (probe.z - bounds.min.z) / cellSize));

    GridCoord testCell = worldToCell(probe, bounds, cellSize);
    assert(testCell.x == probeCellX);
    assert(testCell.y == probeCellY);
    assert(testCell.z == probeCellZ);
    assert(isValidCell(testCell, grid));

    // flattenCell: (z * countY + y) * countX + x
    assert(flattenCell(GridCoord{0, 0, 0}, grid) == 0);


    assert(flattenCell(GridCoord{0, expectedCountY - 1, 0}, grid) ==
        static_cast<std::size_t>(expectedCountY - 1) * expectedCountX);
    assert(flattenCell(GridCoord{0, 0, expectedCountZ - 1}, grid) ==
        static_cast<std::size_t>(expectedCountZ - 1) *
            expectedCountY * expectedCountX);
    assert(flattenCell(GridCoord{
        expectedCountX - 1,
        expectedCountY - 1,
        expectedCountZ - 1}, grid) ==
        static_cast<std::size_t>(expectedCountX) *
            expectedCountY * expectedCountZ - 1);


    // 3x3x3 neighbor search: full interior vs. a single corner.
    assert(expectedCountX >= 3 && expectedCountY >= 3 &&
           expectedCountZ >= 3);
    assert(countValidNeighborCells(GridCoord{
        expectedCountX / 2,
        expectedCountY / 2,
        expectedCountZ / 2}, grid) == 27);
    assert(countValidNeighborCells(GridCoord{0, 0, 0}, grid) == 8);

    testPoly6Kernel();
    testSpikyGradient();
    testViscosityLaplacian();
}