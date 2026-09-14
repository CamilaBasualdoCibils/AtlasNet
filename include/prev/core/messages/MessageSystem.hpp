#pragma once
#include "Message.hpp"
#include "atlasnet/core/network/address/SocketAddress.hpp"
#include "atlasnet/core/events/LocalEventSystem.hpp"
#include "atlasnet/core/utils/assert.hpp"

#include "atlasnet/core/messages/HandshakePacket.hpp"
#include "atlasnet/core/messages/MessageStructs.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/tasks/TaskHandle.hpp"
#include "atlasnet/core/tasks/TaskSystem.hpp"
#include "boost/describe/enum_to_string.hpp"
#include "boost/multi_index/hashed_index.hpp"
#include "boost/multi_index/indexed_by.hpp"
#include "boost/multi_index/member.hpp"
#include "boost/multi_index_container.hpp"
#include "boost/multi_index_container_fwd.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include "taskflow/core/async_task.hpp"
#include <atomic>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unordered_map>
namespace AtlasNet
{
static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info);
enum class MessageSendMode
{
  eNoDelay = k_nSteamNetworkingSend_UnreliableNoDelay,
  eReliable = k_nSteamNetworkingSend_ReliableNoNagle,
  eReliableBatched = k_nSteamNetworkingSend_Reliable,
  eUnreliable = k_nSteamNetworkingSend_UnreliableNoNagle,
  eUnreliableBatched = k_nSteamNetworkingSend_Unreliable,
  eINVALID,
};
BOOST_DESCRIBE_ENUM(MessageSendMode, eNoDelay, eReliable, eReliableBatched, eUnreliable,
                    eUnreliableBatched, eINVALID)
enum class ConnectionState
{
  eNone = k_ESteamNetworkingConnectionState_None,
  eConnecting = k_ESteamNetworkingConnectionState_Connecting,
  eConnected = k_ESteamNetworkingConnectionState_Connected,
  eClosedByPeer = k_ESteamNetworkingConnectionState_ClosedByPeer,
  eClosedByLocalHost,
  eProblemDetectedLocally =
      k_ESteamNetworkingConnectionState_ProblemDetectedLocally
};
BOOST_DESCRIBE_ENUM(ConnectionState, eNone, eConnecting, eConnected,
                    eClosedByPeer, eProblemDetectedLocally)
enum class AuthState
{
  eUnknown,
  eHandshakePendingReq,
  eHandshakePendingRes,
  eHandshakePendingAck,
  eHandshakePendingFinal,
  eAuthenticating,
  eAuthenticatedClient,
  eAuthenticatedServer,
  eRejected
};

class MessageSystem
{
public:
  class Connection
  {
    MessageSystem& system;
    HSteamNetConnection handle;
    ConnectionState connState;
    AuthState authState;
    std::optional<HandshakeIdentity> handshakeIdentity;
    Network::SocketAddress RequestedAddress, ResolvedAddress;
    friend class MessageSystem;

  protected:
    Connection(MessageSystem& system, HSteamNetConnection handle)
        : system(system), handle(handle), connState(ConnectionState::eNone),
          authState(AuthState::eUnknown)
    {
    }

  public:
    HSteamNetConnection GetHandle() const
    {
      return handle;
    }
    ConnectionState GetState() const
    {
      return connState;
    }
    Network::SocketAddress GetRequestedAddress() const
    {
      return RequestedAddress;
    }
    Network::SocketAddress GetResolvedAddress() const
    {
      return ResolvedAddress;
    }
    void SendMessage(const void* data, uint32_t size,
                     MessageSendMode mode) const;
  };
  class ListenSocketHandle
  {
    MessageSystem& system;
    HSteamListenSocket handle;
    std::shared_mutex socket_mutex;
    Network::PortType port;
    using HandlerFunc =
        std::function<void(const IMessage&, const Network::SocketAddress&)>;
    std::unordered_map<MessageID, HandlerFunc> _handlers;
    // using DispatchFunc = std::function<void(const IMessage&, MessageID,
    //                                         const Network::SocketAddress&)>;
    // std::unordered_map<MessageID, DispatchFunc> _dispatchTable;
    friend class MessageSystem;

  protected:
    void DispatchCallbacks(const IMessage& message, MessageID typeIdHash,
                           const Network::SocketAddress& caller_address);

  public:
    ListenSocketHandle(MessageSystem& system, HSteamListenSocket handle,
                       Network::PortType port);
    template <typename MessageType>
      requires std::is_base_of_v<IMessage, MessageType>
    ListenSocketHandle&
    On(std::function<void(const MessageType&, const Network::SocketAddress&)> func);

  private:
    /*  template <typename MsgType>
       requires std::is_base_of_v<IMessage, MsgType>
     void _ensure_socket_message_dispatcher(); */
  };
  using HandshakeHandlerFunc = std::function<HandshakeResponsePacket(
      const HandshakeIdentity&, const Network::SocketAddress&)>;
  struct Config
  {
    TaskSystem* taskSystem = nullptr;
    LocalEventSystem* localEventSystem = nullptr;
    HandshakeHandlerFunc handshakeHandler = nullptr;
    std::optional<HandshakeIdentity> handshakeIdentity;
  };
  MessageSystem(const Config& config);

  ~MessageSystem();
  void Shutdown();

  TaskHandle<MessageConnectionResult> Connect(const Network::SocketAddress& address);

  /**
   * @brief Queues a message to be sent to the specified address. Will connect
   * if not already connected.
   *
   * @tparam MessageType
   * @param message
   * @param address
   * @param mode
   * @return requires
   */
  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  [[nodiscard]] TaskHandle<MessageSendResult>
  QueueMessage(const MessageType& message, const Network::SocketAddress& address,
               MessageSendMode mode);
  /**
   * @brief Tries to send a message immidiately, if not connected, will return
   * false. If connected, will send the message and return true.
   *
   * @tparam MessageType
   * @param message
   * @param address
   * @param mode
   * @return requires
   */
  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  [[nodiscard]] MessageSendResult TrySendMessage(const MessageType& message,
                                                 const Network::SocketAddress& address,
                                                 MessageSendMode mode);

  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  MessageSystem&
  On(std::function<void(const MessageType&, const Network::SocketAddress&)> handler);

  ListenSocketHandle& OpenListenSocket(Network::PortType port);
  ListenSocketHandle& GetListenSocket(Network::PortType port);

  void SteamNetConnectionStatusChanged(
      SteamNetConnectionStatusChangedCallback_t* pInfo);

  ConnectionState GetConnectionState(const Network::SocketAddress& address) const;

  bool IsConnectingTo(const Network::SocketAddress& address) const;
  bool IsConnectedTo(const Network::SocketAddress& address) const;

  size_t GetNumConnections() const;
  void GetConnections(std::vector<Connection>& connections) const;
  std::optional<Connection> GetConnection(const Network::SocketAddress& address) const;

private:
  void SetIdentity(const SteamNetworkingIdentity& identity);
  void _Parse_Incoming_Messages();
  ISteamNetworkingSockets& GNS() const
  {
    AN_ASSERT(_GNS, "GNS is null");
    return *_GNS;
  }
  HSteamNetConnection GetConnectionHandle(const Network::SocketAddress& address) const;
  template <typename MsgType>
    requires std::is_base_of_v<IMessage, MsgType>
  void _ensure_message_dispatcher();

  bool attempt_handshake(const SteamNetworkingIdentity& identity,
                         HandshakeIdentity& handshakeIdentity);

  void OnConnectionStatus_Connecting(
      SteamNetConnectionStatusChangedCallback_t* pInfo);
  void OnConnectionStatus_Connected(
      SteamNetConnectionStatusChangedCallback_t* pInfo);
  void OnConnectionStatus_ClosedByPeer(
      SteamNetConnectionStatusChangedCallback_t* pInfo);
  void OnConnectionStatus_ProblemDetectedLocally(
      SteamNetConnectionStatusChangedCallback_t* pInfo);
  const Config config_;
  ISteamNetworkingSockets* _GNS;
  HSteamNetPollGroup _pollGroup;
  mutable std::shared_mutex _mutex;
  /* struct ListenSocketByPort
  {
  };
  struct ListenSocketBySteamHandle
  {
  };
  boost::multi_index_container<
      std::unique_ptr<ListenSocketHandle>,
      boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<
              boost::multi_index::tag<ListenSocketByPort>,
              boost::multi_index::member<ListenSocketHandle, PortType,
                                         &ListenSocketHandle::port>>,
          boost::multi_index::hashed_unique<boost::multi_index::tag<ListenSocketBySteamHandle>,
                                            boost::multi_index::member<
                                                ListenSocketHandle,
                                                HSteamListenSocket,
                                                &ListenSocketHandle::handle>>>>
      _listenSocketList; */

  std::unordered_map<Network::PortType, std::unique_ptr<ListenSocketHandle>>
      _listenSockets;

  struct ConnectionByRequestedAddress
  {
  };
  struct ConnectionByResolvedAddress
  {
  };
  boost::multi_index_container<
      Connection,
      boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<
              boost::multi_index::tag<ConnectionByRequestedAddress>,
              boost::multi_index::member<Connection, Network::SocketAddress,
                                         &Connection::RequestedAddress>>,
          boost::multi_index::hashed_unique<
              boost::multi_index::tag<ConnectionByResolvedAddress>,
              boost::multi_index::member<Connection, Network::SocketAddress,
                                         &Connection::ResolvedAddress>>>>
      _connections;

  std::optional<Connection> __FindByAddress(const Network::SocketAddress& address) const
  {
    auto it1 = _connections.get<ConnectionByRequestedAddress>().find(address);
    if (it1 != _connections.get<ConnectionByRequestedAddress>().end())
    {
      return *it1;
    }
    auto it2 = _connections.get<ConnectionByResolvedAddress>().find(address);
    if (it2 != _connections.get<ConnectionByResolvedAddress>().end())
    {
      return *it2;
    }
    return std::nullopt;
  }
  bool __ModifyByAddress(const Network::SocketAddress& address,
                         std::function<void(Connection&)> func)
  {
    auto it1 = _connections.get<ConnectionByRequestedAddress>().find(address);
    if (it1 != _connections.get<ConnectionByRequestedAddress>().end())
    {
      _connections.get<ConnectionByRequestedAddress>().modify(it1, func);
      return true;
    }
    auto it2 = _connections.get<ConnectionByResolvedAddress>().find(address);
    if (it2 != _connections.get<ConnectionByResolvedAddress>().end())
    {
      _connections.get<ConnectionByResolvedAddress>().modify(it2, func);
      return true;
    }
    return false;
  }
  void __InsertConnection(const Connection& connection)
  {
    assert(connection.RequestedAddress.IsValid() &&
           connection.ResolvedAddress.IsValid() &&
           "Invalid connection addresses");
    _connections.insert(connection);
  }
  // std::unordered_map<Network::SocketAddress, Connection> _connections;
  std::unordered_map<Network::SocketAddress, TaskHandle<MessageConnectionResult>>
      _connectJobs;
  std::vector<TaskHandle<>> _waitingOnConnectionJobs;
  std::vector<TaskHandle<MessageSendResult>> _sendJobs;
  using HandlerFunc =
      std::function<void(const IMessage&, const Network::SocketAddress&)>;
  std::unordered_map<MessageID, HandlerFunc> _handlers;
  using DispatchFunc = std::function<void(ByteReader&, const Network::SocketAddress&,
                                          std::optional<Network::PortType>)>;
  std::unordered_map<MessageID, DispatchFunc> _dispatchTable;
  std::jthread _pollThread;
  std::atomic_bool shutdown = false;

  std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("MessageSystem");
};
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSystem::ListenSocketHandle& MessageSystem::ListenSocketHandle::On(
    std::function<void(const MessageType&, const Network::SocketAddress&)> func)
{
  system._ensure_message_dispatcher<MessageType>();
  //_ensure_socket_message_dispatcher<MessageType>();
  system.logger->info("Registered handler for message type with hash {} on "
                      "listen socket port {}",
                      MessageType::TypeIdHash, port);
  std::unique_lock lock(socket_mutex);
  MessageID typeIdHash = MessageType::TypeIdHash;
  AN_ASSERT(
      !_handlers.contains(typeIdHash),
      std::format("Handler already registered for message type with hash {}",
                  typeIdHash));
  _handlers[typeIdHash] =
      [h = std::move(func)](const IMessage& msg, const Network::SocketAddress& address)
  { h(static_cast<const MessageType&>(msg), address); };

  return *this;
}
/*
template <typename MsgType>
  requires std::is_base_of_v<IMessage, MsgType>
inline void
MessageSystem::ListenSocketHandle::_ensure_socket_message_dispatcher()
{
  MessageID typeIdHash = MsgType::TypeIdHash;
  {
    std::shared_lock lock(socket_mutex);
    if (_handlers.contains(typeIdHash))
      return;
  }

  std::unique_lock lock(socket_mutex);

  if (!_handlers.contains(typeIdHash))
  {
    _handlers[typeIdHash] = [&](const IMessage& message,

                                     const Network::SocketAddress& caller_address)
    {

      if (_handlers.contains(typeIdHash))
      {

        _handlers[typeIdHash](static_cast<const MsgType&>(message),
                              caller_address);
      }
      else
      {
      logger->error("No handler registered for message type with hash {} on "
                    "listen socket port {}", typeIdHash, port);

      }
    };
  };
}
 */
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSystem& MessageSystem::On(
    std::function<void(const MessageType&, const Network::SocketAddress&)> handler)
{
  _ensure_message_dispatcher<MessageType>();
  logger->info("Registered handler for message type with hash {} on global "
               "message system",
               MessageType::TypeIdHash);
  std::unique_lock lock(_mutex);
  MessageID typeIdHash = MessageType::TypeIdHash;
  AN_ASSERT(
      !_handlers.contains(typeIdHash),
      std::format("Handler already registered for message type with hash {}",
                  typeIdHash));
  _handlers[typeIdHash] = [h = std::move(handler)](const IMessage& msg,
                                                   const Network::SocketAddress& address)
  { h(static_cast<const MessageType&>(msg), address); };

  return *this;
}

template <typename MsgType>
  requires std::is_base_of_v<IMessage, MsgType>
inline void MessageSystem::_ensure_message_dispatcher()
{
  MessageID typeIdHash = MsgType::TypeIdHash;
  {
    std::shared_lock lock(_mutex);
    if (_dispatchTable.contains(typeIdHash))
      return;
  }

  std::unique_lock lock(_mutex);

  if (!_dispatchTable.contains(typeIdHash))
  {
    _dispatchTable[typeIdHash] =
        [this](ByteReader& reader, const Network::SocketAddress& address,
               std::optional<Network::PortType> port_received_on)
    {
      MessageID typeIdHash = MsgType::TypeIdHash;
      MsgType msg;
      msg.Deserialize(reader);

      HandlerFunc handler;
      ListenSocketHandle* listenSocket = nullptr;
      {
        std::shared_lock lock(_mutex);
        if (_handlers.contains(typeIdHash))
        {

          handler = _handlers[typeIdHash]; // Placeholder for actual address
        }
        else
        {
          for (const auto& handler : _handlers)
          {
            logger->error("Registered handler for message type with hash {} "
                          "does not match incoming message of type hash {}",
                          handler.first, typeIdHash);
          }
        }
        if (port_received_on.has_value())
        {
          if (_listenSockets.contains(*port_received_on))
          {
            logger->info("Dispatching message of type hash {} received on "
                         "listen socket port {} to listen socket dispatcher",
                         typeIdHash, *port_received_on);
            listenSocket = _listenSockets.at(*port_received_on).get();
          }
          else
          {
            logger->error("No listen socket found for incoming message of type "
                          "hash {} received on listen socket port {}",
                          typeIdHash, *port_received_on);
          }
        }
      }
      if (handler)
      {
        handler(msg, address);
      }
      if (listenSocket)
      {
        listenSocket->DispatchCallbacks(msg, typeIdHash, address);
      }
    };
  };
}

template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSendResult
MessageSystem::TrySendMessage(const MessageType& message,
                              const Network::SocketAddress& address,
                              MessageSendMode mode)
{
  assert(address.IsValid() && "Invalid address provided to TrySendMessage()");
  const ConnectionState state = GetConnectionState(address);
  if (state != ConnectionState::eConnected)
  {
    logger->warn("Attempted to send message of type hash {} to {} while not "
                 "connected (state: {})",
                 MessageType::TypeIdHash, address.to_string(),
                 boost::describe::enum_to_string(state, "<INVALID>"));
    return MessageSendResult{.code = MessageSendResultCode::eDropped};
  }

  auto connHandle =
      GetConnectionHandle(address); // Ensure connection handle is valid
  if (connHandle == k_HSteamNetConnection_Invalid)
  {
    logger->error("Invalid connection handle for address {}",
                  address.to_string());
    return MessageSendResult{.code = MessageSendResultCode::eDropped};
  }

  ByteWriter writer;
  message.Serialize(writer);
  const auto data = writer.bytes();
  const EResult sendResult = GNS().SendMessageToConnection(
      connHandle, data.data(), static_cast<uint32_t>(data.size()),
      static_cast<int>(mode), nullptr);

  if (sendResult != k_EResultOK)
  {
    return MessageSendResult{.code = MessageSendResultCode::eFailedToSend};
  }
  return MessageSendResult{.code = MessageSendResultCode::eSuccess};
}
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline TaskHandle<MessageSendResult>
MessageSystem::QueueMessage(const MessageType& message,
                            const Network::SocketAddress& address, MessageSendMode mode)
{
  assert(address.IsValid() && "Invalid address provided to QueueMessage()");
  TaskHandle<MessageConnectionResult> ConnectTask = Connect(address);
  TaskHandle<MessageSendResult> SendJob =
      config_.taskSystem->MediumPriority().dependent_async(
          [this, message, address, mode,
           ConnectTask = ConnectTask]() -> MessageSendResult
          {
            logger->info("Connected to {}, sending message of type hash {}",
                         address.to_string(), MessageType::TypeIdHash);
            logger->info("Connect task finished with result code: {}",
                         static_cast<int>(ConnectTask->get().code));
            return TrySendMessage(message, address, mode);
          },
          ConnectTask.GetTask());

  _sendJobs.push_back(SendJob);
  return SendJob;
}

static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info)
{
  int64_t ptr_value = (info->m_info.m_nUserData);
  AN_ASSERT(
      ptr_value != 0 && ptr_value != ~0ll,
      std::format(
          "Invalid pointer value in OnSteamNetConnectionStatusChanged: {}",
          ptr_value));
  MessageSystem* system =
      reinterpret_cast<MessageSystem*>(info->m_info.m_nUserData);

  system->SteamNetConnectionStatusChanged(info);
}
}; // namespace AtlasNet
