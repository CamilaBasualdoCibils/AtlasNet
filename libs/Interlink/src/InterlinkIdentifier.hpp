#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <pch.hpp>
#include <string>

#include "InterlinkEnums.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

struct InterLinkIdentifier
{
	InterlinkType Type = InterlinkType::eInvalid;
	std::string ID = "";

   public:
	InterLinkIdentifier() = default;
	InterLinkIdentifier(InterlinkType _Type, const std::string& _ID = "") : Type(_Type), ID(_ID) {}

	static InterLinkIdentifier MakeIDWatchDog();
	static InterLinkIdentifier MakeIDShard(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameServer(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameClient(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameCoordinator();
	static InterLinkIdentifier MakeIDCartograph();

	std::string ToString() const;
	std::array<std::byte, 32> ToEncodedByteStream() const;
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromString(const std::string& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(
		const std::array<std::byte, 32>& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(
		const std::byte* data, size_t size);

	bool operator==(const InterLinkIdentifier& other) const noexcept

	{
		return ToString() == other.ToString();
	}
	bool operator<(const InterLinkIdentifier& other) const noexcept
	{
		return ToString() < other.ToString();
	}
	void Serialize(ByteWriter& bw) const
	{
		bw.write_scalar(Type);
		bw.str(ID);
	}
	void Deserialize(ByteReader& br)
	{
		Type = br.read_scalar<InterlinkType>();
		ID = br.str();
	}
};

namespace std
{
template <>
struct hash<InterLinkIdentifier>
{
	size_t operator()(const InterLinkIdentifier& key) const noexcept
	{
		return std::hash<int>()(static_cast<int>(key.Type)) ^ std::hash<std::string>()(key.ID);
	}
};
}  // namespace std
