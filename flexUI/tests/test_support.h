#pragma once

#include <cmath>
#include <type_traits>

template <typename Actual, typename Expected, typename Margin>
inline bool approx_eq(const Actual& actual, const Expected& expected,
                      const Margin& margin = Margin{}) {
  using Common = std::common_type_t<Actual, Expected, Margin>;
  return std::fabs(static_cast<Common>(actual) - static_cast<Common>(expected)) <=
         static_cast<Common>(margin);
}
