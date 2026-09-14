#pragma once
#include "bitsery/deserializer.h"
#include "bitsery/serializer.h"
#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/brief_syntax.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <span>/* 
namespace AtlasNet
{
namespace Serialization
{
enum class SerializeDirection
{
  Read,
  Write
};
template <SerializeDirection Dir> struct NetBinary
{

  struct Writer
  {
  };
  struct Reader
  {

    using Data = const uint8_t*;
    using InputAdapter = bitsery::InputBufferAdapter<Data>;
    using Deserializer = bitsery::Deserializer<InputAdapter>;

    Reader(Data data, size_t size) : deserializer(InputAdapter{data, size}) {}
    Reader(std::span<const uint8_t> bytes)
        : deserializer(InputAdapter{bytes.data(), bytes.size()})
    {
    }
    template <typename... Args> void Deserialize(Args&&... args)
    {
      (DeserializeOne(std::forward<Args>(args)), ...);
    }
    template <typename... Args> Reader& operator()(Args&&... args)
    {
      Deserialize(std::forward<Args>(args)...);
      return *this;
    }
    Deserializer* operator->()
    {
      return &deserializer;
    }

  private:
    template <typename T> void DeserializeOne(T&& value)
    {
      deserializer(value);
    }
    Deserializer deserializer;
  };
  using Runtime =
      std::conditional_t<Dir == SerializeDirection::Write, Writer, Reader>;

      template <typename D = Dir>
      requires D == SerializeDirection::Write
      NetBinary() = default;

      template <typename D = Dir>
      requires D == SerializeDirection::Read
      NetBinary(std::span<const uint8_t> bytes) : runtime(bytes) {}
  Runtime runtime;
};
} // namespace Serialization

template <typename Adapter, bool SafeMode = true> class AtlasSerializer
{
public:
  AtlasSerializer& operator()(auto&&... args)
  {
    return *this;
  }

private:
Adapter adapter;
};

}; // namespace AtlasNet
 */