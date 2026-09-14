#pragma once

#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "boost/static_string/static_string.hpp"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <bitset>
#include <chrono>
#include <cstdint>
#include <mutex>
namespace AtlasNet
{
template <uint64_t SequenceBits, uint64_t WorkerBits, uint64_t TimestampBits>
  requires(SequenceBits + WorkerBits + TimestampBits == 64)
class TSnowflake
{

  static inline std::shared_ptr<spdlog::logger> logger =
      spdlog::stdout_color_mt("Snowflake");
  uint64_t _value;

  constexpr static uint64_t SequenceMask = (1ULL << SequenceBits) - 1;
  constexpr static uint64_t WorkerMask = (1ULL << WorkerBits) - 1;
  constexpr static uint64_t TimestampMask = (1ULL << TimestampBits) - 1;

  constexpr static uint64_t SequenceShift = 0;
  constexpr static uint64_t WorkerShift = SequenceBits;
  constexpr static uint64_t TimestampShift = SequenceBits + WorkerBits;
  constexpr static uint64_t TimestampHexDigits = (TimestampBits + 3) / 4;
  constexpr static uint64_t WorkerHexDigits = (WorkerBits + 3) / 4;
  constexpr static uint64_t SequenceHexDigits = (SequenceBits + 3) / 4;

  constexpr static uint64_t StringLength =
      TimestampHexDigits + WorkerHexDigits + SequenceHexDigits + 2; // hyphens

public:
  constexpr static uint64_t MaxWorkers = WorkerMask;
  using Str = boost::static_string<StringLength>;
  class Generator
  {
  public:
    explicit Generator(uint64_t workerId) : m_worker(workerId)
    {
      assert(workerId < MaxWorkers &&
             "Worker ID exceeds the maximum allowed value");
      if (workerId >= MaxWorkers)
      {
        logger->error(
            "Worker ID {} exceeds the maximum allowed value {} from {} "
            "worker bits",
            workerId, MaxWorkers, WorkerBits);
      }
    }
    TSnowflake Next()
    {
      std::unique_lock lock(mutex);
      auto now = CurrentMilliseconds();

      if (now == m_lastTimestamp)
      {
        ++m_sequence;

        if (m_sequence > SequenceMask)
        {
          do
          {
            now = CurrentMilliseconds();
          } while (now == m_lastTimestamp);

          m_lastTimestamp = now;
          m_sequence = 0;
        }
      }
      else
      {
        m_lastTimestamp = now;
        m_sequence = 0;
      }

      return TSnowflake::FromParts(now, m_worker, m_sequence);
    }

  private:
    static uint64_t CurrentMilliseconds()
    {
      using namespace std::chrono;

      return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
    }
    std::mutex mutex;
    uint64_t m_worker;
    uint64_t m_sequence = 0;
    uint64_t m_lastTimestamp = 0;
  };
  TSnowflake() : _value(0) {}
  ~TSnowflake() = default;
  explicit TSnowflake(uint64_t value) : _value(value) {}

protected:
public:
  [[nodiscard]] constexpr uint64_t value() const
  {
    return _value;
  }
  [[nodiscard]] constexpr uint64_t getSequence() const
  {
    return (_value >> SequenceShift) & SequenceMask;
  }
  [[nodiscard]] constexpr uint64_t getWorker() const
  {
    return (_value >> WorkerShift) & WorkerMask;
  }
  [[nodiscard]] constexpr uint64_t getTimestamp() const
  {
    return (_value >> TimestampShift) & TimestampMask;
  }
  [[nodiscard]]
  static constexpr TSnowflake FromParts(uint64_t timestamp, uint64_t worker,
                                        uint64_t sequence)
  {
    return TSnowflake(((timestamp & TimestampMask) << TimestampShift) |
                      ((worker & WorkerMask) << WorkerShift) |
                      ((sequence & SequenceMask) << SequenceShift));
  }

  constexpr auto operator<=>(const TSnowflake&) const = default;

  /**
   * @brief String representation of the Snowflake ID.
   *
   * @return Str
   */
  Str to_string() const
  {
    Str str;
    str.resize(StringLength);

    auto writeField = [&](uint64_t value, uint64_t digits, std::size_t& pos)
    {
      for (int i = static_cast<int>(digits) - 1; i >= 0; --i)
      {
        str[pos + i] = Hex[value & 0xF];
        value >>= 4;
      }

      pos += digits;
    };

    std::size_t pos = 0;

    writeField(getTimestamp(), TimestampHexDigits, pos);

    str[pos++] = '-';

    writeField(getWorker(), WorkerHexDigits, pos);

    str[pos++] = '-';

    writeField(getSequence(), SequenceHexDigits, pos);

    return str;
  }

  static std::optional<TSnowflake> from_string(std::string_view str)
  {
    if (str.size() != StringLength)
      return std::nullopt;

    auto parseField = [&](std::size_t& pos, uint64_t digits,
                          uint64_t& out) -> bool
    {
      out = 0;

      for (uint64_t i = 0; i < digits; ++i)
      {
        uint64_t nibble;

        if (!HexToNibble(str[pos++], nibble))
          return false;

        out = (out << 4) | nibble;
      }

      return true;
    };
    uint64_t timestamp;
    uint64_t worker;
    uint64_t sequence;

    std::size_t pos = 0;

    if (!parseField(pos, TimestampHexDigits, timestamp))
      return std::nullopt;

    if (str[pos++] != '-')
      return std::nullopt;

    if (!parseField(pos, WorkerHexDigits, worker))
      return std::nullopt;

    if (str[pos++] != '-')
      return std::nullopt;

    if (!parseField(pos, SequenceHexDigits, sequence))
      return std::nullopt;

    return TSnowflake::FromParts(timestamp, worker, sequence);
  }

  void Serialize(ByteWriter& bw) const
  {
    bw.u64(value());
  }
  void Deserialize(ByteReader& br)
  {
    br.u64(_value);
  }
  template <typename Archive>
  void serialize(Archive& ar)
  {
    ar(_value);
  }
private:
  static constexpr char Hex[] = "0123456789ABCDEF";

  static constexpr bool HexToNibble(char c, uint64_t& out)
  {
    if (c >= '0' && c <= '9')
    {
      out = c - '0';
      return true;
    }

    if (c >= 'A' && c <= 'F')
    {
      out = c - 'A' + 10;
      return true;
    }

    if (c >= 'a' && c <= 'f')
    {
      out = c - 'a' + 10;
      return true;
    }

    return false;
  }
};

/**
 * 5-bit sequence: allows for 32 unique IDs per worker per timestamp.
 * 8-bit worker: allows for 256 unique workers
 * 51-bit timestamp: allows for a large range of timestamps, providing
 * uniqueness over time.
 */
using Snowflake = TSnowflake<5, 8, 51>;
}; // namespace AtlasNet
namespace std
{
template <uint64_t SequenceBits, uint64_t WorkerBits, uint64_t TimestampBits>
struct hash<AtlasNet::TSnowflake<SequenceBits, WorkerBits, TimestampBits>>
{
  constexpr std::size_t operator()(
      const AtlasNet::TSnowflake<SequenceBits, WorkerBits, TimestampBits>& id)
      const noexcept
  {
    return std::hash<uint64_t>{}(id.value());
  }
};
} // namespace std
