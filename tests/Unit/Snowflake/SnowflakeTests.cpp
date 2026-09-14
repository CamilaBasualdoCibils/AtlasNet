
#include <gtest/gtest.h>
#include <random>
#include <unordered_map>

#include "AtlasNet/Core/Serialization/NetBinarySerializer.hpp"
#include "AtlasNet/Core/Types/Snowflake.hpp"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using SnowflakeId = AtlasNet::TSnowflake<12, 10, 42>;
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
auto logger = spdlog::stdout_color_mt("AtlasNet_tests_Snowflake");
TEST(Snowflake, DefaultConstructedIsZero)
{
  SnowflakeId id;

  EXPECT_EQ(id.getSequence(), 0);
  EXPECT_EQ(id.getWorker(), 0);
  EXPECT_EQ(id.getTimestamp(), 0);
}
TEST(Snowflake, PackZero)
{
  auto id = SnowflakeId::FromParts(0, 0, 0);

  EXPECT_EQ(id.getTimestamp(), 0);
  EXPECT_EQ(id.getWorker(), 0);
  EXPECT_EQ(id.getSequence(), 0);
}
TEST(Snowflake, PackValues)
{
  auto id = SnowflakeId::FromParts(123456789, 321, 4095);

  EXPECT_EQ(id.getTimestamp(), 123456789);
  EXPECT_EQ(id.getWorker(), 321);
  EXPECT_EQ(id.getSequence(), 4095);
}
TEST(Snowflake, MaximumValues)
{
  constexpr uint64_t ts = (1ULL << 42) - 1;

  constexpr uint64_t worker = (1ULL << 10) - 1;

  constexpr uint64_t seq = (1ULL << 12) - 1;

  auto id = SnowflakeId::FromParts(ts, worker, seq);

  EXPECT_EQ(id.getTimestamp(), ts);
  EXPECT_EQ(id.getWorker(), worker);
  EXPECT_EQ(id.getSequence(), seq);
}
TEST(Snowflake, OverflowIsMasked)
{
  auto id = SnowflakeId::FromParts(UINT64_MAX, UINT64_MAX, UINT64_MAX);

  EXPECT_EQ(id.getTimestamp(), (1ULL << 42) - 1);

  EXPECT_EQ(id.getWorker(), (1ULL << 10) - 1);

  EXPECT_EQ(id.getSequence(), (1ULL << 12) - 1);
}
TEST(Snowflake, StringRoundTrip)
{
  auto original = SnowflakeId::FromParts(123456, 42, 1337);

  auto str = original.to_string();
  logger->info("Original ID as string: {}", str);
  auto parsed = SnowflakeId::from_string(str);

  ASSERT_TRUE(parsed.has_value());

  EXPECT_EQ(parsed.value(), original);
}
TEST(Snowflake, ZeroToString)
{
  auto id = SnowflakeId::FromParts(0, 0, 0);

  SnowflakeId::Str str = id.to_string();
  EXPECT_EQ(str.find_first_not_of("0-"), std::string::npos);
}
TEST(Snowflake, MaximumToString)
{
  auto id = SnowflakeId::FromParts((1ULL << 42) - 1, (1ULL << 10) - 1,
                                   (1ULL << 12) - 1);

  auto parsed = SnowflakeId::from_string(id.to_string());

  ASSERT_TRUE(parsed);
  logger->info("Parsed ID: {}", parsed.value().to_string());
  EXPECT_EQ(*parsed, id);
}
TEST(Snowflake, InvalidStringLetters)
{
  EXPECT_FALSE(SnowflakeId::from_string("hello"));
}
TEST(Snowflake, EmptyString)
{
  EXPECT_FALSE(SnowflakeId::from_string(""));
}
TEST(Snowflake, OverflowString)
{
  EXPECT_FALSE(SnowflakeId::from_string("18446744073709551616"));
}
TEST(Snowflake, RejectTrailingCharacters)
{
  EXPECT_FALSE(SnowflakeId::from_string("123abc"));
}
TEST(Snowflake, Equality)
{
  auto a = SnowflakeId::FromParts(1, 2, 3);
  auto b = SnowflakeId::FromParts(1, 2, 3);

  EXPECT_EQ(a, b);
}
TEST(Snowflake, Inequality)
{
  auto a = SnowflakeId::FromParts(1, 2, 3);
  auto b = SnowflakeId::FromParts(1, 2, 4);

  EXPECT_NE(a, b);
}
TEST(Snowflake, Ordering)
{
  auto a = SnowflakeId::FromParts(1, 0, 0);
  auto b = SnowflakeId::FromParts(2, 0, 0);

  EXPECT_LT(a, b);
}
TEST(Snowflake, RandomizedPackUnpack)
{
  std::mt19937_64 rng(12345);

  constexpr uint64_t tsMask = (1ULL << 42) - 1;

  constexpr uint64_t workerMask = (1ULL << 10) - 1;

  constexpr uint64_t seqMask = (1ULL << 12) - 1;

  for (int i = 0; i < 100000; ++i)
  {
    auto ts = rng() & tsMask;
    auto worker = rng() & workerMask;
    auto seq = rng() & seqMask;

    auto id = SnowflakeId::FromParts(ts, worker, seq);

    EXPECT_EQ(id.getTimestamp(), ts);

    EXPECT_EQ(id.getWorker(), worker);

    EXPECT_EQ(id.getSequence(), seq);
  }
}
TEST(Snowflake, RandomizedStringRoundTrip)
{
  std::mt19937_64 rng(123);

  for (int i = 0; i < 100000; ++i)
  {
    uint64_t value = rng();
    auto id = SnowflakeId(value);

    auto parsed = SnowflakeId::from_string(id.to_string());

    ASSERT_TRUE(parsed);

    EXPECT_EQ(*parsed, id);
    EXPECT_EQ(value, parsed.value().value());
  }
}
TEST(Snowflake, Serialize)
{
  const SnowflakeId original = SnowflakeId::FromParts(123456, 42, 1337);
  AtlasNet::NetBinaryWriter writer;
  writer(original);

  AtlasNet::NetBinaryReader reader(writer.GetBytes());
  SnowflakeId deserialized;
  reader(deserialized);
  EXPECT_EQ(original, deserialized);
}
