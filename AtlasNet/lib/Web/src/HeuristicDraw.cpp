#include "HeuristicDraw.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "Global/Misc/UUID.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Network/NetworkIdentity.hpp"
void HeuristicDraw::DrawCurrentHeuristic(std::vector<IBoundsDrawShape>& shapes)
{
	std::cout << "Fetching Shapes" << std::endl;
	shapes.clear();
	logger.DebugFormatted("Fetching Heuristic");
	const auto current_heuristic = HeuristicManifest::Get().PullHeuristic();
	if (!current_heuristic)
	{
		logger.Warning("No active heuristic available; returning 0 shapes.");
		return;
	}
	logger.DebugFormatted("Fetched Heuristic type={}", IHeuristic::TypeToString(current_heuristic->GetType()));

	// Serialize bounds in a heuristic-agnostic way and deserialize them as GridShape.
	// All currently supported heuristics (GridCell and Quadtree) use GridShape bounds.
	std::unordered_map<IBounds::BoundsID, ByteWriter> serializedBounds;
	current_heuristic->SerializeBounds(serializedBounds);
	logger.DebugFormatted("Heuristic reports {} bounds", serializedBounds.size());

	for (const auto& [boundId, writer] : serializedBounds)
	{
		ByteReader br(writer.bytes());
		GridShape grid;
		grid.Deserialize(br);

		IBoundsDrawShape rect;
		rect.pos_x = grid.aabb.center().x;
		rect.pos_y = grid.aabb.center().y;
		rect.type = IBoundsDrawShape::Type::eRectangle;
		rect.size_x = grid.aabb.halfExtents().x * 2;
		rect.size_y = grid.aabb.halfExtents().y * 2;
		rect.id = grid.ID;

		if (const auto shard = HeuristicManifest::Get().ShardFromBoundID(grid.ID);
			shard.has_value())
		{
			rect.color = "rgba(100, 255, 149, 1)";
			rect.owner_id = shard->ToString();
		}
		else
		{
			rect.color = "rgba(255, 149, 100, 1)";
		}

		shapes.emplace_back(rect);
	}
	/*
	 std::vector<GridShape> out_grid_shape;
	 HeuristicManifest::Get().GetAllPendingBounds<GridShape>(out_grid_shape);
	 for (const auto& grid : out_grid_shape)
	 {
		 IBoundsDrawShape rect;
		 rect.pos_x = grid.aabb.center().x;
		 rect.pos_y = grid.aabb.center().y;
		 rect.type = IBoundsDrawShape::Type::eRectangle;
		 rect.size_x = grid.aabb.halfExtents().x * 2;
		 rect.size_y = grid.aabb.halfExtents().y * 2;
		 rect.id = grid.ID;
		 rect.owner_id = "";
		 rect.color = "rgba(255, 149, 100, 1)";
		 shapes.emplace_back(rect);
	 }
	 std::unordered_map<NetworkIdentity, GridShape> claimed_bounds;
	 HeuristicManifest::Get().GetAllClaimedBounds(claimed_bounds);
	 for (const auto& [claim_key, grid] : claimed_bounds)
	 {
		 IBoundsDrawShape rect;
		 rect.pos_x = grid.aabb.center().x;
		 rect.pos_y = grid.aabb.center().y;
		 rect.type = IBoundsDrawShape::Type::eRectangle;
		 rect.size_x = grid.aabb.halfExtents().x * 2;
		 rect.size_y = grid.aabb.halfExtents().y * 2;
		 rect.id = grid.ID;
		 rect.owner_id = claim_key.ToString();
		 rect.color = "rgba(100, 255, 149, 1)";
		 shapes.emplace_back(rect);
	 }
  */
	std::printf("returning %zu shapes", shapes.size());
}
