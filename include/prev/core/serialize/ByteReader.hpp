#pragma once
#include <bit>
#include <boost/beast/core/detail/base64.hpp>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

#include "ByteStream.hpp"
#include "atlasnet/core/utils/UUID.hpp"
namespace AtlasNet
{

// #include "Global/Misc/String_utils.hpp"
// #include "Global/Misc/UUID.hpp"
// #include "Global/pch.hpp"
class ByteReader
{
private:
public:
  ByteReader(std::span<const uint8_t> data, bool IsEncodedBase64 = false)
      : p(data.data()), n(data.size())
  {
    if (IsEncodedBase64)
      DecodeBase64();
  }
  explicit ByteReader(const std::string& s, bool IsEncodedBase64 = false)
      : p(reinterpret_cast<const uint8_t*>(s.data())), n(s.size())
  {
    if (IsEncodedBase64)
      DecodeBase64();
  }
  explicit ByteReader(const std::string_view& s, bool IsEncodedBase64 = false)
      : p(reinterpret_cast<const uint8_t*>(s.data())), n(s.size())
  {
    if (IsEncodedBase64)
      DecodeBase64();
  }
  void DecodeBase64()
  {
    size_t real_len = std::strlen(reinterpret_cast<const char*>(p));
    std::size_t decoded_size =
        boost::beast::detail::base64::decoded_size(real_len);
    DecodedBase64Data.resize(decoded_size);

    const auto result = boost::beast::detail::base64::decode(
        DecodedBase64Data.data(), reinterpret_cast<const char*>(p), real_len);
    DecodedBase64Data.resize(result.first);
    p = DecodedBase64Data.data();
    n = DecodedBase64Data.size();
  }

  size_t remaining() const
  {
    return n - i;
  }
  size_t position() const
  {
    return i;
  }

  // ---------------- Raw -----------------------------------------

  void read(void* out, size_t bytes)
  {
    if (i + bytes > n)
      throw ByteError("read overflow");
    std::memcpy(out, p + i, bytes);
    i += bytes;
  }

  // ---------------- Integers ------------------------------------

  ByteReader& u8(uint8_t& v)
  {
    v = read_int<uint8_t>();
    return *this;
  }
  ByteReader& u16(uint16_t& v)
  {
    v = read_be<uint16_t>();
    return *this;
  }
  ByteReader& u32(uint32_t& v)
  {
    v = read_be<uint32_t>();
    return *this;
  }
  ByteReader& u64(uint64_t& v)
  {
    v = read_be<uint64_t>();
    return *this;
  }

  ByteReader& i8(int8_t& v)
  {
    v = read_be<int8_t>();
    return *this;
  }
  ByteReader& i16(int16_t& v)
  {
    v = read_be<int16_t>();
    return *this;
  }
  ByteReader& i32(int32_t& v)
  {
    v = read_be<int32_t>();
    return *this;
  }
  ByteReader& i64(int64_t& v)
  {
    v = read_be<int64_t>();
    return *this;
  }

  // ---------------- Floats --------------------------------------

  ByteReader& f32(float& v)
  {
    uint32_t temp;
    u32(temp);
    v = std::bit_cast<float>(temp);
    return *this;
  }
  ByteReader& f64(double& v)
  {
    uint64_t temp;
    u64(temp);
    v = std::bit_cast<double>(temp);
    return *this;
  }
  // ---------------- Generic Scalar -----------------------------
  template <typename T> ByteReader& read_scalar(T& v)
  {
    if constexpr (std::is_enum_v<T>)
    {
      using U = std::underlying_type_t<T>;
      U temp;
      read_scalar(temp);
      v = static_cast<T>(temp);
      return *this;
    }
    else if constexpr (std::is_integral_v<T>)
    {
      if constexpr (sizeof(T) == 1)
        v = read_int<T>();
      else
        v = read_be<T>();
      return *this;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
      if constexpr (sizeof(T) == 4)
        f32(v);
      else if constexpr (sizeof(T) == 8)
        f64(v);
      return *this;
    }
    else
    {
      static_assert(!sizeof(T*), "read_scalar: unsupported type");
    }
  }

  // ---------------- Generic Vector -----------------------------
  template <uint32_t Dim, typename T> ByteReader& read_vector(T& out)
  {
    for (uint32_t d = 0; d < Dim; ++d)
      read_scalar(out[d]);
    return *this;
  }
  // ---------------- glm -----------------------------------------

  // glm::half half() {
  //   return glm::unpackHalf1x16(u16());
  // }

  ByteReader& vec2(glm::vec2& v)
  {
    return read_vector<2>(v);
  }
  ByteReader& vec3(glm::vec3& v)
  {
    return read_vector<3>(v);
  }
  ByteReader& vec4(glm::vec4& v)
  {
    return read_vector<4>(v);
  }

  ByteReader& ivec2(glm::ivec2& v)
  {
    return read_vector<2>(v);
  }
  ByteReader& ivec3(glm::ivec3& v)
  {
    return read_vector<3>(v);
  }
  ByteReader& ivec4(glm::ivec4& v)
  {
    return read_vector<4>(v);
  }

  ByteReader& quat(glm::quat& v)
  {
    auto vec = glm::vec4();
    read_vector<4>(vec);
    v = glm::quat(vec.w, vec.x, vec.y, vec.z);
    return *this;
  }

  ByteReader& mat4(glm::mat4& m)
  {
    float* p = glm::value_ptr(m);
    for (int i = 0; i < 16; ++i)
      f32(p[i]);
    return *this;
  }

  // ---------------- Strings / blobs -----------------------------

  ByteReader& str(std::string& s)
  {
    uint32_t len;
    var_u32(len);
    if (remaining() < len)
      throw ByteError("string overflow");
    s.assign(reinterpret_cast<const char*>(p + i), len);
    i += len;
    return *this;
  }

  ByteReader& str(std::string_view& s)
  {
    uint32_t len;
    var_u32(len);
    if (remaining() < len)
      throw ByteError("string_view overflow");
    s = std::string_view(reinterpret_cast<const char*>(p + i), len);
    i += len;
    return *this;
  }
  ByteReader& uuid(UUID& id)
  {
    if (remaining() < 16)
      throw ByteError("UUID overflow");
    read(id.data(), id.size());
    return *this;
  }
  // template <std::size_t N>
  // static_string<N> str()
  //{
  //	uint32_t len = var_u32();
  //	if (remaining() < len)
  //		throw ByteError("string overflow");
  //	static_string<N> s(reinterpret_cast<const char*>(p + i), len);
  //	i += len;
  //	return s;
  // }
  ByteReader& blob(std::span<const uint8_t>& out)
  {
    uint32_t len;
    var_u32(len);
    if (remaining() < len)
      throw ByteError("blob overflow");
    out = std::span<const uint8_t>(p + i, len);
    i += len;
    return *this;
  }

  // ---------------- Varints -------------------------------------

  ByteReader& var_u32(uint32_t& v)
  {
    v = 0;
    int shift = 0;
    for (;;)
    {
      uint8_t b;
      u8(b);
      v |= uint32_t(b & 0x7F) << shift;
      if (!(b & 0x80))
        break;
      shift += 7;
      if (shift > 35)
        throw ByteError("varint overflow");
    }
    return *this;
  }

  ByteReader& var_i32(int32_t& v)
  {
    uint32_t temp;
    var_u32(temp);
    v = (temp >> 1) ^ -(int32_t(temp & 1));
    return *this;
  }

  // ---------------- Bit unpacking -------------------------------

  ByteReader& bits(uint8_t bitCount, uint32_t& out)
  {
    if (bitCount > 32)
      throw ByteError("bitCount > 32");
    while (bitPos < bitCount)
    {
      uint8_t b;
      u8(b);
      bitBuffer |= uint64_t(b) << bitPos;
      bitPos += 8;
    }
    out = uint32_t(bitBuffer & ((1ull << bitCount) - 1));
    bitBuffer >>= bitCount;
    bitPos -= bitCount;
    return *this;
  }

  void align_bits()
  {
    bitBuffer = 0;
    bitPos = 0;
  }

  // ---------------- TLV ------------------------------------------

  bool next_tlv(uint16_t& tag, std::span<const uint8_t>& value)
  {
    if (remaining() == 0)
      return false;
    u16(tag);
    uint32_t len;
    var_u32(len);
    if (remaining() < len)
      throw ByteError("TLV overflow");
    value = {p + i, len};
    i += len;
    return true;
  }

  std::span<const uint8_t> GetReadSource() const
  {
    return {p, n};
  }

  template <typename T> ByteReader& operator()(T& v)
  {
    read_any(v);
    return *this;
  }

  template <typename... Args> ByteReader& operator()(Args&&... args)
  {
    (read_any(args), ...);
    return *this;
  }
  template <typename T> void read_any(T& v)
  {
    using U = std::remove_cvref_t<T>;
    if constexpr (requires { v.Deserialize(*this); })
    {
      v.Deserialize(*this);
    }

    else if constexpr (is_pair_v<U>)
    {
      read_any(v.first);
      read_any(v.second);
    }
    else if constexpr (std::is_same_v<U, std::string>)
    {
      str(v);
    }
    else if constexpr (std::is_same_v<U, std::string_view>)
    {
      str(v);
    }
    else if constexpr (std::is_same_v<U, UUID>)
    {
      uuid(v);
    }
    else if constexpr (std::is_same_v<U, std::span<const uint8_t>>)
    {
      blob(v);
    }
    else if constexpr (std::is_arithmetic_v<U>)
    {
      read_scalar(v);
    }
    else if constexpr (std::is_enum_v<U>)
    {
      read_scalar(v);
    }
    else if constexpr (std::is_same_v<U, glm::quat>)
    {
      quat(v);
    }
    else if constexpr (std::is_same_v<U, glm::mat4>)
    {
      mat4(v);
    }
    else if constexpr (is_glm_vec<U>::value)
    {
      read_vector<U::length()>(v);
    }
    else if constexpr (resizable_byte_container<T>)
    {

      std::span<const uint8_t> span;
      blob(span);
      v.assign(span.begin(), span.end());
    }
    else if constexpr (fixed_byte_container<T>)
    {
      // uint32_t size{};
      // read_scalar(size);

      // safety check
      // if v is literally a span then just blob(v)
      if constexpr (std::is_same_v<U, std::span<const uint8_t>>)
      {
        blob(v);
        return;
      }
      else if constexpr (std::is_same_v<U, std::span<uint8_t>>)
      {
        static_assert(false, "Cannot read into a non-const span");
      }
      else
      {
        // if v is a fixed size container then just read the blob into it
        std::span<const uint8_t> span;
        blob(span);
        if (span.size() != v.size())
          throw ByteError("Fixed-size container mismatch");
        std::memcpy(v.data(), span.data(), span.size());
      }
    }
    else if constexpr (ResizableIterable<U>)
    {
      uint32_t count{};
      read_scalar(count);

      v.resize(count);
      for (auto& elem : v)
      {
        read_any(elem);
      }
    }
    else if constexpr (is_associative_container_v<U>)
    {
      uint32_t count{};
      read_scalar(count);

      for (uint32_t i = 0; i < count; ++i)
      {
        using value_type = typename U::value_type;
        using raw_key_type = typename value_type::first_type;
        using raw_mapped_type = typename value_type::second_type;

        using key_type = std::remove_cvref_t<raw_key_type>;
        using mapped_type = std::remove_cvref_t<raw_mapped_type>;

        key_type key{};
        mapped_type value{};

        read_any(key);
        read_any(value);

        v.emplace(std::move(key), std::move(value));
      }
    }

    else
    {
      static_assert(!sizeof(U*), "Unsupported type for ByteReader");
    }
  }

private:
  template <typename T> T read_int()
  {
    if (i + sizeof(T) > n)
      throw ByteError("read overflow");
    T v;
    std::memcpy(&v, p + i, sizeof(T));
    i += sizeof(T);
    return v;
  }

  template <typename T> T read_be()
  {
    if (i + sizeof(T) > n)
      throw ByteError("read overflow");
    T v = 0;
    for (size_t j = 0; j < sizeof(T); ++j)
      v = (v << 8) | p[i++];
    return v;
  }

  std::vector<uint8_t> DecodedBase64Data;
  const uint8_t* p;
  size_t n;
  size_t i = 0;

  uint64_t bitBuffer = 0;
  uint8_t bitPos = 0;
};

} // namespace AtlasNet