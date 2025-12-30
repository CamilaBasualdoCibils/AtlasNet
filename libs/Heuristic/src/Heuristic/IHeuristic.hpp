#pragma once
#include <boost/describe/enum.hpp>
#include <cstdint>
#include <optional>
#include <pch.hpp>
#include <unordered_map>
#include <vector>

#include "Entity.hpp"
#include "IBounds.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
#include "EntityList.hpp"
class IHeuristic
{
   public:
	enum class Type
	{
        eNone,
		eGridCell,
		eOctree,
		eQuadtree
	};
    BOOST_DESCRIBE_NESTED_ENUM(Type,eNone, eGridCell, eOctree, eQuadtree)
	struct Value
	{
		vec3 Position;
	};

   private:


   public:
	[[nodiscard]] virtual Type GetType() const = 0;

	virtual void Compute(const AtlasEntitySpan<const AtlasEntityMinimal>& span) = 0;

	virtual uint32_t GetBoundsCount() const = 0;

	virtual void SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter> bws) = 0;
};
template <typename BoundType>
class THeuristic : public IHeuristic
{
   public:
	virtual void GetBounds(std::vector<BoundType>& out_bounds) const = 0;
	struct BoundDelta
	{
		BoundType OldShape;
		std::optional<BoundType> NewShape;
	};
	virtual void GetBoundDeltas(std::vector<BoundDelta>& out_deltas) const = 0;
};