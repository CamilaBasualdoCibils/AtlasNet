#include "GridHeuristic.hpp"
GridHeuristic::GridHeuristic(const Options& _options) : options(_options) {}
void GridHeuristic::Compute(const AtlasEntitySpan<const AtlasEntityMinimal>& span) {}
uint32_t GridHeuristic::GetBoundsCount() const {}
void GridHeuristic::GetBounds(std::vector<GridShape>& out_bounds) const {}
void GridHeuristic::GetBoundDeltas(std::vector<BoundDelta>& out_deltas) const {}
IHeuristic::Type GridHeuristic::GetType() const
{
	return IHeuristic::Type::eGridCell;
}
void GridHeuristic::SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter> bws) {}
