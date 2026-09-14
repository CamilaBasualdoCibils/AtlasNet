#pragma once
#include <expected>
#include <type_traits>
namespace AtlasNet::Serialization
{
template <typename T> struct is_std_expected : std::false_type
{
};

template <typename T, typename E>
struct is_std_expected<std::expected<T, E>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_std_expected_v =
    is_std_expected<std::remove_cvref_t<T>>::value;
} // namespace AtlasNet::Serialization