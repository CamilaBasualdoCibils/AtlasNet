#pragma once
#include <pch.hpp>
#include "InterlinkEnums.hpp"

struct InterLinkIdentifier
{
	InterlinkType Type = InterlinkType::eInvalid;
	uint32 ID = -1;

  public:
	InterLinkIdentifier() = default;
	InterLinkIdentifier(InterlinkType _Type,uint32_t _ID = -1)
	{
		Type = _Type;
		ID = _ID;
	}
	static InterLinkIdentifier MakeIDGod();
	static InterLinkIdentifier MakeIDPartition(uint32 _ID);
	static InterLinkIdentifier MakeIDGameServer(uint32 _ID);
	static InterLinkIdentifier MakeIDGameClient(uint32 _ID);
	static InterLinkIdentifier MakeIDGodView();
    std::string ToString() const;
    static std::optional<InterLinkIdentifier> FromString(const std::string& input);
};