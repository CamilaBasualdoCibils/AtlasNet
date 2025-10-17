#include "Heuristic.hpp"

/**
 * @brief Computes partition shapes using heuristic algorithms.
 * 
 * @note This is currently a placeholder implementation that returns a single triangular shape.
 *       This method should be replaced with actual heuristic logic that analyzes system state
 *       and computes optimal partition boundaries.
 * 
 * @return std::vector<Shape> A collection of shapes representing partition boundaries.
 *         Currently returns a single triangle as a demonstration.
 */
std::vector<Shape> Heuristic::computePartition()
{
    HeuristicType theType = Grid;
    switch (theType)
    {
    case BigSquare:
        return computeBigSquareShape();
    case QuadTree:
        // Implement QuadTree heuristic logic here
        break;
    case KDTree:
        // Implement KDTree heuristic logic here
        break;
    case Grid:
        return computeGridShape();
    default:
        // Default case if no heuristic type is matched
        break;  
    }
    return {};
}
//(0,1)(1,1)
//(0,0)(1,0)
std::vector<Shape> Heuristic::computeBigSquareShape()
{
    std::vector<Shape> shapes;
    Shape Square;
    Square.triangles = {{vec2{0, 0}, vec2{1, 0},vec2 {0, 1}} , {vec2{1, 1}, vec2{1, 0}, vec2{0, 1}}};

    shapes.push_back(Square);
    return shapes;
}
std::vector<Shape> Heuristic::computeGridShape()
{
    std::vector<Shape> shapes;
    int gridSize = 2; // Define the grid size (e.g., 4x4)
    float step = 1.0f / gridSize;

    for (int i = 0; i < gridSize; ++i)
    {
        for (int j = 0; j < gridSize; ++j)
        {
            Shape cell;
            float x0 = i * step;
            float y0 = j * step;
            float x1 = (i + 1) * step;
            float y1 = (j + 1) * step;

            // Create two triangles for each grid cell
            cell.triangles.push_back({vec2{x0, y0}, vec2{x1, y0}, vec2{x0, y1}});
            cell.triangles.push_back({vec2{x1, y1}, vec2{x1, y0}, vec2{x0, y1}});

            shapes.push_back(cell);
        }
    }

    return shapes;
}

