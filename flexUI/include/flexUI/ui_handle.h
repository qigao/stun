#pragma once

#include <cstdint>
#include <string>

namespace flexUI {

/// Stable value passed across UI/runtime boundaries instead of Element*.
/// A handle resolves only while its id still names the same Box index
/// incarnation. An empty id or zero generation is always invalid.
struct UiHandle {
  std::string id;
  std::uint64_t generation = 0;

  explicit operator bool() const noexcept {
    return !id.empty() && generation != 0;
  }
};

} // namespace flexUI
