#pragma once

#include <string>
#include <vector>

#include "InternalDB.hpp"
#include "pch.hpp"
#include "Entity.hpp"
#include "Serialize/ByteWriter.hpp"
#include "Serialize/ByteReader.hpp"

/**
 * @brief Database manifest for authoritative entities per partition.
 *
 * This is intended as a source-of-truth snapshot of which entities a partition
 * currently owns authority over. Runtime code can push its in-memory
 * authoritative set here periodically, and tools / other services can read it.
 */
class EntityAuthorityManifest
{
  public:
	static inline const std::string HASH_KEY = "EntityAuthority:Partitions";

	/**
	 * @brief Store the full authoritative entity set for a partition.
	 *
	 * The entities are serialized as:
	 *   [u64 count][entity0][entity1]...[entityN-1]
	 * using AtlasEntity::Serialize.
	 */
	static bool StorePartitionEntities(const std::string &partitionId,
									   const std::vector<AtlasEntity> &entities)
	{
		ByteWriter bw;
		bw.write_scalar<uint64_t>(entities.size());
		for (const auto &e : entities)
		{
			e.Serialize(bw);
		}

		const auto data = bw.as_string_view();
		const auto result = InternalDB::Get()->HSet(HASH_KEY, partitionId, data);
		return result >= 0;
	}

	/**
	 * @brief Fetch the last stored authoritative entity set for a partition.
	 */
	static std::vector<AtlasEntity> FetchPartitionEntities(const std::string &partitionId)
	{
		std::vector<AtlasEntity> entities;

		const auto stored = InternalDB::Get()->HGet(HASH_KEY, partitionId);
		if (!stored || stored->empty())
		{
			return entities;
		}

		ByteReader br(*stored);
		const auto count = br.read_scalar<uint64_t>();
		entities.resize(count);
		for (uint64_t i = 0; i < count; ++i)
		{
			entities[i].Deserialize(br);
		}

		return entities;
	}

	/**
	 * @brief Fetch all partitions and their serialized entity blobs.
	 *
	 * This is useful for tooling that wants to inspect every partition's
	 * authoritative set without caring about the decoded entities.
	 */
	static std::unordered_map<std::string, std::string> FetchAllRaw()
	{
		return InternalDB::Get()->HGetAll(HASH_KEY);
	}
};

