/* Retained-view Streaming tail policy.
 *
 * A small, owner-thread-only snapshot that separates user viewport intent
 * from asynchronous content growth. This is a FlexUI layout policy; stream
 * admission, event ordering and byte ownership belong to the application.
 * Never retain an Element* across tree retirement.
 */
#ifndef FLEXUI_STREAM_SCROLL_FOLLOW_H
#define FLEXUI_STREAM_SCROLL_FOLLOW_H

#include <flexUI/element.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

struct StreamScrollFollow {
  bool was_at_tail = false;

  // Capture before publishing new Markdown and recalculating layout.
  // The owning event loop must not dispatch user scrolling between capture
  // and apply. A non-scrollable, existing scroll container is at its tail.
  static StreamScrollFollow capture(const Element &scroll,
                                    float tolerance = 2.0f) noexcept {
    if (!scroll.is_scroll_container()) return {};
    if (!std::isfinite(tolerance) || tolerance < 0.0f) tolerance = 2.0f;
    if (!std::isfinite(scroll.scroll_y()) ||
        !std::isfinite(scroll.max_scroll_y())) return {};
    return {scroll.max_scroll_y() - scroll.scroll_y() <= tolerance};
  }

  // Apply only after the owner has committed its new layout. When the user
  // scrolled away, preserve their existing viewport; never force tail.
  bool apply(Element &scroll) const {
    if (!was_at_tail || !scroll.is_scroll_container()) return false;
    return scroll.set_scroll_offset(scroll.scroll_x(), scroll.max_scroll_y());
  }
};

}  // namespace flexUI

#endif  // FLEXUI_STREAM_SCROLL_FOLLOW_H
