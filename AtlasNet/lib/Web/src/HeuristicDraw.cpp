#include "HeuristicDraw.hpp"

#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include <cstdio>
#include <iostream>
void HeuristicDraw::DrawCurrentHeuristic(std::vector<IBoundsDrawShape>& shapes)
{
	std::cout << "Fetching Shapes" << std::endl;
	shapes.clear();
	std::vector<GridShape> out_grid_shape;
	HeuristicManifest::Get().GetAllPendingBounds<GridShape>(out_grid_shape);
	for (const auto& grid:out_grid_shape)
	{
		IBoundsDrawShape rect;
		rect.pos_x = grid.center().x;
		rect.pos_y = grid.center().y;
		rect.type = IBoundsDrawShape::Type::eRectangle;
		rect.size_x = grid.halfExtents().x*2;
		rect.size_y = grid.halfExtents().y*2;
		rect.id = grid.ID;
		shapes.emplace_back(rect);
	}

	std::printf("returning %i shapes",shapes.size());
}
