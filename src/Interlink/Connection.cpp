#include "Connection.hpp"

IPAddress::IPAddress()
{
	SteamAddress.Clear();
}

IPAddress::IPAddress(SteamNetworkingIPAddr addr)
{
	SteamAddress.Clear();
	SteamAddress = addr;
}

IPAddress IPAddress::MakeLocalHost(PortType port)
{
	IPAddress address;
	address.SetLocalHost(port);
	return address;
}
void IPAddress::SetLocalHost(PortType port)
{
	SetIPv4(127, 0, 0, 1, port);
	ASSERT(SteamAddress.IsLocalHost(), "Fuck?");
}

void IPAddress::SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port)
{
	SteamAddress.SetIPv4((a << 24) | (b << 16) | (c << 8) | d, port);
}

std::string IPAddress::ToString(bool IncludePort) const
{
	std::string str;
	str.resize(SteamAddress.k_cchMaxString);
	SteamAddress.ToString(str.data(), str.size(), IncludePort);
	return str;
}

SteamNetworkingIPAddr IPAddress::ToSteamIPAddr() const
{
	return SteamAddress;
}

void Connection::SetNewState(ConnectionState newState)
{
	oldState = state;
	state = newState;
}
