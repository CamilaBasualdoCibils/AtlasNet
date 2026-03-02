#include "QuadtreeHeuristic.hpp"

#include <cmath>

#include "Global/Serialize/ByteWriter.hpp"

QuadtreeHeuristic::QuadtreeHeuristic() = default;

void QuadtreeHeuristic::SetTargetLeafCount(uint32_t count)
{
	if (count == 0)
	{
		count = 1;
	}
	options.TargetLeafCount = count;
}

void QuadtreeHeuristic::Compute(const std::span<const AtlasEntityMinimal>&)
{
	// For now we ignore entities and build a uniform quadtree-style grid that
	// subdivides the legacy grid area into N x N cells where N^2 ~= TargetLeafCount.

	const uint32_t requestedLeaves = std::max<uint32_t>(1, options.TargetLeafCount);
	const float rootWidthX = options.NetHalfExtent.x * 2.0f;
	const float rootWidthY = options.NetHalfExtent.y * 2.0f;

	// Choose an integer grid dimension N such that N^2 ~= requestedLeaves.
	const float sideF = std::sqrt(static_cast<float>(requestedLeaves));
	uint32_t side = static_cast<uint32_t>(std::round(sideF));
	if (side == 0)
	{
		side = 1;
	}
	const uint32_t actualLeaves = side * side;

	logger.DebugFormatted(
		"QuadtreeHeuristic::Compute: requested_leaves={} -> grid={}x{} ({} cells)",
		requestedLeaves, side, side, actualLeaves);

	_cells.clear();
	_cells.resize(actualLeaves);

	const float cellWidthX = rootWidthX / static_cast<float>(side);
	const float cellWidthY = rootWidthY / static_cast<float>(side);

	const float minX = -options.NetHalfExtent.x;
	const float minY = -options.NetHalfExtent.y;

	IBounds::BoundsID nextId = 0;

	for (uint32_t row = 0; row < side; ++row)
	{
		for (uint32_t col = 0; col < side; ++col)
		{
			const float centerX =
				minX + (static_cast<float>(col) + 0.5f) * cellWidthX;
			const float centerY =
				minY + (static_cast<float>(row) + 0.5f) * cellWidthY;

			GridShape cell;
			cell.ID = nextId++;
			cell.aabb.SetCenterExtents(
				vec3(centerX, centerY, 0.0f),
				vec3(cellWidthX * 0.5f, cellWidthY * 0.5f, 5.0f));

			const size_t index = static_cast<size_t>(row * side + col);
			_cells[index] = cell;
		}
	}
}

uint32_t QuadtreeHeuristic::GetBoundsCount() const
{
	return static_cast<uint32_t>(_cells.size());
}

void QuadtreeHeuristic::GetBounds(std::vector<GridShape>& out_bounds) const
{
	out_bounds = _cells;
}

void QuadtreeHeuristic::GetBoundDeltas(
	std::vector<TBoundDelta<GridShape>>& out_deltas) const
{
	out_deltas.clear();
}

IHeuristic::Type QuadtreeHeuristic::GetType() const
{
	return IHeuristic::Type::eQuadtree;
}

void QuadtreeHeuristic::SerializeBounds(
	std::unordered_map<IBounds::BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const GridShape& cell : _cells)
	{
		auto [it, inserted] = bws.emplace(cell.ID, ByteWriter{});
		(void)inserted;
		cell.Serialize(it->second);
	}
}

void QuadtreeHeuristic::Serialize(ByteWriter& bw) const
{
	bw.u64(_cells.size());
	for (const auto& cell : _cells)
	{
		cell.Serialize(bw);
	}
}

void QuadtreeHeuristic::Deserialize(ByteReader& br)
{
	const size_t cellCount = br.u64();
	_cells.resize(cellCount);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		_cells[i].Deserialize(br);
	}
}

std::optional<IBounds::BoundsID> QuadtreeHeuristic::QueryPosition(vec3 p)
{
	for (const auto& cell : _cells)
	{
		if (cell.aabb.contains(p))
		{
			return cell.ID;
		}
	}
	return std::nullopt;
}

std::unique_ptr<IBounds> QuadtreeHeuristic::GetBound(IBounds::BoundsID id)
{
	for (const auto& cell : _cells)
	{
		if (cell.ID == id)
		{
			return std::make_unique<GridShape>(cell);
		}
	}
	return nullptr;
}

