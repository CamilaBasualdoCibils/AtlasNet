#pragma once

#include <cstdint>

#include "Client/Client.hpp"
#include "Entity/Entity.hpp"

struct NetCommandHeader
{
};
class INetCommand
{
	NetCommandHeader header;
};
struct NetClientIntentHeader
{
	ClientID clientID;
	AtlasEntityID entityID;
};

class IClientIntentCommand : public INetCommand
{
	NetClientIntentHeader ClientIntentHeader;
};
struct NetServerStateHeader
{
	uint64_t ServerTick;
};
class IServerStateCommand : public INetCommand
{
	NetServerStateHeader ServerStateHeader;
};