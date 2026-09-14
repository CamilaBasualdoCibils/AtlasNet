#pragma once

#include "SerializationConcepts.hpp"
#include "bitsery/deserializer.h"
#include "bitsery/details/brief_syntax_common.h"
#include "bitsery/serializer.h"
#include "boost/static_string/static_string.hpp"
#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/traits/string.h>

#include <bitsery/brief_syntax.h>
#include <bitsery/brief_syntax/array.h>
#include <bitsery/brief_syntax/atomic.h>
#include <bitsery/brief_syntax/bitset.h>
#include <bitsery/brief_syntax/chrono.h>
#include <bitsery/brief_syntax/deque.h>
#include <bitsery/brief_syntax/forward_list.h>
#include <bitsery/brief_syntax/list.h>
#include <bitsery/brief_syntax/map.h>
#include <bitsery/brief_syntax/memory.h>
#include <bitsery/brief_syntax/optional.h>
#include <bitsery/brief_syntax/queue.h>
#include <bitsery/brief_syntax/set.h>
#include <bitsery/brief_syntax/stack.h>
#include <bitsery/brief_syntax/string.h>
#include <bitsery/brief_syntax/tuple.h>
#include <bitsery/brief_syntax/unordered_map.h>
#include <bitsery/brief_syntax/unordered_set.h>
#include <bitsery/brief_syntax/variant.h>
#include <bitsery/brief_syntax/vector.h>
#include <boost/container/small_vector.hpp>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace AtlasNet
{

class NetBinaryReader
{

public:
  using Data = const uint8_t*;
  using InputAdapter = bitsery::InputBufferAdapter<Data>;
  using Deserializer = bitsery::Deserializer<InputAdapter>;

  NetBinaryReader(Data data, size_t size)
      : deserializer(InputAdapter{data, size})
  {
  }
  NetBinaryReader(std::span<const uint8_t> bytes)
      : deserializer(InputAdapter{bytes.data(), bytes.size()})
  {
  }
  template <typename... Args> void Deserialize(Args&&... args)
  {
    (DeserializeOne(std::forward<Args>(args)), ...);
  }
  template <typename... Args> NetBinaryReader& operator()(Args&&... args)
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
class NetBinaryWriter
{
public:
  using Buffer = std::vector<uint8_t>;
  using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
  using Serializer = bitsery::Serializer<OutputAdapter>;

  NetBinaryWriter() : buffer(), serializer(buffer) {}
  template <typename... Args> void Serialize(Args&&... args)
  {
    (SerializeOne(std::forward<Args>(args)), ...);
    serializer.adapter().flush();
  }
  template <typename... Args> NetBinaryWriter& operator()(Args&&... args)
  {
    Serialize(std::forward<Args>(args)...);
    return *this;
  }

  Serializer* operator->()
  {
    return &serializer;
  }
  void Flush()
  {
    serializer.adapter().flush();
  }
  auto GetBytes()
  {
    Flush();
    return std::span<const uint8_t>(buffer.data(),
                                    serializer.adapter().writtenBytesCount());
  }

private:
  template <typename T> void SerializeOne(T&& value)
  {

    serializer(value);
  }
  Buffer buffer;
  Serializer serializer;
};

struct XMLAdapter
{
};
struct JSONAdapter
{
};

} // namespace AtlasNet
namespace bitsery::traits
{

template <size_t N, typename CHAR_T>
struct TextTraits<boost::static_strings::basic_static_string<N, CHAR_T>>
{
  using TValue = CHAR_T;
  static constexpr bool addNUL = false;
  static constexpr size_t maxSize = N;
  static size_t
  length(const boost::static_strings::basic_static_string<N, CHAR_T>& str)
  {
    return str.size();
  }
};
template <size_t N, typename CHAR_T>
struct ContainerTraits<boost::static_strings::basic_static_string<N, CHAR_T>>
{
  using TValue = CHAR_T;
  static constexpr bool isResizable = true;
  static constexpr bool isContiguous = true;
  static size_t
  size(const boost::static_strings::basic_static_string<N, CHAR_T>& str)
  {
    return str.size();
  }
  static void resize(boost::static_strings::basic_static_string<N, CHAR_T>& str,
                     size_t newSize)
  {
    str.resize(newSize);
  }
};
template <typename T, std::size_t N>
struct ContainerTraits<boost::container::small_vector<T, N>>
{
  using TValue = T;
  static constexpr bool isResizable = true;
  static constexpr bool isContiguous = true;
  static size_t size(const boost::container::small_vector<T, N>& vec)
  {
    return vec.size();
  }
  static void resize(boost::container::small_vector<T, N>& vec, size_t newSize)
  {
    vec.resize(newSize);
  }
};
template <typename T, std::size_t N>
struct ContainerTraits<boost::container::static_vector<T, N>>
{
  using TValue = T;
  static constexpr bool isResizable = true;
  static constexpr bool isContiguous = true;
  static size_t size(const boost::container::static_vector<T, N>& vec)
  {
    return vec.size();
  }
  static void resize(boost::container::static_vector<T, N>& vec, size_t newSize)
  {
    vec.resize(newSize);
  }
};
} // namespace bitsery::traits
namespace bitsery
{

template <typename S, typename CHAR_T, size_t N>
void serialize(S& s,
               boost::static_strings::basic_static_string<N, CHAR_T>& value)
{
  s.template text<sizeof(CHAR_T)>(value, value.max_size());
}
template <typename S, typename T, std::size_t N>
void serialize(S& s, boost::container::small_vector<T, N>& vec)
{
  bitsery::brief_syntax::processContainer(s, vec);
}

template <typename S, typename T, std::size_t N>
void serialize(S& s, boost::container::static_vector<T, N>& vec)
{
  s.container(vec, vec.max_size());
}
} // namespace bitsery