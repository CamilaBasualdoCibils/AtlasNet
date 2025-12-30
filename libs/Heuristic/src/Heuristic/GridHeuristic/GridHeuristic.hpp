#pragma once

#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "pch.hpp"

class GridShape : public IBounds
{
	vec2 Min, Max;

   public:
	virtual ~GridShape() = default;
	virtual void Serialize(ByteWriter& bw) const override
	{
		bw.vec2(Min);
		bw.vec2(Max);
	}
	virtual void Deserialize(ByteReader& br) override 
    {
        Min = br.vec2();
        Max = br.vec2();
    }

	virtual bool IsInside() const override 
    {
        
    }
};
class GridHeuristic : public THeuristic<GridShape>
{
   public:
	struct Options
	{
		vec2 GridSize = {100, 100};
	} options;
	GridHeuristic(const Options& _options);

	void Compute(const AtlasEntitySpan<const AtlasEntityMinimal>&) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<GridShape>& out_bounds) const override;
	void GetBoundDeltas(std::vector<BoundDelta>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter> bws) override;
};