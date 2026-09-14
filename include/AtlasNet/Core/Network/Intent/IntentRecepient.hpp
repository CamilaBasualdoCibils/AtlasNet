#pragma once

#include "AtlasNet/Core/Core.hpp"
#include <boost/describe/enum.hpp>
namespace AtlasNet::Network::Intent
{
struct IRecepient
{
};
enum class IntentMethod
{
  INVALID = 0,
  NodeRecepient = 1,

  ShardOfEntity = 2,
  ShardOfClient = 3,

  IngressOfClient = 4,
  Database = 5,
};
BOOST_DESCRIBE_ENUM(IntentMethod, INVALID, NodeRecepient, ShardOfEntity,
                    ShardOfClient, IngressOfClient, Database)
namespace Recepient
{
struct InvalidRecepient : public IRecepient
{
  static constexpr IntentMethod Method = IntentMethod::INVALID;
  template <typename Archive> void serialize(Archive& archive)
  {
    // No fields to serialize
  }
  std::string ToString() const
  {
    return "InvalidRecepient";
  }
};
struct NodeRecepient : public IRecepient
{
  static constexpr IntentMethod Method = IntentMethod::NodeRecepient;

  AtlasNetNodeID nodeID;
  template <typename Archive> void serialize(Archive& archive)
  {
    archive(nodeID);
  }
  std::string ToString() const
  {
    return std::format("NodeRecepient(nodeID={})", nodeID.to_string());
  }
};
struct ShardOfEntityRecepient : public IRecepient
{
  static constexpr IntentMethod Method = IntentMethod::ShardOfEntity;
  AtlasNetEntityID entityID;

  template <typename Archive> void serialize(Archive& archive)
  {
    archive(entityID);
  }
  std::string ToString() const
  {
    return std::format("ShardOfEntityRecepient(entityID={})",
                       entityID.to_string());
  }
};
struct ShardOfClientRecepient : public IRecepient
{
  static constexpr IntentMethod Method = IntentMethod::ShardOfClient;
  AtlasNetClientID clientID;

  template <typename Archive> void serialize(Archive& archive)
  {
    archive(clientID);
  }
  std::string ToString() const
  {
    return std::format("ShardOfClientRecepient(clientID={})",
                       clientID.to_string());
  }
};
struct IngressOfClientRecepient : public IRecepient
{
  static constexpr IntentMethod Method = IntentMethod::IngressOfClient;
  AtlasNetClientID clientID;

  template <typename Archive> void serialize(Archive& archive)
  {
    archive(clientID);
  }
  std::string ToString() const
  {
    return std::format("IngressOfClientRecepient(clientID={})",
                       clientID.to_string());
  }
};
struct DatabaseRecepient : public IRecepient
{
  static constexpr IntentMethod Method = IntentMethod::Database;
  // Database specific fields can be added here

  template <typename Archive> void serialize(Archive& archive)
  {
    // No fields to serialize for now
  }
  std::string ToString() const
  {
    return "DatabaseRecepient";
  }
};
} // namespace Recepient

using VIntent = std::variant<
    Recepient::InvalidRecepient, Recepient::NodeRecepient,
    Recepient::ShardOfEntityRecepient, Recepient::ShardOfClientRecepient,
    Recepient::IngressOfClientRecepient, Recepient::DatabaseRecepient>;
constexpr inline std::string IntentToString(const VIntent& intent)
{
  return std::visit([](const auto& recepient) -> std::string
                    { return recepient.ToString(); }, intent);
}
} // namespace AtlasNet::Network::Intent