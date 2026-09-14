#pragma once

#include <sstream>
#include <string>
namespace AtlasNet::Network
{
    #pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

class MACAddress
{
public:
    static constexpr size_t Size = 6;

    MACAddress() = default;

    explicit MACAddress(std::array<uint8_t, Size> bytes)
        : m_bytes(bytes)
    {
    }

    static MACAddress FromString(std::string_view str)
    {
        if (str.size() != 17)
            throw std::invalid_argument("Invalid MAC address");

        MACAddress mac;

        for (size_t i = 0; i < Size; ++i)
        {
            unsigned int value = 0;

            auto result = std::from_chars(
                str.data() + i * 3,
                str.data() + i * 3 + 2,
                value,
                16);

            if (result.ec != std::errc{} || value > 0xFF)
                throw std::invalid_argument("Invalid MAC address");

            mac.m_bytes[i] = static_cast<uint8_t>(value);

            if (i != Size - 1 && str[i * 3 + 2] != ':')
                throw std::invalid_argument("Invalid MAC address");
        }

        return mac;
    }

    std::string ToString() const
    {
        char buffer[17];

        for (size_t i = 0; i < Size; ++i)
        {
            auto result = std::to_chars(
                buffer + i * 3,
                buffer + i * 3 + 2,
                static_cast<unsigned>(m_bytes[i]),
                16);

            // Pad single-digit values with a leading zero.
            if (result.ptr == buffer + i * 3 + 1)
            {
                buffer[i * 3 + 1] = buffer[i * 3];
                buffer[i * 3] = '0';
            }

            if (i != Size - 1)
                buffer[i * 3 + 2] = ':';
        }

        return std::string(buffer, sizeof(buffer));
    }

    const std::array<uint8_t, Size>& Bytes() const
    {
        return m_bytes;
    }

    bool operator==(const MACAddress&) const = default;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar(m_bytes);
    }

private:
    std::array<uint8_t, Size> m_bytes{};
};
};