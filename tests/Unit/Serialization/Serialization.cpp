
#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include <boost/container/static_vector.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
using namespace AtlasNet;
TEST(Serialization, BinarySerializerManual)
{
  {
    NetBinaryWriter serializer;
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    serializer->container1b(data, data.size());

    NetBinaryReader deserializer(serializer.GetBytes());
    std::vector<uint8_t> output(5);
    deserializer->container1b(output, data.size());

    EXPECT_EQ(data, output);
  }
  {
    std::string str = "Hello, AtlasNet!";
    NetBinaryWriter serializer;
    serializer->text1b(str, str.size());

    NetBinaryReader deserializer(serializer.GetBytes());
    std::string output;
    deserializer->text1b(output, str.size());
    EXPECT_EQ(str, output);
  }
}
TEST(Serialization, BinarySerializer)
{
  NetBinaryWriter serializer;
  std::array<uint8_t, 5> data = {1, 2, 3, 4, 5};
  serializer(data[0], data[1], data[2], data[3], data[4]);

  NetBinaryReader deserializer(serializer.GetBytes());
  std::array<uint8_t, 5> output;
  deserializer(output[0], output[1], output[2], output[3], output[4]);
  EXPECT_EQ(data, output);
}/* 
TEST(Serialization, XMLSerializer)
{
  AtlasNet::XMLSerializer serializer;
  serializer("Hello, AtlasNet!", 42, 3.14f);
} */

TEST(Serialization, BinarySerializer_int)
{
  NetBinaryWriter serializer;
  int8_t i8_val = -128;
  int16_t i16_val = -32768;
  int32_t i32_val = -2147483648;
  int64_t i64_val = -9223372036854775807ll - 1;
  uint8_t u8_val = 255;
  uint16_t u16_val = 65535;
  uint32_t u32_val = 4294967295;
  uint64_t u64_val = 18446744073709551615ull;
  serializer(i8_val, i16_val, i32_val, i64_val, u8_val, u16_val, u32_val,
             u64_val);

  NetBinaryReader deserializer(serializer.GetBytes());
  int8_t i8_out;
  int16_t i16_out;
  int32_t i32_out;
  int64_t i64_out;
  uint8_t u8_out;
  uint16_t u16_out;
  uint32_t u32_out;
  uint64_t u64_out;
  deserializer(i8_out, i16_out, i32_out, i64_out, u8_out, u16_out, u32_out,
               u64_out);
  EXPECT_EQ(i8_out, i8_val);
  EXPECT_EQ(i16_out, i16_val);
  EXPECT_EQ(i32_out, i32_val);
  EXPECT_EQ(i64_out, i64_val);
  EXPECT_EQ(u8_out, u8_val);
  EXPECT_EQ(u16_out, u16_val);
  EXPECT_EQ(u32_out, u32_val);
  EXPECT_EQ(u64_out, u64_val);
}
TEST(Serialization, BinarySerializer_float)
{
  NetBinaryWriter serializer;
  float f32_val = 3.14f;
  double f64_val = 3.141592653589793;
  serializer(f32_val, f64_val);

  NetBinaryReader deserializer(serializer.GetBytes());
  float f32_out;
  double f64_out;
  deserializer(f32_out, f64_out);
  EXPECT_FLOAT_EQ(f32_out, f32_val);
  EXPECT_DOUBLE_EQ(f64_out, f64_val);
}
TEST(Serialization, BinarySerializer_string)
{
  NetBinaryWriter serializer;
  std::string str_val = "Hello, AtlasNet!";
  std::u8string u8str_val = u8"Hello, AtlasNet!";
  std::u16string u16str_val = u"Hello, AtlasNet!";
  std::u32string u32str_val = U"Hello, AtlasNet!";
  std::wstring wstr_val = L"Hello, AtlasNet!";
  serializer(str_val, u8str_val, u16str_val, u32str_val, wstr_val);

  NetBinaryReader deserializer(serializer.GetBytes());
  std::string str_out;
  std::u8string u8str_out;
  std::u16string u16str_out;
  std::u32string u32str_out;
  std::wstring wstr_out;
  deserializer(str_out, u8str_out, u16str_out, u32str_out, wstr_out);
  EXPECT_EQ(str_out, str_val);
  EXPECT_TRUE(u8str_out == u8str_val);
  EXPECT_EQ(u16str_out, u16str_val);
  EXPECT_EQ(u32str_out, u32str_val);
  EXPECT_EQ(wstr_out, wstr_val);
}
TEST(Serialization, BinarySerializer_array)
{
  size_t arraySize;
  {
    NetBinaryWriter serializer;
    std::array<uint8_t, 5> array_val = {1, 2, 3, 4, 5};
    serializer(array_val);
    arraySize = serializer.GetBytes().size_bytes();
    NetBinaryReader deserializer(serializer.GetBytes());
    std::array<uint8_t, 5> array_out;
    deserializer(array_out);
    EXPECT_EQ(array_out, array_val);
  }
  size_t vectorSize;
  {
    NetBinaryWriter serializer;
    std::vector<uint8_t> vector_val = {1, 2, 3, 4, 5};
    serializer(vector_val);
    vectorSize = serializer.GetBytes().size_bytes();
    NetBinaryReader deserializer(serializer.GetBytes());
    std::vector<uint8_t> vector_out;
    deserializer(vector_out);
    EXPECT_EQ(vector_out, vector_val);
  }
  EXPECT_LT(arraySize, vectorSize);
}
TEST(Serialization, BinarySerializer_array_vector_string)
{
  using type = std::vector<std::array<std::string, 5>>;
  NetBinaryWriter serializer;
  type value = {{"Hello", "AtlasNet", "Test", "Serialization", "Array"},
                {"Hello", "AtlasNet", "Test", "Serialization", "Vector"},
                {"Hello", "AtlasNet", "Test", "Serialization", "String"}};
  serializer(value);
  NetBinaryReader deserializer(serializer.GetBytes());
  type value_out;
  deserializer(value_out);
  EXPECT_EQ(value_out, value);
}
TEST(Serialization, BinarySerializer_BoostTypes)
{
  boost::static_string<64> static_str_val = "Hello, AtlasNet!";
  boost::container::small_vector<uint8_t, 64> small_vector_val = {1, 2, 3, 4,
                                                                  5};
  boost::container::static_vector<uint8_t, 64> static_vector_val = {1, 2, 3, 4,
                                                                    5};
  NetBinaryWriter serializer;
  serializer(static_str_val, small_vector_val, static_vector_val);
  NetBinaryReader deserializer(serializer.GetBytes());
  boost::static_string<64> static_str_out;
  boost::container::small_vector<uint8_t, 64> small_vector_out;
  boost::container::static_vector<uint8_t, 64> static_vector_out;
  deserializer(static_str_out, small_vector_out, static_vector_out);
  EXPECT_EQ(static_str_out, static_str_val);
  EXPECT_EQ(small_vector_out, small_vector_val);
  EXPECT_EQ(static_vector_out, static_vector_val);
}
TEST(Serialization, RemainingSkipPosition)
{
  NetBinaryWriter serializer;
  std::array<uint8_t, 5> data = {1, 2, 3, 4, 5};
  serializer(data[0], data[1], data[2], data[3], data[4]);

  NetBinaryReader deserializer(serializer.GetBytes());
  EXPECT_EQ(deserializer.Remaining(), 5);
  EXPECT_EQ(deserializer.Position(), 0);
  std::array<uint8_t, 5> output;
  deserializer(output[0], output[1]);
  EXPECT_EQ(output[0], data[0]);
  EXPECT_EQ(output[1], data[1]);
  EXPECT_EQ(deserializer.Remaining(), 3);
  EXPECT_EQ(deserializer.Position(), 2);
  deserializer.Skip(2);
  EXPECT_EQ(deserializer.Remaining(), 1);
  EXPECT_EQ(deserializer.Position(), 4);
  deserializer(output[4]);
  EXPECT_EQ(output[4], data[4]);
  EXPECT_EQ(deserializer.Remaining(), 0);
  EXPECT_EQ(deserializer.Position(), 5);
}