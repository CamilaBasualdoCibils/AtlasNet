#pragma once
#include "AtlasNet/Core/Network/Transport/Connection/IConnection.hpp"
#include "AtlasNet/Core/Network/Transport/Datagram/IDatagramTransport.hpp"
namespace AtlasNet::Network
{
class DatagramConnection : public IConnection
{
  std::shared_ptr<IDatagramTransport> transport;

public:
  DatagramConnection(std::shared_ptr<IDatagramTransport> transport)
      : transport(transport)
  {
  }
};
} // namespace AtlasNet::Network