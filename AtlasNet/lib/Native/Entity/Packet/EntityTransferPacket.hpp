#pragma once

#include <boost/container/small_vector.hpp>
#include <cstdint>

#include "Entity/Entity.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/Packet/Packet.hpp"
#include "Entity/EntityEnums.hpp"
class EntityTransferPacket : public TPacket<EntityTransferPacket, "EntityTransferPacket">
{
    public:

	struct StageData
	{
		virtual ~StageData() = default;
		virtual void Serialize(ByteWriter& bw) const = 0;
		virtual void Deserialize(ByteReader& br) = 0;
	};
	struct PrepareStageData : StageData
	{
		boost::container::small_vector<AtlasEntityID, 10> entityIDs;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entityIDs.size());
			for (const auto& ed : entityIDs)
			{
				bw.uuid(ed);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entityIDs.resize(br.u64());
			for (uint64_t i = 0; i < entityIDs.size(); i++)
			{
				entityIDs[i] = br.uuid();
			}
		}
	};
	struct ReadyStageData : StageData
	{
		// boost::container::small_vector<AtlasEntityID, 10> entityIDs;
		void Serialize(ByteWriter& bw) const override
		{
			// bw.u64(entityIDs.size());
			// for (const auto& ed : entityIDs)
			//{
			//	bw.uuid(ed);
			// }
		}
		void Deserialize(ByteReader& br) override
		{
			// entityIDs.resize(br.u64());
			// for (uint64_t i = 0; i < entityIDs.size(); i++)
			//{
			//	entityIDs[i] = br.uuid();
			// }
		}
	};
	struct CommitStageData : StageData
	{
		struct Data
		{
			AtlasEntity Snapshot;
			uint64_t Generation;
		};
		boost::container::small_vector<Data, 10> entitySnapshots;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitySnapshots.size());
			for (const auto& ed : entitySnapshots)
			{
				ed.Snapshot.Serialize(bw);
				bw.u64(ed.Generation);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entitySnapshots.resize(br.u64());
			for (uint64_t i = 0; i < entitySnapshots.size(); i++)
			{
				Data d;
				d.Snapshot.Deserialize(br);
				d.Generation = br.u64();
				entitySnapshots[i] = d;
			}
		}
	};
	struct CompleteStageData : StageData
	{
		// boost::container::small_vector<AtlasEntityID, 10> entityIDs;
		void Serialize(ByteWriter& bw) const override
		{
			// bw.u64(entityIDs.size());
			// for (const auto& ed : entityIDs)
			//{
			//	bw.uuid(ed);
			// }
		}
		void Deserialize(ByteReader& br) override
		{
			// entityIDs.resize(br.u64());
			// for (uint64_t i = 0; i < entityIDs.size(); i++)
			//{
			//	entityIDs[i] = br.uuid();
			// }
		}
	};
	UUID TransferID;
	EntityTransferStage stage;
	std::variant<PrepareStageData, ReadyStageData, CommitStageData, CompleteStageData> Data;

   
	void SerializeData(ByteWriter& bw) const override
	{
		bw.uuid(TransferID);
		bw.write_scalar<EntityTransferStage>(stage);
		std::visit([&bw](auto const& stageData) { stageData.Serialize(bw); }, Data);
	};
	void DeserializeData(ByteReader& br) override
	{
		TransferID = br.uuid();
		stage = br.read_scalar<EntityTransferStage>();
		switch (stage)
		{
			case EntityTransferStage::ePrepare:
				Data.emplace<PrepareStageData>();
				break;

			case EntityTransferStage::eReady:
				Data.emplace<ReadyStageData>();
				break;

			case EntityTransferStage::eCommit:
				Data.emplace<CommitStageData>();
				break;

			case EntityTransferStage::eComplete:
				Data.emplace<CompleteStageData>();
				break;

			default:
				throw std::runtime_error("Invalid ClientTransferPacket stage");
		}
		// 4️⃣ Deserialize into the constructed variant
		std::visit([&br](auto& stageData) { stageData.Deserialize(br); }, Data);
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
};
ATLASNET_REGISTER_PACKET(EntityTransferPacket, "EntityTransferPacket");