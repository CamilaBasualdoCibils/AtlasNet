#pragma once
#include "Message.hpp"
#include "atlasnet/core/EndPoint.hpp"
#include "atlasnet/core/System.hpp"
#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobOptions.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <atomic>
#include <format>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
namespace AtlasNet
{
static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info);
enum class MessageSendMode
{
  eReliable = k_nSteamNetworkingSend_ReliableNoNagle,
  eReliableBatched = k_nSteamNetworkingSend_Reliable,
  eUnreliable = k_nSteamNetworkingSend_UnreliableNoNagle,
  eUnreliableBatched = k_nSteamNetworkingSend_Unreliable
};
enum class ConnectionState
{
  eNone = k_ESteamNetworkingConnectionState_None,
  eConnecting = k_ESteamNetworkingConnectionState_Connecting,
  eConnected = k_ESteamNetworkingConnectionState_Connected,
  eClosedByPeer = k_ESteamNetworkingConnectionState_ClosedByPeer,
  eProblemDetectedLocally =
      k_ESteamNetworkingConnectionState_ProblemDetectedLocally
};
using MessagePriority = JobPriority;
class MessageSystem : public System<MessageSystem>
{
public:
  class Connection
  {
    MessageSystem& system;
    HSteamNetConnection handle;
    ConnectionState state;
    friend class MessageSystem;

  protected:
    Connection(MessageSystem& system, HSteamNetConnection handle)
        : system(system), handle(handle), state(ConnectionState::eNone)
    {
    }

  public:
    HSteamNetConnection GetHandle() const
    {
      return handle;
    }
    ConnectionState GetState() const
    {
      return state;
    }
    void SendMessage(const void* data, uint32_t size,
                     MessageSendMode mode) const;
  };
  class ListenSocketHandle
  {
    MessageSystem& system;
    HSteamListenSocket handle;
    std::shared_mutex socket_mutex;
    using HandlerFunc =
        std::function<void(const IMessage&, const EndPointAddress&)>;
    std::unordered_map<MessageIDHash, HandlerFunc> _handlers;
    using DispatchFunc = std::function<void(const IMessage&, MessageIDHash,
                                            const EndPointAddress&)>;
    std::unordered_map<MessageIDHash, DispatchFunc> _dispatchTable;
    friend class MessageSystem;

  protected:
    void DispatchCallbacks(const IMessage& message, MessageIDHash typeIdHash,
                           const EndPointAddress& caller_address);

  public:
    ListenSocketHandle(MessageSystem& system, HSteamListenSocket handle);
    template <typename MessageType>
      requires std::is_base_of_v<IMessage, MessageType>
    ListenSocketHandle&
    On(std::function<void(const MessageType&, const EndPointAddress&)> func);

  private:
    template <typename MsgType>
      requires std::is_base_of_v<IMessage, MsgType>
    void _ensure_socket_message_dispatcher();
  };

  MessageSystem();

  void SetIdentity(const SteamNetworkingIdentity& identity);

  ~MessageSystem();
  void Shutdown() override;

  JobHandle Connect(const EndPointAddress& address);
  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  void SendMessage(const MessageType& message, const EndPointAddress& address,
                   MessageSendMode mode,
                   MessagePriority priority = MessagePriority::eMedium);

  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  MessageSystem&
  On(std::function<void(const MessageType&, const EndPointAddress&)> handler);

  ListenSocketHandle& OpenListenSocket(PortType port);
  ListenSocketHandle& GetListenSocket(PortType port);

  void SteamNetConnectionStatusChanged(
      SteamNetConnectionStatusChangedCallback_t* pInfo);

  ConnectionState GetConnectionState(const EndPointAddress& address) const;

  bool IsConnectingTo(const EndPointAddress& address) const;
  bool IsConnectedTo(const EndPointAddress& address) const;

  size_t GetNumConnections() const;
  void GetConnections(
      std::unordered_map<EndPointAddress, Connection>& connections) const;
  std::optional<Connection> GetConnection(const EndPointAddress& address) const;

private:
  void _Connect_to_job(JobContext& handle, const EndPointAddress& address);
  void _Parse_Incoming_Messages();
  ISteamNetworkingSockets& GNS() const
  {
    AN_ASSERT(_GNS, "GNS is null");
    return *_GNS;
  }
  HSteamNetConnection GetConnectionHandle(const EndPointAddress& address) const;
  template <typename MsgType>
    requires std::is_base_of_v<IMessage, MsgType>
  void _ensure_message_dispatcher();

  ISteamNetworkingSockets* _GNS;
  HSteamNetPollGroup _pollGroup;
  mutable std::shared_mutex _mutex;
  std::unordered_map<PortType, std::unique_ptr<ListenSocketHandle>>
      _listenSockets;
  std::unordered_map<EndPointAddress, Connection> _connections;
  std::unordered_map<EndPointAddress, JobHandle> _connectJobs;
  using HandlerFunc =
      std::function<void(const IMessage&, const EndPointAddress&)>;
  std::unordered_map<MessageIDHash, HandlerFunc>
      _handlers;
      using DispatchFunc = std::function<void(ByteReader&, const EndPointAddress&, PortType)>;
  std::unordered_map<MessageIDHash, DispatchFunc>
      _dispatchTable;
  std::optional<JobHandle> _pollJobHandle;
  std::atomic_bool shutdown = false;
};
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSystem::ListenSocketHandle& MessageSystem::ListenSocketHandle::On(
    std::function<void(const MessageType&, const EndPointAddress&)> func)
{
  system._ensure_message_dispatcher<MessageType>();
  _ensure_socket_message_dispatcher<MessageType>();
  std::unique_lock lock(socket_mutex);
  MessageIDHash typeIdHash = MessageType::TypeIdHash;
  AN_ASSERT(
      !_handlers.contains(typeIdHash),
      std::format("Handler already registered for message type with hash {}",
                  typeIdHash));
  _handlers[typeIdHash] =
      [h = std::move(func)](const IMessage& msg, const EndPointAddress& address)
  { h(static_cast<const MessageType&>(msg), address); };

  return *this;
}

template <typename MsgType>
  requires std::is_base_of_v<IMessage, MsgType>
inline void
MessageSystem::ListenSocketHandle::_ensure_socket_message_dispatcher()
{
  MessageIDHash typeIdHash = MsgType::TypeIdHash;
  {
    std::shared_lock lock(socket_mutex);
    if (_dispatchTable.contains(typeIdHash))
      return;
  }

  std::unique_lock lock(socket_mutex);

  if (!_dispatchTable.contains(typeIdHash))
  {
    _dispatchTable[typeIdHash] = [&](const IMessage& message,
                                     MessageIDHash typeIdHash,
                                     const EndPointAddress& caller_address)
    {
      if (_handlers.contains(typeIdHash))
      {

        _handlers[typeIdHash](static_cast<const MsgType&>(message),
                              caller_address);
      }
      else
      {
        std::cerr << std::format(
            "No handler registered for message type with hash {}", typeIdHash);
      }
    };
  };
}

template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSystem& MessageSystem::On(
    std::function<void(const MessageType&, const EndPointAddress&)> handler)
{
  _ensure_message_dispatcher<MessageType>();
  std::unique_lock lock(_mutex);
  MessageIDHash typeIdHash = MessageType::TypeIdHash;
  AN_ASSERT(
      !_handlers.contains(typeIdHash),
      std::format("Handler already registered for message type with hash {}",
                  typeIdHash));
  _handlers[typeIdHash] =
      [h = std::move(handler)](const IMessage& msg,
                               const EndPointAddress& address)
  { h(static_cast<const MessageType&>(msg), address); };

  return *this;
}

template <typename MsgType>
  requires std::is_base_of_v<IMessage, MsgType>
inline void MessageSystem::_ensure_message_dispatcher()
{
  MessageIDHash typeIdHash = MsgType::TypeIdHash;
  {
    std::shared_lock lock(_mutex);
    if (_dispatchTable.contains(typeIdHash))
      return;
  }

  std::unique_lock lock(_mutex);

  if (!_dispatchTable.contains(typeIdHash))
  {
    _dispatchTable[typeIdHash] =
        [this](ByteReader& reader, const EndPointAddress& address, PortType port_received_on)
    {
      MessageIDHash typeIdHash = MsgType::TypeIdHash;
      MsgType msg;
      msg.Deserialize(reader);
      if (_handlers.contains(typeIdHash))
      {

        _handlers[typeIdHash](msg, address); // Placeholder for actual address
      }
      else
      {
        for (const auto& handler : _handlers)
        {
          std::cerr << std::format(
              "Registered handler for message type with hash {}, but no ",
              handler.first);
        }
      }

      if (_listenSockets.contains(port_received_on))
      {
        _listenSockets.at(port_received_on)
            ->DispatchCallbacks(msg, typeIdHash, address);
      }
    };
  };
}

template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline void MessageSystem::SendMessage(const MessageType& message,
                                       const EndPointAddress& address,
                                       MessageSendMode mode,
                                       MessagePriority priority)

{
  auto SendMessageFunc =
      [this, message = message, address, mode](JobContext& handle)
  {
    HSteamNetConnection con;
    {
      std::shared_lock lock(_mutex);
      con = GetConnectionHandle(address);
    }
    ByteWriter bw;
    message.Serialize(bw);
    const auto data_span = bw.bytes();

    GNS().SendMessageToConnection(con, data_span.data(),
                                  static_cast<uint32_t>(data_span.size()),
                                  static_cast<int>(mode), nullptr);
  };
  JobSystem::Get().Submit(
      [this, message = message, address, mode](JobContext& handle)
      {
        HSteamNetConnection con;
        {
          std::shared_lock lock(_mutex);
          con = GetConnectionHandle(address);
        }
        ByteWriter bw;
        message.Serialize(bw);
        const auto data_span = bw.bytes();

        GNS().SendMessageToConnection(con, data_span.data(),
                                      static_cast<uint32_t>(data_span.size()),
                                      static_cast<int>(mode), nullptr);
      },
      JobOpts::Notify<JobNotifyLevel::eOnStart>(),
      JobOpts::Name(std::format("MessageSystem::SendMessage to {}",
                                address.to_string())));

  if (_connections.contains(address))
  {
    std::cerr << "Connection exists, Pushing Send Job\n";
    {

      std::unique_lock lock(_mutex);
      if (_connectJobs.contains(address))
      {
        std::cerr << "Connect job exists, chaining Send Job on completion\n";
        _connectJobs.at(address).on_complete(
            SendMessageFunc,
            AtlasNet::JobOpts::Name(
                std::format("MessageSystem::SendMessage to {} after connect",
                            address.to_string())));
        return;
      }
    }
    JobSystem::Get().Submit(
        SendMessageFunc,
        AtlasNet::JobOpts::Name(std::format("MessageSystem::SendMessage to {}",
                                            address.to_string())),
        AtlasNet::JobOpts::Priority<MessagePriority::eMedium>{},
        AtlasNet::JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{});
  }
  else
  {
    std::cerr << "Connection does not exist, Pushing Connect Job\n";
    JobHandle connectHandle = Connect(address);
    JobHandle SendMessageHandle = connectHandle.on_complete(
        SendMessageFunc,
        AtlasNet::JobOpts::Name(std::format("MessageSystem::SendMessage to {}",
                                            address.to_string())),
        AtlasNet::JobOpts::Priority<MessagePriority::eMedium>{},
        AtlasNet::JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{});
  }
}

static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info)
{
  AtlasNet::MessageSystem::Get().SteamNetConnectionStatusChanged(info);
}
}; // namespace AtlasNet
