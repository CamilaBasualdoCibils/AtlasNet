#include "HeuristicManifest.hpp"

#include <boost/describe/enum_from_string.hpp>

#include "Heuristic/IHeuristic.hpp"
#include "InternalDB.hpp"
//IHeuristic::Type HeuristicManifest::GetActiveHeuristic() const
//{
//	const auto TypeEntry = InternalDB::Get()->Get(HeuristicTypeKey);
//	if (!TypeEntry) //if the entry in redis does not exist the its none of em.
//		return IHeuristic::Type::eNone;
//
//	IHeuristic::Type type;
//
//	boost::describe::enum_from_string(TypeEntry.value(), type);
//	return type;
//}
