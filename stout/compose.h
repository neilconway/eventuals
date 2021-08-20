#pragma once

#include "glog/logging.h"

////////////////////////////////////////////////////////////////////////

inline bool StoutEventualsLog(int level) {
  static const char* variable = std::getenv("STOUT_EVENTUALS_LOG");
  static int value = variable != nullptr ? atoi(variable) : 0;
  return value >= level;
}

#define STOUT_EVENTUALS_LOG(level) LOG_IF(INFO, StoutEventualsLog(level))

////////////////////////////////////////////////////////////////////////

namespace stout {
namespace eventuals {

////////////////////////////////////////////////////////////////////////

namespace detail {

////////////////////////////////////////////////////////////////////////

template <typename Left_, typename Right_>
struct Composed {
  Left_ left_;
  Right_ right_;

  template <typename Arg>
  using ValueFrom = typename Right_::template ValueFrom<
      typename Left_::template ValueFrom<Arg>>;

  template <typename Arg>
  auto k() && {
    using Value = typename Left_::template ValueFrom<Arg>;
    return std::move(left_)
        .template k<Arg>(std::move(right_).template k<Value>());
  }

  template <typename Arg, typename K>
  auto k(K k) && {
    using Value = typename Left_::template ValueFrom<Arg>;
    return std::move(left_)
        .template k<Arg>(std::move(right_).template k<Value>(std::move(k)));
  }
};

////////////////////////////////////////////////////////////////////////

template <typename Left, typename Right>
auto operator|(Left left, Right right) {
  return Composed<Left, Right>{std::move(left), std::move(right)};
}

////////////////////////////////////////////////////////////////////////

} // namespace detail

////////////////////////////////////////////////////////////////////////

} // namespace eventuals
} // namespace stout

////////////////////////////////////////////////////////////////////////
