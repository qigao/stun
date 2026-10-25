/* Bounded application-side FlexUI/Tango transcript projection.
 *
 * This adapter belongs to the optional TUI backend, not Tango C11 or the
 * FlexUI CSS/runtime core. The Tango registry owns stream bytes and message
 * identity; Box owns the fixed Element/Widget slots. Window changes recycle
 * those same Elements (and give each new message ID a fresh UiHandle).
 *
 * Call all methods only on the existing UI owner. Destroy this borrowed
 * adapter before destroying Box. It never owns or retains C11 text pointers.
 */
#ifndef FLEX_TUI_TRANSCRIPT_WINDOW_H
#define FLEX_TUI_TRANSCRIPT_WINDOW_H

#include <flexUI/box.h>
#include <flexUI/stream_scroll_follow.h>
#include <flexUI/widgets/markdown_widget.h>
#include <tui_transcript.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flex::tui_backend {

class TranscriptWindow {
public:
  enum class Status {
    Updated,
    Unchanged,
    Invalid,
    QueryRejected,
  };

  // max_visible bounds both the number of permanent Box-owned host/widget
  // pairs and the maximum number of projected messages in a frame.
  // All slots are id-less until assigned; no index entry is created by
  // construction. The application owns viewport, Box and registry lifetimes.
  TranscriptWindow(flexUI::Box& box, flexUI::Element& viewport,
                   std::size_t max_visible)
      : box_(box), viewport_(viewport) {
    if (max_visible == 0u || max_visible > 64u ||
        viewport.is_widget_owned()) {
      throw std::invalid_argument("invalid bounded transcript viewport");
    }
    slots_.reserve(max_visible);
    scratch_.resize(max_visible);
    for (std::size_t i = 0u; i < max_visible; ++i) {
      auto* host = box_.create_widget<flexUI::MarkdownWidget>(
          "div", "", std::string{});
      if (!host || !host->widget) {
        throw std::runtime_error("cannot allocate transcript slot");
      }
      host->add_class("transcript-message");
      host->focusable = false;
      slots_.push_back({
        host, static_cast<flexUI::MarkdownWidget*>(host->widget), 0u
      });
    }
  }

  TranscriptWindow(const TranscriptWindow&) = delete;
  TranscriptWindow& operator=(const TranscriptWindow&) = delete;

  std::size_t slot_capacity() const noexcept { return slots_.size(); }
  std::size_t visible_count() const noexcept { return visible_.size(); }

  // A stable host and UiHandle remain stable as long as message_id remains
  // in the visible window, even as source bytes or global window order change.
  flexUI::Element* visible_host(std::uint64_t message_id) const noexcept {
    for (auto* slot : visible_)
      if (slot->message_id == message_id) return slot->host;
    return nullptr;
  }

  // Compatibility surface: ordinary callers preserve existing viewport
  // intent. Interactive navigation chooses Tail or Anchor explicitly.
  Status show(const tui_transcript_t* transcript, std::size_t first) {
    return show_with_policy(transcript, first, ScrollPolicy::PreserveIntent);
  }

private:
  enum class ScrollPolicy { PreserveIntent, FollowTail, AnchorTop };

  Status show_with_policy(const tui_transcript_t* transcript,
                          std::size_t first, ScrollPolicy policy) {
    if (!transcript) return Status::Invalid;

    std::size_t count = 0u;
    if (tui_transcript_window(transcript, first, scratch_.size(),
                              scratch_.data(), scratch_.size(), &count) !=
        TUI_TRANSCRIPT_OK) {
      return Status::QueryRejected;
    }

    // Stage all owned source copies and identities before changing any live
    // Box or Widget. Borrowed native stream text never survives this scope.
    std::vector<Desired> desired;
    desired.reserve(count);
    for (std::size_t i = 0u; i < count; ++i) {
      const auto id = scratch_[i].message_id;
      std::size_t bytes = 0u;
      const char* text = tui_transcript_text(transcript, id, &bytes);
      if (!text) return Status::QueryRejected;
      desired.push_back({
          id, std::string(text, bytes), std::string("transcript-msg-") +
          std::to_string(id)
      });
    }

    std::vector<Slot*> next;
    next.reserve(count);
    for (const auto& item : desired) {
      Slot* existing = nullptr;
      for (auto& slot : slots_)
        if (slot.message_id == item.id) { existing = &slot; break; }
      next.push_back(existing);
    }
    for (std::size_t i = 0u; i < count; ++i) {
      if (next[i]) continue;
      for (auto& slot : slots_) {
        if (std::find(next.begin(), next.end(), &slot) != next.end())
          continue;
        next[i] = &slot;
        break;
      }
      if (!next[i]) return Status::QueryRejected;
    }

    bool different = count != visible_.size();
    for (std::size_t i = 0u; i < count && !different; ++i)
      if (next[i] != visible_[i] ||
          next[i]->markdown->markdown() != desired[i].text)
        different = true;
    // Anchor semantics are independent of the old viewport's scroll tail.
    // A different first visible message starts at the top; an unchanged
    // anchor retains its numeric scroll position across streaming reflow.
    const bool different_anchor = count != 0u &&
        (visible_.empty() || visible_[0]->message_id != desired[0].id);
    if (!different) {
      if (policy == ScrollPolicy::FollowTail &&
          viewport_.set_scroll_offset(viewport_.scroll_x(),
                                      viewport_.max_scroll_y())) {
        box_.update();
        return Status::Updated;
      }
      return Status::Unchanged;
    }

    const auto follow = flexUI::StreamScrollFollow::before_content_update(
        viewport_);

    // Invalidate focus and capture on any recycled host *before* replacing
    // its message identity. A widget must not inherit the previous message's
    // interactions, and old generational UiHandles become unresolvable.
    for (auto& slot : slots_) {
      const auto it = std::find(next.begin(), next.end(), &slot);
      const std::size_t index = static_cast<std::size_t>(it - next.begin());
      const std::uint64_t next_id =
          it == next.end() ? 0u : desired[index].id;
      if (slot.message_id != next_id) {
        if (has_ancestor(box_.focused_element(), slot.host))
          box_.set_focus(nullptr);
        if (has_ancestor(box_.capturing_element(), slot.host))
          box_.release_mouse_capture(box_.capturing_element());
        slot.host->set_element_id(
            next_id ? desired[index].name : std::string{});
        slot.message_id = next_id;
      }
      if (!next_id) {
        slot.markdown->set_markdown("");
        slot.markdown->update(0.0f, *slot.host);
      }
    }

    std::vector<flexUI::Element*> children;
    children.reserve(count);
    for (std::size_t i = 0u; i < count; ++i) {
      auto* slot = next[i];
      slot->markdown->set_markdown(desired[i].text);
      slot->markdown->update(0.0f, *slot->host);
      children.push_back(slot->host);
    }
    viewport_.replace_children(children);
    visible_ = std::move(next);
    box_.invalidate();
    box_.update();
    if (policy == ScrollPolicy::FollowTail) {
      (void)viewport_.set_scroll_offset(viewport_.scroll_x(),
                                        viewport_.max_scroll_y());
    } else if (policy == ScrollPolicy::AnchorTop) {
      if (different_anchor)
        (void)viewport_.set_scroll_offset(viewport_.scroll_x(), 0.0f);
    } else {
      (void)follow.apply(viewport_);
    }
    return Status::Updated;
  }

public:
  // Show the most recent N retained messages. Window identity and scroll
  // position remain separate: clients may explicitly call show_from_id()
  // when the user scrolls away from the tail.
  Status show_tail(const tui_transcript_t* transcript) {
    if (!transcript) return Status::Invalid;
    const std::size_t count = tui_transcript_count(transcript);
    return show_with_policy(
        transcript, count > slots_.size() ? count - slots_.size() : 0u,
        ScrollPolicy::FollowTail);
  }

  // A message-ID anchor is stable through earlier settled-message evictions.
  // If the anchor was evicted, clamp to oldest retained. An anchor newer than
  // every retained message clamps to the latest bounded visible window:
  // navigation must not unexpectedly jump back to the oldest entry.
  // This bounded scan copies no native source or pointer to evicted data.
  Status show_from_id(const tui_transcript_t* transcript,
                      std::uint64_t first_message_id) {
    if (!transcript || !first_message_id) return Status::Invalid;
    const std::size_t count = tui_transcript_count(transcript);
    std::size_t start = count;
    for (std::size_t i = 0u; i < count; ++i) {
      tui_transcript_message_t record{};
      std::size_t written = 0u;
      if (tui_transcript_window(transcript, i, 1u, &record, 1u,
                                 &written) != TUI_TRANSCRIPT_OK ||
          written != 1u) return Status::QueryRejected;
      if (record.message_id >= first_message_id) {
        start = i;
        break;
      }
    }
    if (start == count) {
      // Future/nonexistent high anchors follow the newest window. Zero
      // retained items still project as an empty bounded viewport.
      start = count > slots_.size() ? count - slots_.size() : 0u;
    }
    return show_with_policy(transcript, start, ScrollPolicy::AnchorTop);
  }

private:
  struct Slot {
    flexUI::Element* host;
    flexUI::MarkdownWidget* markdown;
    std::uint64_t message_id;
  };
  struct Desired {
    std::uint64_t id;
    std::string text;
    std::string name;
  };

  static bool has_ancestor(const flexUI::Element* node,
                           const flexUI::Element* ancestor) noexcept {
    while (node) {
      if (node == ancestor) return true;
      node = node->parent_elem();
    }
    return false;
  }

  flexUI::Box& box_;
  flexUI::Element& viewport_;
  std::vector<Slot> slots_;
  std::vector<Slot*> visible_;
  std::vector<tui_transcript_message_t> scratch_;
};

}  // namespace flex::tui_backend
#endif  // FLEX_TUI_TRANSCRIPT_WINDOW_H
