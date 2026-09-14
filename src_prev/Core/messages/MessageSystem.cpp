#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/network/address/Address.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/events/MessagingEvents.hpp"
#include "atlasnet/core/utils/assert.hpp"

#include "atlasnet/core/messages/HandshakePacket.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/messages/MessageStructs.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/tasks/TaskHandle.hpp"
#include "boost/container/small_vector.hpp"
#include "boost/describe/enum_to_string.hpp"
#include "enviroment/Enviroment.hpp"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include "taskflow/core/async_task.hpp"
#include "taskflow/core/taskflow.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <vector>

AtlasNet::MessageSystem::MessageSystem(const Config& config) : config_(config)
{
  AN_ASSERT(config_.taskSystem, "JobSystem must be provided");
  if (config_.handshakeHandler)
  {
    logger->info("Handshake handler provided. Incoming connections will be "
                 "subject to handshake validation.");
  }
  else
  {
    logger->warn("No handshake handler provided. All incoming connections "
                 "will be accepted by default.");
  }
  SteamNetworkingIdentity identity;
  if (config_.handshakeIdentity)
  {
    ByteWriter handshakeWriter;
    config_.handshakeIdentity->Serialize(handshakeWriter);
    bool success = identity.SetGenericBytes(handshakeWriter.data(),
                                            handshakeWriter.size());
    AN_ASSERT(success,
              "Failed to set SteamNetworkingIdentity with handshake data");
    if (!success)
    {
      throw std::runtime_error(
          "Failed to set SteamNetworkingIdentity with handshake data");
    }
  }

  SteamNetworkingErrMsg errMsg = {0};
  const bool result = GameNetworkingSockets_Init(&identity, errMsg);
  if (errMsg[0] != '\0')
  {
    logger->error("GameNetworkingSockets_Init message: {}", errMsg);
  }
  if (!result)
  {
    throw std::runtime_error(
        std::string("Failed to initialize GameNetworkingSockets: ") + errMsg);
  }

  SteamNetworkingUtils()->SetDebugOutputFunction(
      k_ESteamNetworkingSocketsDebugOutputType_Msg,
      [](ESteamNetworkingSocketsDebugOutputType, const char* pszMsg)
      { spdlog::info("GNS Debug: {}", pszMsg); });

  _GNS = SteamNetworkingSockets();

  _pollGroup = GNS().CreatePollGroup();
  if (_pollGroup == k_HSteamNetPollGroup_Invalid)
  {
    GameNetworkingSockets_Kill();
    throw std::runtime_error("Failed to create SteamNetworking poll group");
  }

  _pollThread = std::jthread(
      [this]()
      {
        while (!shutdown.load(std::memory_order_acquire))
        {
          GNS().RunCallbacks();
          _Parse_Incoming_Messages();
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      });
}

AtlasNet::MessageSystem::ListenSocketHandle&
AtlasNet::MessageSystem::OpenListenSocket(Network::PortType port)
{
  std::unique_lock lock(_mutex);

  if (_listenSockets.contains(port))
  {
    throw std::runtime_error("Listen socket already open");
  }

  SteamNetworkingIPAddr localAddr;
  localAddr.Clear();
  localAddr.m_port = port;

  SteamNetworkingConfigValue_t opts[2];
  opts[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                 (void*)&OnSteamNetConnectionStatusChanged);
  opts[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, (int64_t)this);

  const HSteamListenSocket listenSocket =
      GNS().CreateListenSocketIP(localAddr, 2, opts);

  if (listenSocket == k_HSteamListenSocket_Invalid)
  {
    throw std::runtime_error("Failed to create listen socket on port " +
                             std::to_string(port));
  }
  logger->info("Opened listen socket on port {}", port);

  auto inserted = _listenSockets.emplace(
      port, std::make_unique<ListenSocketHandle>(*this, listenSocket, port));

  return *inserted.first->second;
}

AtlasNet::MessageSystem::ListenSocketHandle&
AtlasNet::MessageSystem::GetListenSocket(Network::PortType port)
{
  std::shared_lock lock(_mutex);

  auto it = _listenSockets.find(port);
  if (it == _listenSockets.end())
  {
    throw std::runtime_error("Listen socket not open");
  }

  return *it->second;
}

void AtlasNet::MessageSystem::SteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  if (!pInfo)
    return;

  switch (pInfo->m_info.m_eState)
  {
  case k_ESteamNetworkingConnectionState_Connecting:
    OnConnectionStatus_Connecting(pInfo);
    break;

  case k_ESteamNetworkingConnectionState_Connected:
    OnConnectionStatus_Connected(pInfo);
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
    OnConnectionStatus_ClosedByPeer(pInfo);
    break;

  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
    OnConnectionStatus_ProblemDetectedLocally(pInfo);

    break;

  case k_ESteamNetworkingConnectionState_None:
    logger->info("k_ESteamNetworkingConnectionState_None for: {}",
                 pInfo->m_info.m_szConnectionDescription);

    break;
  case k_ESteamNetworkingConnectionState_FindingRoute:
  case k_ESteamNetworkingConnectionState_FinWait:
  case k_ESteamNetworkingConnectionState_Linger:
  case k_ESteamNetworkingConnectionState_Dead:
  case k_ESteamNetworkingConnectionState__Force32Bit:
  {
    Network::SocketAddress address(pInfo->m_info.m_addrRemote);
    throw std::runtime_error(
        std::format("Unexpected connection state {} for {}",
                    (int)pInfo->m_info.m_eState, address.to_string()));
  }
  break;
  }
}

AtlasNet::ConnectionState
AtlasNet::MessageSystem::GetConnectionState(const Network::SocketAddress& address) const
{
  std::shared_lock lock(_mutex);
  auto connection = __FindByAddress(address);
  if (connection)
  {
    const HSteamNetConnection handle = connection->GetHandle();
    SteamNetConnectionInfo_t info;
    if (handle != k_HSteamNetConnection_Invalid &&
        GNS().GetConnectionInfo(handle, &info))
    {
      return static_cast<ConnectionState>(info.m_eState);
    }
  }

  return ConnectionState::eNone;
}

bool AtlasNet::MessageSystem::IsConnectingTo(const Network::SocketAddress& address) const
{
  return GetConnectionState(address) == ConnectionState::eConnecting;
}

bool AtlasNet::MessageSystem::IsConnectedTo(const Network::SocketAddress& address) const
{
  return GetConnectionState(address) == ConnectionState::eConnected;
}

AtlasNet::TaskHandle<AtlasNet::MessageConnectionResult>
AtlasNet::MessageSystem::Connect(const Network::SocketAddress& address)
{

  assert(address.IsValid() && "Invalid address provided to Connect()");
  {
    std::shared_lock lock(_mutex);

    // Reuse an existing connect job if one is already tracked.
    auto existingJobIt = _connectJobs.find(address);
    if (existingJobIt != _connectJobs.end())
    {
      return existingJobIt->second;
    }

    // If already connected, return a completed no-op job so callers can
    // still safely chain on_complete() if they want.

    auto connection = __FindByAddress(address);
    if (connection && connection->GetState() == ConnectionState::eConnected)
    {
      return config_.taskSystem->HighPriority().dependent_async(
          []() -> MessageConnectionResult
          {
            return MessageConnectionResult{
                MessageConnectionResultCode::eAlreadyConnected};
          });
    }
  }

  // Submit outside any long-lived write logic? Normally yes.
  // But to avoid duplicate jobs for the same address, we still need to
  // serialize the "check + insert" with a unique lock before returning.
  std::unique_lock lock(_mutex);

  // Double-check after upgrading.
  auto existingJobIt = _connectJobs.find(address);
  if (existingJobIt != _connectJobs.end())
  {
    return existingJobIt->second;
  }

  auto connection = __FindByAddress(address);
  if (connection && connection->GetState() == ConnectionState::eConnected)
  {

    return config_.taskSystem->HighPriority().dependent_async(
        []() -> MessageConnectionResult
        {
          return MessageConnectionResult{
              MessageConnectionResultCode::eAlreadyConnected};
        });
  }
  auto connectTask = config_.taskSystem->HighPriority().dependent_async(
      [this, address = address]() -> MessageConnectionResult
      {
        assert(address.IsValid() && "Invalid address provided to Connect()");
        logger->info("Starting connection to {}", address.to_string());
        SteamNetworkingConfigValue_t opts[2];
        opts[0].SetPtr(
            k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
            (void*)&OnSteamNetConnectionStatusChanged);
        opts[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData,
                         (int64_t)this);

        SteamNetworkingIPAddr steamAddr = address.ToSteamAddr();

        const HSteamNetConnection con =
            GNS().ConnectByIPAddress(steamAddr, 2, opts);

        if (con == k_HSteamNetConnection_Invalid)
        {
          char errMsg[1024] = {};
          steamAddr.ToString(errMsg, sizeof(errMsg), true);
          logger->error("Failed to connect to {}", errMsg);
          return MessageConnectionResult{
              MessageConnectionResultCode::eFailedToConnect};
        }

        if (!GNS().SetConnectionPollGroup(con, _pollGroup))
        {
          GNS().CloseConnection(con, 0, "Failed to assign poll group", false);
          logger->error("Failed to assign outgoing connection to poll group");
          return MessageConnectionResult{
              MessageConnectionResultCode::eFailedToConnect};
        }
        {
          std::unique_lock lock(_mutex);
          Connection conn(*this, con);
          conn.RequestedAddress = address;
          conn.ResolvedAddress = address.EnsureResolved();
          conn.connState = ConnectionState::eConnecting;

          auto it = _connections.find(address);
          if (it == _connections.end())
          {
            __InsertConnection(conn);
          }
          else
          {
            __ModifyByAddress(
                address, [&](Connection& existingConn)
                { existingConn.connState = ConnectionState::eConnecting; });
          }
          logger->info("Tracked new outgoing connection to {}",
                       address.to_string());
        }
        return MessageConnectionResult{
            MessageConnectionResultCode::eConnecting};
      });

  auto WaitForConnectTask = config_.taskSystem->HighPriority().dependent_async(
      [this, address]() -> MessageConnectionResult
      {
        ConnectionState state = GetConnectionState(address);

        while (state == ConnectionState::eConnecting ||
               state == ConnectionState::eNone)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
          state = GetConnectionState(address);
        }

        MessageConnectionResultCode resultCode;
        switch (state)
        {
        case ConnectionState::eConnected:
          resultCode = MessageConnectionResultCode::eSuccess;
          break;
        case ConnectionState::eClosedByPeer:
          resultCode = MessageConnectionResultCode::eFailedToConnect;
          break;
        case ConnectionState::eProblemDetectedLocally:
          resultCode = MessageConnectionResultCode::eFailedToConnect;
          break;
        default:
          resultCode = MessageConnectionResultCode::eFailedToConnect;
          break;
        }
        std::unique_lock lock(_mutex);
        _connectJobs.erase(address);
        return MessageConnectionResult{resultCode};
      },
      connectTask.first);

  auto ins = _connectJobs.emplace(address, std::move(WaitForConnectTask));
  return ins.first->second;
}

void AtlasNet::MessageSystem::Shutdown()
{
  if (shutdown.exchange(true, std::memory_order_acq_rel))
  {
    return;
  }
  _pollThread.request_stop();
  _pollThread.join();

  logger->info("Shutting down MessageSystem...");

  // If your JobSystem supports cancellation, do it here.
  // Example:
  // _pollJobHandle.Cancel();
  //
  // And also cancel/wait any connect jobs in _connectJobs.

  std::vector<HSteamNetConnection> connectionHandles;
  std::vector<HSteamListenSocket> listenHandles;

  {
    std::unique_lock lock(_mutex);

    connectionHandles.reserve(_connections.size());
    for (const auto& connection : _connections)
    {

      connectionHandles.push_back(connection.GetHandle());
    }
    _connections.clear();

    listenHandles.reserve(_listenSockets.size());
    for (const auto& [port, listenSocket] : _listenSockets)
    {
      (void)port;
      if (listenSocket)
      {
        listenHandles.push_back(listenSocket->handle);
      }
    }
    _listenSockets.clear();

    _connectJobs.clear();
  }

  for (HSteamNetConnection handle : connectionHandles)
  {
    if (handle != k_HSteamNetConnection_Invalid)
    {
      GNS().CloseConnection(handle, 0, "Shutting down", false);
    }
  }

  for (HSteamListenSocket handle : listenHandles)
  {
    if (handle != k_HSteamListenSocket_Invalid)
    {
      GNS().CloseListenSocket(handle);
    }
  }

  if (_pollGroup != k_HSteamNetPollGroup_Invalid)
  {
    GNS().DestroyPollGroup(_pollGroup);
    _pollGroup = k_HSteamNetPollGroup_Invalid;
  }

  GameNetworkingSockets_Kill();
  logger->info("MessageSystem shutdown complete.");
}

AtlasNet::MessageSystem::~MessageSystem()
{
  Shutdown();
}

void AtlasNet::MessageSystem::SetIdentity(
    const SteamNetworkingIdentity& identity)
{
  GNS().ResetIdentity(&identity);
}

void AtlasNet::MessageSystem::ListenSocketHandle::DispatchCallbacks(
    const IMessage& message, MessageID typeIdHash,
    const Network::SocketAddress& caller_address)
{
  HandlerFunc dispatcher;
  bool found = false;

  {
    std::shared_lock lock(socket_mutex);
    auto it = _handlers.find(typeIdHash);
    if (it != _handlers.end())
    {
      dispatcher = it->second;
      found = true;
    }
  }

  if (found)
  {
    system.logger->info("Dispatching message of type hash {} received on "
                        "listen socket port {} to socket dispatcher",
                        typeIdHash, port);
    dispatcher(message, caller_address);
  }
  else
  {
    system.logger->warn(
        "No dispatcher registered for message type with hash {}", typeIdHash);
  }
}
void AtlasNet::MessageSystem::MessageSystem::_Parse_Incoming_Messages()
{

  ISteamNetworkingMessage* pIncomingMessages[32] = {};
  const int numMsgs = GNS().ReceiveMessagesOnPollGroup(
      _pollGroup, pIncomingMessages,
      static_cast<int>(std::size(pIncomingMessages)));

  if (numMsgs < 0)
  {
    logger->error("ReceiveMessagesOnPollGroup failed");
    return;
  }

  struct MessageInfo
  {
    Network::SocketAddress from;
    std::optional<Network::PortType> port_received_on;
  };
  boost::container::small_vector<MessageInfo, 32> messageInfos;
  boost::container::small_vector<std::vector<uint8_t>, 32> MessageBuffers;

  /* parses each message and copies the data, releases the message*/
  auto parseMessagesTask = config_.taskSystem->MediumPriority().dependent_async(
      [this, numMsgs, &pIncomingMessages, &messageInfos, &MessageBuffers]()
      {
        for (int i = 0; i < numMsgs; ++i)
        {
          ISteamNetworkingMessage* msg = pIncomingMessages[i];
          if (!msg)
            continue;

          SteamNetConnectionInfo_t info;
          if (!GNS().GetConnectionInfo(msg->m_conn, &info))
          {
            logger->error("Failed to get connection info for incoming message");
            msg->Release();
            continue;
          }
          std::optional<Network::PortType> ListenSocketPortReceivedOn = std::nullopt;
          if (info.m_hListenSocket != k_HSteamListenSocket_Invalid)
          {
            SteamNetworkingIPAddr listenAddr;
            if (GNS().GetListenSocketAddress(info.m_hListenSocket, &listenAddr))
            {
              ListenSocketPortReceivedOn = listenAddr.m_port;
            }
          }

          std::optional<Network::SocketAddress> addressRemote;
          if (info.m_addrRemote.IsIPv4())
          {
            const uint32 ip4Packed = info.m_addrRemote.GetIPv4();
            const uint16 port = info.m_addrRemote.m_port;
            addressRemote = Network::SocketAddress(Network::IPv4(ip4Packed), port);
          }
          else
          {
            addressRemote = Network::SocketAddress(Network::IPv6(info.m_addrRemote.m_ipv6),
                                          info.m_addrRemote.m_port);
            logger->info("Incoming message from {}",
                         addressRemote->to_string());
          }
          AN_ASSERT(addressRemote->IsValid(),
                    "Invalid remote address in incoming message");

          MessageInfo infoStruct{.from = *addressRemote,
                                 .port_received_on =
                                     ListenSocketPortReceivedOn};
          messageInfos.push_back(infoStruct);
          MessageBuffers.emplace_back();
          MessageBuffers.back().insert(
              MessageBuffers.back().end(),
              static_cast<const uint8_t*>(msg->m_pData),
              static_cast<const uint8_t*>(msg->m_pData) +
                  static_cast<std::size_t>(msg->m_cbSize));

          msg->Release();
        }
      });
  // Only wait on the parsing, no need to wait on the dispatching, as they are
  // all dependent on the parsing task
  parseMessagesTask.second.wait();
  /*Dispatch each message in parallel while owning their messageData*/
  for (int messageIndex = 0; messageIndex < numMsgs; ++messageIndex)
  {
    auto task = config_.taskSystem->MediumPriority().silent_dependent_async(
        [this, messageData = std::move(MessageBuffers[messageIndex]),
         messageInfo = messageInfos[messageIndex]]()
        {
          ByteReader readerID(messageData);

          const MessageID typeIdHash =
              IMessage::DeserializeTypeIdHash(readerID);

          logger->info("Message of type hash: {} {}", typeIdHash,
                       messageInfo.port_received_on.has_value()
                           ? std::format("on listen socket port {}",
                                         *messageInfo.port_received_on)
                           : "on client port");

          // Copy dispatcher out while holding the lock, then invoke
          // unlocked.
          DispatchFunc dispatcher;
          bool found = false;
          {
            std::shared_lock lock(_mutex);
            auto it = _dispatchTable.find(typeIdHash);
            if (it != _dispatchTable.end())
            {
              dispatcher = it->second;
              found = true;
            }
          }

          if (found)
          {

            ByteReader readerFull(messageData);
            AN_ASSERT(dispatcher != nullptr,
                      "Dispatcher should not be null here");
            dispatcher(readerFull, messageInfo.from,
                       messageInfo.port_received_on);
          }
        });
  }
}
HSteamNetConnection
AtlasNet::MessageSystem::GetConnectionHandle(const Network::SocketAddress& address) const
{
  std::shared_lock lock(_mutex);

  auto connection = __FindByAddress(address);
  if (connection)
  {
    return connection->GetHandle();
  }
  logger->error("Connection handle requested for non-existent connection to {}",
                address.to_string());
  logger->error("Known connections:");
  for (const auto& connection : _connections)
  {
    logger->error(" - {}", connection.RequestedAddress.to_string());
  }
  throw std::runtime_error("Connection not found");
}
std::optional<AtlasNet::MessageSystem::Connection>
AtlasNet::MessageSystem::GetConnection(const Network::SocketAddress& address) const
{
  std::shared_lock lock(_mutex);
  auto connection = __FindByAddress(address);
  if (connection)
  {
    return connection;
  }
  return std::nullopt;
}
void AtlasNet::MessageSystem::GetConnections(
    std::vector<Connection>& connections) const
{
  connections.clear();
  std::shared_lock lock(_mutex);
  for (const auto& connection : _connections)
  {
    connections.push_back(connection);
  }
}
size_t AtlasNet::MessageSystem::GetNumConnections() const
{
  std::shared_lock lock(_mutex);
  return _connections.size();
}
AtlasNet::MessageSystem::ListenSocketHandle::ListenSocketHandle(
    MessageSystem& system, HSteamListenSocket handle, Network::PortType port)
    : system(system), handle(handle), port(port)
{
}
void AtlasNet::MessageSystem::Connection::SendMessage(
    const void* data, uint32_t size, MessageSendMode mode) const
{
  system.GNS().SendMessageToConnection(handle, data, size,
                                       static_cast<int>(mode), nullptr);
}
bool AtlasNet::MessageSystem::attempt_handshake(
    const SteamNetworkingIdentity& identity,
    HandshakeIdentity& handshakeIdentity)
{
  if (config_.handshakeHandler)
  {
    if (identity.IsInvalid())
    {
      logger->error("Received connection with invalid identity, rejecting.");
      return false;
    }

    HandshakeIdentity handshakeIdentity;
    try
    {
      int cbLen;
      const uint8_t* bytes = identity.GetGenericBytes(cbLen);
      ByteReader reader(std::span<const uint8_t>(bytes, cbLen));
      handshakeIdentity.Deserialize(reader);
    }
    catch (const std::exception& e)
    {
      logger->error("Failed to parse handshake identity: {}", e.what());
      return false;
    }
    HandshakeResponsePacket response =
        config_.handshakeHandler(handshakeIdentity, Network::SocketAddress());
    return response.accepted;
  }
  return true; // accept by default if no handler provided
}
void AtlasNet::MessageSystem::OnConnectionStatus_Connecting(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  Network::SocketAddress address(pInfo->m_info.m_addrRemote);
  logger->info("k_ESteamNetworkingConnectionState_Connecting: {}",
               address.to_string());
  const bool isIncoming =
      (pInfo->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid);

  /* ==================== EVENTS ==================*/
  if (config_.localEventSystem)
  {
    if (isIncoming)
    {
      SteamNetworkingIPAddr localAddr;
      GNS().GetListenSocketAddress(pInfo->m_info.m_hListenSocket, &localAddr);
      Network::SocketAddress remoteAddr(pInfo->m_info.m_addrRemote);
      ConnectionRequestReceivedEvent event;
      event.remoteAddr = remoteAddr;
      event.localPort = localAddr.m_port;
      config_.localEventSystem->Emit(event);
    }
    else
    {
      ConnectionStartedInternallyEvent event;
      Network::SocketAddress remoteAddr(pInfo->m_info.m_addrRemote);
      event.address = remoteAddr;
      config_.localEventSystem->Emit(event);
    }
  }
  std::optional<HandshakeIdentity> remoteHandshakeIdentity;
  /* =================  HANDSHAKE ================== */
  if (config_.handshakeHandler)
  {
    // only check for handshake if incoming because if we initiated then the
    // identity is not available to us yet
    if (isIncoming)
    {

      /* if the identity of the other is invalid then reject*/
      if (pInfo->m_info.m_identityRemote.IsInvalid() && isIncoming)
      {
        logger->warn("Incoming connection from {} has invalid identity. "
                     "Rejecting connection.",
                     address.to_string());
        GNS().CloseConnection(pInfo->m_hConn,
                              (int)HandshakeResponseCode::eReject,
                              "Invalid identity", false);
        return;
      }
      else /*otherwise try parse*/
      {

        try
        {
          int cbLen = 0;
          const uint8_t* handshakeData =
              pInfo->m_info.m_identityRemote.GetGenericBytes(cbLen);
          ByteReader reader(std::span<const uint8_t>(handshakeData, cbLen));
          remoteHandshakeIdentity.emplace();
          remoteHandshakeIdentity->Deserialize(reader);
        }
        catch (const std::exception& e)
        {
          logger->error("Failed to parse handshake identity packet from {}: "
                        "{}. Rejecting connection.",
                        address.to_string(), e.what());
          GNS().CloseConnection(pInfo->m_hConn,
                                (int)HandshakeResponseCode::eReject,
                                "Failed to parse handshake data", false);
          remoteHandshakeIdentity.reset();
          return;
        }
        /* call the handshake handler*/
        if (HandshakeResponsePacket response =
                config_.handshakeHandler(*remoteHandshakeIdentity, address);
            !response.accepted)
        {
          logger->warn("Handshake rejected for connection from {}. Closing "
                       "connection. Reason: {}",
                       address.to_string(), response.rejectReason);
          GNS().CloseConnection(pInfo->m_hConn,
                                (int)HandshakeResponseCode::eReject,
                                response.rejectReason.c_str(), false);
          return;
        }
        else
        {
          logger->info("Handshake accepted for connection from {}. Proceeding "
                       "with connection.",
                       address.to_string());
        }
      }
    }
  }

  /* Accept the connection*/
  if (isIncoming)
  {

    GNS().SetConnectionUserData(pInfo->m_hConn, (int64)this);
    logger->info("Incoming connection from {} on listen socket {}",
                 address.to_string(), pInfo->m_info.m_hListenSocket);

    const EResult r = GNS().AcceptConnection(pInfo->m_hConn);
    if (r != k_EResultOK)
    {
      logger->error("AcceptConnection failed: {} for {}", static_cast<int>(r),
                    pInfo->m_info.m_szConnectionDescription);

      GNS().CloseConnection(pInfo->m_hConn, 0, "AcceptConnection failed",
                            false);
      return;
    }

    if (!GNS().SetConnectionPollGroup(pInfo->m_hConn, _pollGroup))
    {
      logger->error("Failed to assign accepted connection to poll group");
      GNS().CloseConnection(pInfo->m_hConn, 0, "Failed to assign poll group",
                            false);
      return;
    }

    {
      std::unique_lock lock(_mutex);

      Connection conn(*this, pInfo->m_hConn);
      conn.authState = AuthState::eUnknown;
      conn.connState = ConnectionState::eConnecting;
      conn.handshakeIdentity = remoteHandshakeIdentity;
      conn.RequestedAddress = address;
      conn.ResolvedAddress = address.EnsureResolved();
      auto existing = __FindByAddress(address);
      if (!existing)
      {
        __InsertConnection(conn);
      }
      else
      {
        __ModifyByAddress(address, [](Connection& conn)
                          { conn.connState = ConnectionState::eConnecting; });
      }
    }
    if (config_.localEventSystem)
    {
      ConnectionAcceptedInternallyPreHandshakeEvent event;
      event.address = address;
      config_.localEventSystem->Emit(event);
    }
  }
  else
  {
    logger->info("Outgoing connection (initiated locally) to {}",
                 address.to_string());
  }
}
void AtlasNet::MessageSystem::OnConnectionStatus_Connected(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  Network::SocketAddress address(pInfo->m_info.m_addrRemote);
  logger->info("k_ESteamNetworkingConnectionState_Connected: {}",
               address.to_string());
  const bool isIncoming =
      (pInfo->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid);

  if (!isIncoming)
  {
    HandshakeIdentity handshakeIdentity;
    bool handshakeresult =
        attempt_handshake(pInfo->m_info.m_identityRemote, handshakeIdentity);
    if (!handshakeresult)
    {
      logger->error(
          "Handshake failed for outgoing connection to {}. Closing connection.",
          address.to_string());
      GNS().CloseConnection(pInfo->m_hConn, (int)HandshakeResponseCode::eReject,
                            "Handshake failed", false);
      std::unique_lock lock(_mutex);
      const bool success =
          __ModifyByAddress(address,
                            [](Connection& conn)
                            {
                              conn.connState =
                                  ConnectionState::eClosedByLocalHost;
                              // conn.authState = AuthState::eHandshakePending;
                            });

      /*  else
       {
         // Defensive: callback may win race against insertion path.
         Connection conn(*this, pInfo->m_hConn);
         conn.connState = ConnectionState::eClosedByLocalHost;
         conn.RequestedAddress = address;
         conn.ResolvedAddress = address.EnsureResolved();
         // conn.authState = AuthState::eHandshakePending;
         __InsertConnection(conn);
       } */
      return;
    }
    std::unique_lock lock(_mutex);

    bool modified =
        __ModifyByAddress(address,
                          [&](Connection& conn)
                          {
                            conn.connState = ConnectionState::eConnected;
                            conn.handshakeIdentity = handshakeIdentity;
                          });

    if (!modified)
    {
      logger->error("Internal failure: connection not found after handshake "
                    "for address {}",
                    address.to_string());
      for (const auto& conn : _connections)
      {
        logger->error("Existing connection: {}",
                      conn.RequestedAddress.to_string());
      }
      assert(false);
      throw std::runtime_error(
          "Internal failure: connection not found after handshake");
    }
  }
  if (config_.localEventSystem)
  {
    std::optional<HandshakeIdentity> handshakeIdentity;
    {
      std::shared_lock lock(_mutex);
      auto existing = __FindByAddress(address);
      if (existing)
      {
        handshakeIdentity = existing->handshakeIdentity;
      }
    }
    ConnectionEstablishedEvent event;
    event.address = address;
    if (config_.handshakeHandler)
    {
      event.source = handshakeIdentity->role == HandshakeRole::eClient
                         ? ConnectionSource::External
                         : ConnectionSource::Internal;
    }
    else
    {
      event.source = ConnectionSource::Unverified;
    }

    config_.localEventSystem->Emit(event);
  }
  logger->info("Connection established: {}",
               pInfo->m_info.m_szConnectionDescription);
}
void AtlasNet::MessageSystem::OnConnectionStatus_ClosedByPeer(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  Network::SocketAddress address(pInfo->m_info.m_addrRemote);
  std::unique_lock lock(_mutex);
  __ModifyByAddress(address, [](Connection& conn)
                    { conn.connState = ConnectionState::eClosedByPeer; });

  logger->info("Connection closed by peer: {}",
               pInfo->m_info.m_szConnectionDescription);
}

void AtlasNet::MessageSystem::OnConnectionStatus_ProblemDetectedLocally(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  Network::SocketAddress address(pInfo->m_info.m_addrRemote);
  std::unique_lock lock(_mutex);
  __ModifyByAddress(
      address, [](Connection& conn)
      { conn.connState = ConnectionState::eProblemDetectedLocally; });
  logger->warn("Problem detected locally: {}",
               pInfo->m_info.m_szConnectionDescription);
}
