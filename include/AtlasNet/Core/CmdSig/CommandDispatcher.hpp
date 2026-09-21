#pragma once

#include "AtlasNet/Core/CmdSig/Command.hpp"
#include "AtlasNet/Core/CmdSig/ICommandTransport.hpp"
#include "AtlasNet/Core/CmdSig/ISignalEgress.hpp"
#include "AtlasNet/Core/CmdSig/Signal.hpp"
#include "AtlasNet/Core/SpatialObject/IEntityOwnershipSource.hpp"
#include "AtlasNet/Core/SpatialObject/ISpatialObjectStore.hpp"
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace AtlasNet::CmdSig
{

/**
 * AN-A-1 Entity Handle role: resolve ownership, require actor target,
 * apply atomically with ordering, emit client signals.
 */
class CommandDispatcher
{
public:
  using CommandHandler = std::function<bool(
      SpatialObject::ISpatialObjectStore& store,
      const TransitCommandEnvelope& envelope, TransitSignalEnvelope* outSignal)>;

  struct Config
  {
    SpatialObject::ISpatialObjectStore* store = nullptr;
    SpatialObject::IEntityOwnershipSource* ownership = nullptr;
    ICommandTransport* transport = nullptr;
    ISignalEgress* signalEgress = nullptr;
    AtlasNetNodeID localNodeId{};
  };

  explicit CommandDispatcher(const Config& config) : config_(config) {}

  void Bind(std::string commandName, CommandHandler handler)
  {
    std::lock_guard lock(mutex_);
    handlers_[std::move(commandName)] = std::move(handler);
  }

  CommandAck Dispatch(TransitCommandEnvelope envelope)
  {
    if (!config_.store)
    {
      return CommandAck{CommandAckStatus::Rejected, envelope.ordering};
    }

    // Same-shard vs remote (AN-A-1)
    if (config_.ownership)
    {
      auto owner = config_.ownership->OwnerOfEntity(envelope.targetActor);
      if (!owner)
      {
        // Not local and unknown — try forward if transport present
        if (config_.transport)
        {
          return config_.transport->Forward(envelope);
        }
        return CommandAck{CommandAckStatus::NotFound, envelope.ordering};
      }
      if (*owner != config_.localNodeId)
      {
        if (config_.transport)
        {
          auto ack = config_.transport->Forward(envelope);
          ack.status = CommandAckStatus::Forwarded;
          return ack;
        }
        return CommandAck{CommandAckStatus::Forwarded, envelope.ordering};
      }
    }

    if (!config_.store->Exists(envelope.targetActor))
    {
      return CommandAck{CommandAckStatus::NotFound, envelope.ordering};
    }
    if (!config_.store->IsActor(envelope.targetActor))
    {
      return CommandAck{CommandAckStatus::NotActor, envelope.ordering};
    }

    // Atomic apply: assign sequence, run handler, publish, then emit signal
    auto seq = config_.store->NextCommandSequence(envelope.targetActor);
    if (!seq)
    {
      return CommandAck{CommandAckStatus::NotFound, envelope.ordering};
    }
    envelope.ordering.target = envelope.targetActor;
    envelope.ordering.sequence = *seq;

    CommandHandler handler;
    {
      std::lock_guard lock(mutex_);
      const std::string name(envelope.package.commandPayload.commandName.c_str());
      auto it = handlers_.find(name);
      if (it == handlers_.end())
      {
        return CommandAck{CommandAckStatus::Invalid, envelope.ordering};
      }
      handler = it->second;
    }

    TransitSignalEnvelope signal;
    signal.sourceActor = envelope.targetActor;
    signal.ordering = envelope.ordering;

    const bool ok =
        handler(*config_.store, envelope, &signal);
    if (!ok)
    {
      return CommandAck{CommandAckStatus::Rejected, envelope.ordering};
    }

    if (config_.signalEgress &&
        !signal.payload.signalName.empty())
    {
      config_.signalEgress->Emit(signal);
    }

    return CommandAck{CommandAckStatus::Ok, envelope.ordering};
  }

private:
  Config config_;
  std::mutex mutex_;
  std::unordered_map<std::string, CommandHandler> handlers_;
};

/** Test/local transport that records forwards. */
class CapturingCommandTransport final : public ICommandTransport
{
public:
  CommandAck Forward(const TransitCommandEnvelope& envelope) override
  {
    std::lock_guard lock(mutex_);
    forwarded_.push_back(envelope);
    return CommandAck{CommandAckStatus::Forwarded, envelope.ordering};
  }

  [[nodiscard]] std::vector<TransitCommandEnvelope> TakeForwarded()
  {
    std::lock_guard lock(mutex_);
    auto out = std::move(forwarded_);
    forwarded_.clear();
    return out;
  }

private:
  std::mutex mutex_;
  std::vector<TransitCommandEnvelope> forwarded_;
};

class CapturingSignalEgress final : public ISignalEgress
{
public:
  void Emit(const TransitSignalEnvelope& signal) override
  {
    std::lock_guard lock(mutex_);
    emitted_.push_back(signal);
  }

  [[nodiscard]] std::vector<TransitSignalEnvelope> TakeEmitted()
  {
    std::lock_guard lock(mutex_);
    auto out = std::move(emitted_);
    emitted_.clear();
    return out;
  }

private:
  std::mutex mutex_;
  std::vector<TransitSignalEnvelope> emitted_;
};

} // namespace AtlasNet::CmdSig
