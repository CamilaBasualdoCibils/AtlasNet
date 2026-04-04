#pragma once

#include "JobEnums.hpp"
#include <chrono>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace AtlasNet::JobOpts
{

struct Name
{
  std::string value;
};

struct RepeatOnce
{
  std::chrono::milliseconds interval{0};
};

template <JobPriority P>
struct TPriority
{
  static constexpr JobPriority value = P;
};
struct Priority
{
const JobPriority value;
Priority(const JobPriority v) : value(v) {}
};

template <JobNotifyLevel N>
struct Notify
{
  static constexpr JobNotifyLevel value = N;
};

template <typename F, typename... Opts>
struct OnCompleteT
{
  [[no_unique_address]] F func;
  std::tuple<Opts...> opts;
};

template <typename F, typename... Opts>
auto OnComplete(F&& f, Opts&&... opts)
{
  return OnCompleteT<std::decay_t<F>, std::decay_t<Opts>...>{
      std::forward<F>(f),
      std::tuple<std::decay_t<Opts>...>(std::forward<Opts>(opts)...)
  };
}

template <typename T>
struct IsName : std::false_type
{
};

template <>
struct IsName<Name> : std::true_type
{
};

template <typename T>
inline constexpr bool IsNameV = IsName<std::remove_cvref_t<T>>::value;

template <typename T>
struct IsRepeat : std::false_type
{
};

template <>
struct IsRepeat<RepeatOnce> : std::true_type
{
};

template <typename T>
inline constexpr bool IsRepeatV = IsRepeat<std::remove_cvref_t<T>>::value;

template <typename T>
struct IsPriority : std::false_type
{
};

template <JobPriority P>
struct IsPriority<TPriority<P>> : std::true_type
{
};
template <>
struct IsPriority<Priority> : std::true_type
{
};
template <typename T>
inline constexpr bool IsPriorityV =
    IsPriority<std::remove_cvref_t<T>>::value;

template <typename T>
struct IsNotify : std::false_type
{
};

template <JobNotifyLevel N>
struct IsNotify<Notify<N>> : std::true_type
{
};

template <typename T>
inline constexpr bool IsNotifyV = IsNotify<std::remove_cvref_t<T>>::value;

template <typename T>
struct IsOnComplete : std::false_type
{
};

template <typename F, typename... Opts>
struct IsOnComplete<OnCompleteT<F, Opts...>> : std::true_type
{
};

template <typename T>
inline constexpr bool IsOnCompleteV =
    IsOnComplete<std::remove_cvref_t<T>>::value;

template <typename T>
inline constexpr bool IsSupportedJobOptionV =
    IsNameV<T> || IsRepeatV<T> || IsPriorityV<T> || IsNotifyV<T> ||
    IsOnCompleteV<T>;

template <template <typename> class Pred, typename... Ts>
inline constexpr std::size_t CountIfV =
    (std::size_t{0} + ... + (Pred<std::remove_cvref_t<Ts>>::value ? 1u : 0u));

} // namespace AtlasNet