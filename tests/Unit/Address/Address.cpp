
#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/EndPoint.hpp"
#include <gtest/gtest.h>
#include <unordered_set>

TEST(Address, IPV4)
{
  IPv4Address addr("127.0.0.1");
  EXPECT_EQ(addr.to_string(), "127.0.0.1");
  EXPECT_EQ(addr[0], 127);
  EXPECT_EQ(addr[1], 0);
  EXPECT_EQ(addr[2], 0);
  EXPECT_EQ(addr[3], 1);
}
TEST(Address, IPV6)
{
  IPv6Address addr("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  EXPECT_EQ(addr.to_string(), "2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  EXPECT_EQ(addr[0], 0x20);
  EXPECT_EQ(addr[1], 0x01);
  EXPECT_EQ(addr[2], 0x0d);
  EXPECT_EQ(addr[3], 0xb8);
  EXPECT_EQ(addr[4], 0x85);
  EXPECT_EQ(addr[5], 0xa3);
  EXPECT_EQ(addr[6], 0x00);
  EXPECT_EQ(addr[7], 0x00);
  EXPECT_EQ(addr[8], 0x00);
  EXPECT_EQ(addr[9], 0x00);
  EXPECT_EQ(addr[10], 0x8a);
  EXPECT_EQ(addr[11], 0x2e);
  EXPECT_EQ(addr[12], 0x03);
  EXPECT_EQ(addr[13], 0x70);
  EXPECT_EQ(addr[14], 0x73);
  EXPECT_EQ(addr[15], 0x34);
}
TEST(Address, DNS)
{
  DNSAddress addr("localhost");
  EXPECT_EQ(addr.to_string(), "localhost");
}
TEST(Address, DNSResolve)
{
  DNSAddress addr("localhost");
  auto resolved = addr.resolve();
  EXPECT_TRUE(resolved.has_value());
  EXPECT_TRUE(std::holds_alternative<IPv4Address>(*resolved) ||
              std::holds_alternative<IPv6Address>(*resolved));

  if (std::holds_alternative<IPv4Address>(*resolved))
  {
    const IPv4Address& ipv4 = std::get<IPv4Address>(*resolved);
    const IPv4Address localHost(127, 0, 0, 1);
    EXPECT_EQ(ipv4, localHost);
  }
  else if (std::holds_alternative<IPv6Address>(*resolved))
  {
    const IPv6Address& ipv6 = std::get<IPv6Address>(*resolved);
    // We won't assert the exact address since it can vary, but we can check
    // that it's not all zeros

    const IPv6Address localHost(0, 0, 0, 0, 0, 0, 0, 1);
    EXPECT_EQ(ipv6, localHost);
  }
}
TEST(Address, DeduceIpv4)
{
  EndPointAddress endpoint("192.25.126.245:8080");

  EXPECT_TRUE(endpoint.IsIpv4());
  EXPECT_EQ(endpoint.get_port(), 8080);
}
TEST(Address, DeduceIpv6)
{
  EndPointAddress endpoint("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:8080");

  EXPECT_TRUE(endpoint.IsIpv6());
  EXPECT_EQ(endpoint.get_port(), 8080);
}
TEST(Address, DeduceDns)
{
  EndPointAddress endpoint("localhost:8080");
  endpoint.parse_string("localhost:8080");
  EXPECT_TRUE(endpoint.IsDNS());
  EXPECT_EQ(endpoint.get_port(), 8080);
}
TEST(Address, DeduceInvalid)
{

  EXPECT_ANY_THROW(
      []()
      {
        EndPointAddress addr("invalid_address"); // not an address
      }());
  EXPECT_ANY_THROW(
      []()
      {
        EndPointAddress addr("256.256.256.256"); // invalid IPv4
      }());

  EXPECT_ANY_THROW(
      []()
      {
        EndPointAddress addr(
            "[2001:0db8:85a3:0000:0000:8a2e:0370:7334g]:8080"); // invalid ipv6
      }());
  EXPECT_ANY_THROW(
      []()
      {
        EndPointAddress addr(
            "2001:0db8:85a3:0000:0000:8a2e:0370:7334:8080"); // missing brackets
                                                             // for IPv6
      }());
  EXPECT_ANY_THROW(
      []()
      {
        EndPointAddress addr("localhost"); // missing port
      }());
  EXPECT_ANY_THROW(
      []()
      {
        EndPointAddress addr("localhost:notaport"); // invalid port
      }());
}
TEST(Address, AddrHashing)
{
  std::unordered_set<IPv4Address> ip4addressSet;
  std::unordered_set<IPv6Address> ip6addressSet;
  std::unordered_set<DNSAddress> dnsAddressSet;
  IPv4Address addr1(192, 125, 6, 21);
  IPv6Address addr2("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  DNSAddress addr3("localhost");

  ip4addressSet.insert(addr1);
  ip6addressSet.insert(addr2);
  dnsAddressSet.insert(addr3);

  EXPECT_TRUE(ip4addressSet.contains(addr1));
  EXPECT_TRUE(ip6addressSet.contains(addr2));
  EXPECT_TRUE(dnsAddressSet.contains(addr3));
}
TEST(Address, EndPointHashing)
{
  std::unordered_set<EndPointAddress> addressSet;
  EndPointAddress addr1("192.125.6.21:8080");
  EndPointAddress addr2(IPv6Address("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
                        8080);
  EndPointAddress addr3("localhost:8080");

  EndPointAddress addr4("1.1.1.1:8080");
  addressSet.insert(addr1);
  addressSet.insert(addr2);
  addressSet.insert(addr3);

  EXPECT_TRUE(addressSet.contains(addr1));
  EXPECT_TRUE(addressSet.contains(addr2));
  EXPECT_TRUE(addressSet.contains(addr3));
  EXPECT_FALSE(addressSet.contains(addr4));
}

TEST(Address, AddrEquality)
{
  IPv4Address addr1(192, 125, 6, 21);
  IPv4Address addr2(192, 125, 6, 21);
  IPv4Address addr3(192, 125, 6, 22);

  IPv6Address addr4("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  IPv6Address addr5("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  IPv6Address addr6("2001:0db8:85a3:0000:0000:8a2e:0370:7335");

  DNSAddress addr7("localhost");
  DNSAddress addr8("localhost");
  DNSAddress addr9("localsuperhost");

  EXPECT_EQ(addr1, addr2);
  EXPECT_NE(addr1, addr3);
  EXPECT_EQ(addr4, addr5);
  EXPECT_NE(addr4, addr6);
  EXPECT_EQ(addr7, addr8);
  EXPECT_NE(addr7, addr9);
}
TEST(Address, EndPointEquality)
{
  EndPointAddress addr1(IPv4Address(192, 125, 6, 21), 8080);
  EndPointAddress addr2(IPv4Address(192, 125, 6, 21), 8080);
  EndPointAddress addr3(IPv4Address(192, 125, 6, 22), 8080);

  EndPointAddress addr4(IPv6Address("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
                        8080);
  EndPointAddress addr5(IPv6Address("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
                        8080);
  EndPointAddress addr6(IPv6Address("2001:0db8:85a3:0000:0000:8a2e:0370:7335"),
                        8080);

  EndPointAddress addr7("localhost:8080");
  EndPointAddress addr8("localhost:8080");
  EndPointAddress addr9("localhost:8081");

  EXPECT_EQ(addr1, addr2);
  EXPECT_NE(addr1, addr3);
  EXPECT_EQ(addr4, addr5);
  EXPECT_NE(addr4, addr6);
  EXPECT_EQ(addr7, addr8);
  EXPECT_NE(addr7, addr9);
}
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}