#pragma once

#include <cstdint>
namespace AtlasNet::Network::Cluster
{
    using ChannelID = uint16_t;
    enum class DeliveryMode : uint8_t
{
    Unreliable,
    Reliable,
};

enum class OrderingMode : uint8_t
{
    Unordered,

    // Messages are delivered in sequence order.
    // Usually requires reliable delivery.
    Ordered,

    // Unreliable, but stale/out-of-order messages are discarded.
    // Useful for movement snapshots.
    Sequenced,
};

enum class BatchMode : uint8_t
{
    Immediate,

    // Queue until flush, size threshold, or scheduler tick.
    Automatic,

    // Queue until Flush() is explicitly called.
    Manual,
};


} // namespace AtlasNet::Network::Cluster