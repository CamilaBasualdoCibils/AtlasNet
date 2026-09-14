#pragma once

#include <concepts>
#include <expected>
#include <optional>
#include <string>
namespace AtlasNet
{
namespace Serialization
{
template <typename T>
concept StringLike =
    std::same_as<T, std::string> || std::same_as<T, std::wstring> ||
    std::same_as<T, std::u8string> || std::same_as<T, std::u16string> ||
    std::same_as<T, std::u32string>;

template <typename T>
concept ResizableContainer = requires(T c) {
  typename T::value_type;
  c.resize(std::size_t{});
  c.begin();
  c.end();
};
template<typename T>
concept FixedSizeContainer =
requires(T c)
{
    typename T::value_type;
    std::tuple_size<T>::value;
    c.begin();
    c.end();
};
template<typename T>
struct is_optional : std::false_type {};

template<typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template<typename T>
concept OptionalLike = is_optional<T>::value;

template<typename T>
struct is_expected : std::false_type {};

template<typename V, typename E>
struct is_expected<std::expected<V,E>> : std::true_type {};

template<typename T>
concept ExpectedLike = is_expected<T>::value;
} // namespace Serialization
} // namespace AtlasNet