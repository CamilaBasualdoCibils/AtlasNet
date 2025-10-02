#pragma once
#include <pch.hpp>
enum class InterlinkType
{
	eInvalid = -1,
    eUnknown, 
	ePartition,	 // partition Server
	eGod,		 // God
	eGodView,	 // God Debug Tool
	eGameClient, // Game Client / Player
	eGameServer 	 // Game Server
};
BOOST_DESCRIBE_ENUM(InterlinkType,eInvalid,ePartition,eGod,eGodView,eGameClient,eGameServer)

enum class ConnectionState
{
	eInvalid,		/// Invalid state
    ePreConnecting, /// The Connection request has not been sent yet
	eConnecting,	/// This Connection is trying to connect
	eConnected,		/// Successfully Connected
	eConnectedVerifyingIndentity, /// Successfully Connected but now asking credentials and user for permission before continuing
	eDisconnecting, /// Trying to disconnect
	eClosed,
	eError
};

/// @brief Packet Types Internal to InterLink
enum InterlinkPacketType
{
	eDataPacket, /// Packet with data to be given to host of Interlink, contains data foreign to
				 /// InterLink,
	eRelay, /// this packet should or has been redirected
};