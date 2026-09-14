#pragma once

#include <cstdint>
#include <string>
namespace AtlasNet
{
    enum class MessageConnectionResultCode : uint8_t
    {
        eSuccess = 0,
        eAlreadyConnected = 1,
        eFailedToConnect = 2,
        eConnecting = 3
    };
    struct MessageConnectionResult
    {
        MessageConnectionResultCode code;
    };
    enum class MessageSendResultCode :uint8_t
    {
        eSuccess = 0,
        eFailedToConnect = 1,
        eFailedToSend = 2,
        eDropped = 3,
        eFailedToResolveAddress = 4
    };
    struct MessageSendResult
    {
        MessageSendResultCode code;
        std::string errorMessage;
    };
};