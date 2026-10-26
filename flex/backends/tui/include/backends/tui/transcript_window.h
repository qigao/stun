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

#include <backends/tui/transcript_height_index.h>
#include <flexUI/box.h>
#include <flexUI/stream_scroll_follow.h>
#include <flexUI/widgets/markdown_widget.h>
#include <tui_transcript.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flex::tui_backend {

/* UI-owner-only presentation coordinate; no borrowed Element* or C11 bytes.
 * The offset uses FlexUI layout coordinates (the TUI maps these to cells).
 * This first slice preserves anchors among bounded visible hosts, not an
 * all-history variable-height virtualization or a character-level cursor. */
struct TranscriptScrollAnchor {
  std::uint64_t message_id = 0u;
  float within_message_y = 0.0f;
  bool follow_tail = false;
};

class TranscriptWindow {
public:
  enum class Status {
    Updated,
    Unchanged,
    Invalid,
    QueryRejected,
    ResourceExhausted,
  };

  // max_visible bounds both the number of permanent Box-owned host/widget
  // pairs and the maximum number of projected messages in a frame.
  // All slots are id-less until assigned; no index entry is created by
  // construction. The application owns viewport, Box and registry lifetimes.
  // Private UI-owner staging allocator: allocation returns nullptr on
  // resource exhaustion. Production uses real malloc/free, while tests
  // inject a bounded malloc-backed failure. No C11/Tango ABI changes.
  // Both callbacks must be supplied together (or both omitted).
  struct StagingAllocation {
    void* context = nullptr;
    void* (*allocate)(void*, std::size_t) noexcept = nullptr;
    void (*deallocate)(void*, void*) noexcept = nullptr;
  };

  TranscriptWindow(flexUI::Box& box, flexUI::Element& viewport,
                   std::size_t max_visible)
      : TranscriptWindow(box, viewport, max_visible, StagingAllocation{}) {}

  TranscriptWindow(flexUI::Box& box, flexUI::Element& viewport,
                   std::size_t max_visible, StagingAllocation staging)
      : box_(box), viewport_(viewport), staging_(staging) {
    if (max_visible == 0u || max_visible > 64u ||
        (bool(staging.allocate) != bool(staging.deallocate)) ||
        viewport.is_widget_owned()) {
      throw std::invalid_argument("invalid bounded transcript viewport");
    }
    slots_.reserve(max_visible);
    // The public visible slot list is permanently bounded by max_visible.
    // Reserve before any live projection so pointer assignment cannot
    // allocate after the first host identity is mutated.
    visible_.reserve(max_visible);
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
  // Private adapter diagnostics for deterministic wheel-path qualification;
  // not a public Tango ABI counter or a cross-thread observable.
  std::size_t in_message_scroll_fast_paths() const noexcept {
    return in_message_scroll_fast_paths_;
  }
  // Private owner-only diagnostic: one pass means Box was updated and at
  // least one valid visible message height was sampled afterwards.
  std::size_t visible_height_sample_passes() const noexcept {
    return visible_height_sample_passes_;
  }
  std::uint64_t first_visible_message_id() const noexcept {
    return visible_.empty() ? 0u : visible_.front()->message_id;
  }

  // Optional, value-only geometry tracking, enabled by the UI owner before
  // its first projection. Defaults OFF for all existing TUI consumers.
  // Box ownership and native C11 stream lifetimes remain unchanged.
  void enable_height_tracking(std::size_t retained_capacity,
                              float fallback_height) {
    if (!visible_.empty())
      throw std::logic_error("enable bounded height tracking before show()");
    heights_ = std::make_unique<TranscriptHeightIndex>(
        retained_capacity, fallback_height);
  }

  void set_height_layout_revision(std::uint64_t revision) noexcept {
    layout_revision_ = revision;
  }

  const TranscriptHeightIndex* tracked_heights() const noexcept {
    return heights_.get();
  }

  // Experimental opt-in: two permanent Box-owned spacer Elements represent
  // offscreen retained height estimates without full-history DOM inflation.
  // The caller must load:
  // .tui-virtual-spacer { height:var(--tui-virtual-height);
  //                       flex-shrink:0; width:100%; }
  // This is NOT scroll-position-to-window admission or exact unseen geometry.
  void enable_virtual_spacers() {
    if (!heights_ || !visible_.empty() || leading_spacer_ ||
        trailing_spacer_)
      throw std::logic_error("enable spacers once before first projection");
    leading_spacer_ = box_.create("div");
    trailing_spacer_ = box_.create("div");
    if (!leading_spacer_ || !trailing_spacer_)
      throw std::runtime_error("cannot allocate bounded virtual spacers");
    leading_spacer_->add_class("tui-virtual-spacer");
    trailing_spacer_->add_class("tui-virtual-spacer");
    leading_spacer_->set_custom_property("--tui-virtual-height", "0px");
    trailing_spacer_->set_custom_property("--tui-virtual-height", "0px");
  }

  bool virtual_spacers_enabled() const noexcept {
    return leading_spacer_ != nullptr;
  }

  // Private opt-in value lookup. Unknown offscreen source heights retain
  // prefix_exact=false/message_measured=false: never claim a pixel-exact
  // anchor until this message has been projected and measured by Box.
  std::optional<TranscriptHeightIndex::LocatedOffset> virtual_location(
      double scroll_y) const noexcept {
    return heights_ && leading_spacer_
        ? heights_->locate_offset(scroll_y) : std::nullopt;
  }

  // Called only on the existing UI Owner after a real scroll action reaches
  // an estimated spacer. Select the bounded host window for that retained
  // message and restore a local scroll position after actual Box layout.
  // No new input loop, actor, parser or application-owned offscreen nodes.
  Status show_at_scroll_offset(const tui_transcript_t* transcript,
                               double scroll_y,
                               TranscriptScrollAnchor* resolved = nullptr) {
    if (!transcript || !heights_ || !leading_spacer_ ||
        !std::isfinite(scroll_y) || scroll_y < 0.0)
      return Status::Invalid;
    // Do not publish the new retained height index before the bounded
    // Markdown/Slot* plan is admitted. If a staging allocation rejects,
    // UI identity, layout samples and scroll position all remain old.
    const auto located = heights_->preview_locate_offset(
        transcript, viewport_.width(), layout_revision_, scroll_y);
    if (!located || !std::isfinite(located->local_y) ||
        located->local_y > 10000000.0)
      return Status::QueryRejected;
    const TranscriptScrollAnchor mapped{
        located->message_id, static_cast<float>(located->local_y), false};
    const Status projected = show_preserving_anchor(transcript, mapped);
    if (resolved && (projected == Status::Updated ||
                     projected == Status::Unchanged)) {
      *resolved = mapped;
      // This ID may have occupied a 400px estimated spacer but measure
      // only a few real Box pixels once brought into the bounded window.
      // Never hand an impossible estimated local offset back to the next
      // owner wheel turn. This is a clamped message-local anchor, not a
      // claim of exact offscreen pixel geometry.
      const auto measured = heights_->lookup(mapped.message_id);
      if (measured && measured->measured)
        resolved->within_message_y = std::clamp(
            mapped.within_message_y, 0.0f, measured->height);
    }
    return projected;
  }

  // Take a REAL owner scroll delta (one or more cell/scroll-wheel steps)
  // into the bounded virtual height domain. Source heights not yet laid
  // out remain estimates and become accurate only after Box projection.
  // Clamp against the actual Box scroll extent to avoid falsely following
  // the tail or fabricating a message outside the retained C11 registry.
  // This optional adapter method does not modify native Tango input ABI.
  Status scroll_by_offset(const tui_transcript_t* transcript,
                          double delta_y,
                          TranscriptScrollAnchor* resolved = nullptr) {
    if (!transcript || !heights_ || !leading_spacer_ ||
        !std::isfinite(delta_y) || !viewport_.is_scroll_container() ||
        !std::isfinite(viewport_.scroll_y()) ||
        !std::isfinite(viewport_.max_scroll_y()) ||
        viewport_.max_scroll_y() < 0.0f)
      return Status::Invalid;
    const double current = viewport_.scroll_y();
    const double maximum = viewport_.max_scroll_y();
    const double target = std::clamp(current + delta_y, 0.0, maximum);
    return show_at_scroll_offset(transcript, target, resolved);
  }

  // A history scroll can arrive in the SAME Owner turn as settled-message
  // eviction and Box resize. Old absolute scroll_y then has a stale origin.
  // Rebase a value-only message/local anchor against the NEW retained index
  // before applying the relative step, rather than interpreting a removed
  // prefix as still existing. If the old ID has been evicted, clamp to the
  // new oldest retained message at local zero, never to tail.
  Status scroll_from_anchor(const tui_transcript_t* transcript,
                            TranscriptScrollAnchor anchor, double delta_y,
                            TranscriptScrollAnchor* resolved = nullptr) {
    if (!transcript || !heights_ || !leading_spacer_ ||
        anchor.follow_tail || !anchor.message_id ||
        !std::isfinite(anchor.within_message_y) ||
        anchor.within_message_y < 0.0f || !std::isfinite(delta_y) ||
        !viewport_.is_scroll_container() ||
        !std::isfinite(viewport_.width()) ||
        !std::isfinite(viewport_.height()) ||
        viewport_.height() < 0.0f)
      return Status::Invalid;
    // Rebase a semantic message/local anchor via ACTUAL Box layout before
    // applying the wheel delta. A resize can invalidate every cached
    // sample and replace a 500px GFM document with a 32px placeholder.
    // Using that placeholder to clamp within_message_y would jump into
    // the NEXT message even while the original message is still retained.
    // This bounded projection also clamps a newly evicted anchor to the
    // oldest retained ID at offset zero, never silently returning to tail.
    // No native text or Element pointer is borrowed across the projection.
    const Status rebased = show_preserving_anchor(transcript, anchor);
    if (rebased != Status::Updated && rebased != Status::Unchanged)
      return rebased;

    // Fast-path an in-message wheel move after the semantic anchor has
    // already been rebound and measured by Box. No second C11 snapshot,
    // Markdown projection or height-index sync is needed when the target
    // remains in the SAME retained host and inside the actual viewport
    // scroll extent. Boundary crossings still use the full bounded map.
    // This optimization is private to the experimental C++ adapter.
    for (const Slot* slot : visible_) {
      if (slot->message_id != anchor.message_id) continue;
      const double top = slot->host->y();
      const double height = slot->host->height();
      const double maximum = viewport_.max_scroll_y();
      if (!std::isfinite(top) || !std::isfinite(height) ||
          !std::isfinite(maximum) || height <= 0.0 ||
          maximum < 0.0)
        break;
      const double local = std::clamp(
          static_cast<double>(anchor.within_message_y), 0.0, height);
      const double next_local = local + delta_y;
      const double next_absolute = top + next_local;
      if (std::isfinite(next_local) &&
          std::isfinite(next_absolute) &&
          next_local >= 0.0 && next_local < height &&
          next_absolute >= 0.0 && next_absolute <= maximum) {
        const bool moved = viewport_.set_scroll_offset(
            viewport_.scroll_x(), static_cast<float>(next_absolute));
        if (moved) box_.update();
        ++in_message_scroll_fast_paths_;
        if (resolved)
          *resolved = {anchor.message_id,
                       static_cast<float>(next_local), false};
        return moved ? Status::Updated : rebased;
      }
      break;
    }
    // Crossing a message boundary, an evicted ID, or a temporarily
    // non-scrollable viewport must NOT reinterpret the user's semantic
    // message/local anchor as the Box's transient numeric scroll_y.
    // In particular, a reprojected GFM host can be measured and retained
    // while Box has briefly clamped scroll_y to zero. That zero is not the
    // previously admitted 4px message-local anchor. Reconstruct the
    // retained coordinate AFTER the Box remeasurement above.
    double base = 0.0;
    const auto prefix = heights_->height_before(anchor.message_id);
    const auto own_height = heights_->height_for(anchor.message_id);
    if (prefix && own_height) {
      base = *prefix + std::clamp(
          static_cast<double>(anchor.within_message_y),
          0.0, *own_height);
    }
    // If the original ID was evicted, show_preserving_anchor() has already
    // clamped to the new oldest retained host; only the zero local origin
    // is safe. Never resurrect an evicted ID or invent offscreen geometry.
    const double total = heights_->total_height_estimate();
    if (!std::isfinite(base) || !std::isfinite(total) || total < 0.0)
      return Status::QueryRejected;
    const double target = std::clamp(base + delta_y, 0.0, total);
    return show_at_scroll_offset(transcript, target, resolved);
  }

  float leading_spacer_height() const noexcept {
    return leading_spacer_ ? leading_spacer_->height() : 0.0f;
  }

  float trailing_spacer_height() const noexcept {
    return trailing_spacer_ ? trailing_spacer_->height() : 0.0f;
  }

  // A stable host and UiHandle remain stable as long as message_id remains
  // in the visible window, even as source bytes or global window order change.
  flexUI::Element* visible_host(std::uint64_t message_id) const noexcept {
    for (auto* slot : visible_)
      if (slot->message_id == message_id) return slot->host;
    return nullptr;
  }

  // A snapshot has only value identity and a within-message layout offset.
  // The caller captures it BEFORE changing the viewport size or publishing
  // new source. Never keep borrowed UI pointers across a window shift.
  TranscriptScrollAnchor capture_scroll_anchor(
      float tail_tolerance = 2.0f) const noexcept {
    if (visible_.empty() || !viewport_.is_scroll_container() ||
        !std::isfinite(viewport_.scroll_y()) ||
        !std::isfinite(viewport_.max_scroll_y()))
      return {};
    if (!std::isfinite(tail_tolerance) || tail_tolerance < 0.0f)
      tail_tolerance = 2.0f;
    const float scroll_y = viewport_.scroll_y();
    if (viewport_.max_scroll_y() - scroll_y <= tail_tolerance)
      return {0u, 0.0f, true};
    // The user can scroll into an unprojected spacer before the UI owner
    // swaps the two bounded hosts. Do not mislabel that position as an
    // offset inside the first/last VISIBLE host. Locate the retained ID
    // through the bounded value index; its local offset remains estimated
    // until the message is projected and measured by Box.
    if (heights_ && leading_spacer_) {
      const float first_top = visible_.front()->host->y();
      const float last_top = visible_.back()->host->y();
      const float last_height = visible_.back()->host->height();
      if (std::isfinite(first_top) && std::isfinite(last_top) &&
          std::isfinite(last_height) && last_height >= 0.0f &&
          (scroll_y < first_top || scroll_y >= last_top + last_height)) {
        const auto located = heights_->locate_offset(scroll_y);
        if (located && located->message_id &&
            std::isfinite(located->local_y) &&
            located->local_y >= 0.0 && located->local_y <= 10000000.0)
          return {located->message_id,
                  static_cast<float>(located->local_y), false};
      }
    }
    for (const Slot* slot : visible_) {
      const float top = slot->host->y();
      const float height = slot->host->height();
      if (!slot->message_id || !std::isfinite(top) ||
          !std::isfinite(height) || height < 0.0f)
        return {};
      if (scroll_y < top + height)
        return {slot->message_id,
                std::clamp(scroll_y - top, 0.0f, height), false};
    }
    const Slot* last = visible_.back();
    return {last->message_id,
            std::clamp(scroll_y - last->host->y(), 0.0f,
                       std::max(0.0f, last->host->height())), false};
  }

  // Opt-in stable viewport policy. Preserve the same retained message and
  // its local layout offset when an earlier Markdown block grows or reflows.
  // If that ID has been evicted, clamp to the first retained message at 0;
  // only an explicit follow_tail value can switch to the newest messages.
  // Source ownership and admission remain entirely in the C11 transcript.
  Status show_preserving_anchor(const tui_transcript_t* transcript,
                                TranscriptScrollAnchor anchor) {
    if (!transcript) return Status::Invalid;
    if (anchor.follow_tail) return show_tail(transcript);
    if (!anchor.message_id || !std::isfinite(anchor.within_message_y) ||
        anchor.within_message_y < 0.0f)
      return Status::Invalid;
    const std::size_t count = tui_transcript_count(transcript);
    if (!count) return show_with_policy(
        transcript, 0u, ScrollPolicy::AnchorTop);

    std::size_t anchor_index = count;
    std::size_t previous_first_index = count;
    std::uint64_t first_retained_id = 0u;
    std::uint64_t matched_id = 0u;
    const std::uint64_t previous_first_id =
        visible_.empty() ? 0u : visible_.front()->message_id;
    for (std::size_t i = 0u; i < count; ++i) {
      tui_transcript_message_t item{};
      std::size_t written = 0u;
      if (tui_transcript_window(transcript, i, 1u, &item, 1u,
                                &written) != TUI_TRANSCRIPT_OK ||
          written != 1u)
        return Status::QueryRejected;
      if (!i) first_retained_id = item.message_id;
      if (item.message_id == previous_first_id) previous_first_index = i;
      if (anchor_index == count && item.message_id >= anchor.message_id) {
        anchor_index = i;
        matched_id = item.message_id;
      }
    }
    if (anchor_index == count) {
      anchor_index = 0u;
      matched_id = first_retained_id;
    }
    if (matched_id != anchor.message_id) anchor.within_message_y = 0.0f;
    anchor.message_id = matched_id;

    // Keep the previous first visible host when it and the anchor still
    // coexist within the fixed slot budget. Otherwise rebase the bounded
    // window at the retained anchor, without materializing offscreen hosts.
    std::size_t first = anchor_index;
    if (previous_first_index <= anchor_index &&
        anchor_index - previous_first_index < slots_.size())
      first = previous_first_index;
    return show_with_policy(transcript, first, ScrollPolicy::PreserveAnchor,
                            &anchor);
  }

  // Compatibility surface: existing show() retains numeric scroll behavior.
  Status show(const tui_transcript_t* transcript, std::size_t first) {
    return show_with_policy(transcript, first, ScrollPolicy::PreserveIntent);
  }

private:
  enum class ScrollPolicy {
    PreserveIntent, FollowTail, AnchorTop, PreserveAnchor
  };

  bool restore_anchor(const TranscriptScrollAnchor& anchor) {
    if (!viewport_.is_scroll_container()) return false;
    for (const Slot* slot : visible_) {
      if (slot->message_id != anchor.message_id) continue;
      const float top = slot->host->y();
      const float height = slot->host->height();
      if (!std::isfinite(top) || !std::isfinite(height) || height < 0.0f)
        return false;
      return viewport_.set_scroll_offset(viewport_.scroll_x(),
          top + std::clamp(anchor.within_message_y, 0.0f, height));
    }
    return viewport_.set_scroll_offset(viewport_.scroll_x(), 0.0f);
  }

  void record_visible_heights() noexcept {
    if (!heights_) return;
    bool sampled = false;
    for (const Slot* slot : visible_) {
      if (!slot->message_id) continue;
      sampled = heights_->record_height(
          slot->message_id, slot->host->height()) || sampled;
    }
    if (sampled) ++visible_height_sample_passes_;
  }

  // Caller controls which bounded window is visible; this method merely
  // prepares two value-derived placeholder heights. Unknown offscreen rows
  // remain estimates and are never advertised as exact geometry.
  bool prepare_virtual_spacers(std::uint64_t first_id,
                               std::size_t count,
                               bool& changed) {
    changed = false;
    if (!leading_spacer_) return true;
    if (!heights_ || (count != 0u && !first_id)) return false;
    // A new, empty native session must not inherit the previous session's
    // virtual spacer sizes. There are no retained IDs to estimate.
    double lead = 0.0;
    double trail = 0.0;
    if (count != 0u) {
      const auto plan = heights_->window_extent(first_id, count);
      if (!plan || !std::isfinite(plan->leading) ||
          !std::isfinite(plan->trailing) ||
          plan->leading < 0.0 || plan->trailing < 0.0 ||
          plan->leading > 10000000.0 || plan->trailing > 10000000.0)
        return false;
      lead = plan->leading;
      trail = plan->trailing;
    }
    // Stage both values before mutating the live Box. A same-window view
    // still needs a relayout when offscreen height estimates are refined.
    const auto leading = std::to_string(lead) + "px";
    const auto trailing = std::to_string(trail) + "px";
    const auto* old_leading =
        leading_spacer_->custom_property("--tui-virtual-height");
    const auto* old_trailing =
        trailing_spacer_->custom_property("--tui-virtual-height");
    const bool leading_changed = !old_leading || *old_leading != leading;
    const bool trailing_changed = !old_trailing || *old_trailing != trailing;
    changed = leading_changed || trailing_changed;
    // An unchanged Owner turn must not republish identical CSS and dirty
    // the retained virtual spacers. Unknown heights remain estimates.
    if (leading_changed)
      leading_spacer_->set_custom_property("--tui-virtual-height", leading);
    if (trailing_changed)
      trailing_spacer_->set_custom_property("--tui-virtual-height", trailing);
    return true;
  }

  // No source or message-identity rebind is required. Preserve the same
  // width/revision-sensitive height reconciliation, spacer geometry and
  // follow/anchor policy without building temporary source/slot arrays.
  // Native registry bytes are only inspected on this UI Owner turn.
  Status refresh_unchanged_projection(const tui_transcript_t* transcript,
                                      std::size_t count,
                                      ScrollPolicy policy,
                                      const TranscriptScrollAnchor* anchor) {
    bool geometry_changed = false;
    if (heights_) {
      const auto result = heights_->sync(
          transcript, viewport_.width(), layout_revision_);
      if (result != TranscriptHeightIndex::Status::Updated &&
          result != TranscriptHeightIndex::Status::Unchanged)
        return Status::QueryRejected;
      geometry_changed = result == TranscriptHeightIndex::Status::Updated;
    }
    bool spacers_changed = false;
    if (!prepare_virtual_spacers(
            count ? scratch_[0].message_id : 0u,
            count, spacers_changed))
      return Status::QueryRejected;
    // A terminal-state change (including CANCEL) can invalidate native
    // height revisions without changing Markdown bytes or Spacer CSS.
    // Box::update() skips its layout stage unless the tree is dirty, so
    // force actual layout before recording a fresh measured height.
    if (geometry_changed && !spacers_changed)
      viewport_.mark_layout_dirty();
      if (policy == ScrollPolicy::PreserveAnchor && anchor) {
        // Reflow can change host geometry even when the Markdown source did
        // not change (e.g. resize or CSS width change).
        box_.update();
        record_visible_heights();
        const bool moved = restore_anchor(*anchor);
        return moved || geometry_changed || spacers_changed
            ? Status::Updated : Status::Unchanged;
      }
      if (policy == ScrollPolicy::FollowTail) {
        // Offscreen retained history may change spacer geometry without
        // changing a single visible Markdown byte. Reflow must precede
        // reading max_scroll_y(), or a growing leading spacer strands the
        // reader above the actual tail until another producer event.
        if (geometry_changed || spacers_changed) {
          box_.update();
          record_visible_heights();
          (void)viewport_.set_scroll_offset(
              viewport_.scroll_x(), viewport_.max_scroll_y());
          return Status::Updated;
        }
        if (viewport_.set_scroll_offset(
                viewport_.scroll_x(), viewport_.max_scroll_y())) {
          box_.update();
          record_visible_heights();
          return Status::Updated;
        }
      }
      // Native revision/width/style changes can invalidate cached numeric
      // heights even with identical visible Markdown. Re-measure only after
      // an actual owner Box update; never stamp old layout as a new sample.
      if (geometry_changed || spacers_changed) {
        box_.update();
        record_visible_heights();
        return Status::Updated;
      }
      return Status::Unchanged;
  }

  Status show_with_policy(const tui_transcript_t* transcript,
                          std::size_t first, ScrollPolicy policy,
                          const TranscriptScrollAnchor* anchor = nullptr) {
    if (!transcript) return Status::Invalid;

    std::size_t count = 0u;
    if (tui_transcript_window(transcript, first, scratch_.size(),
                              scratch_.data(), scratch_.size(), &count) !=
        TUI_TRANSCRIPT_OK) {
      return Status::QueryRejected;
    }

    // Common wheel/resize case: same retained IDs in the same host order,
    // and identical native GFM bytes. Compare borrowed C11 source only
    // within this UI Owner turn; never store that pointer in a widget.
    // Source generation/state may still have changed, so run the existing
    // height-index/spacer reconciliation and scroll policy below.
    // This skips BOTH malloc-backed staging allocations and source copies.
    if (count == visible_.size()) {
      bool same_source = true;
      for (std::size_t i = 0u; i < count; ++i) {
        if (visible_[i]->message_id != scratch_[i].message_id) {
          same_source = false;
          break;
        }
        std::size_t bytes = 0u;
        const char* source = tui_transcript_text(
            transcript, scratch_[i].message_id, &bytes);
        if (!source) return Status::QueryRejected;
        const auto& cached = visible_[i]->markdown->markdown();
        if (cached.size() != bytes ||
            (bytes && std::memcmp(cached.data(), source, bytes) != 0)) {
          same_source = false;
          break;
        }
      }
      if (same_source)
        return refresh_unchanged_projection(
            transcript, count, policy, anchor);
    }

    // Preflight native source copies and bounded slot identities before
    // changing the Box, any UiHandle, height samples, or Spacer geometry.
    // The two real malloc-backed blocks are checked for nullptr rather
    // than relying on exception propagation through MSVC pmr virtual calls.
    // All partially constructed strings are destroyed on early return.
    struct DesiredScratch {
      StagingAllocation staging;
      Desired* items = nullptr;
      std::size_t constructed = 0u;
      ~DesiredScratch() {
        for (std::size_t i = 0u; i < constructed; ++i)
          items[i].~Desired();
        if (items) {
          if (staging.deallocate) staging.deallocate(staging.context, items);
          else std::free(items);
        }
      }
    } desired{staging_};
    if (count) {
      void* memory = staging_.allocate
          ? staging_.allocate(staging_.context, count * sizeof(Desired))
          : std::malloc(count * sizeof(Desired));
      if (!memory) return Status::ResourceExhausted;
      desired.items = static_cast<Desired*>(memory);
    }
    try {
      for (std::size_t i = 0u; i < count; ++i) {
        const auto id = scratch_[i].message_id;
        std::size_t bytes = 0u;
        const char* text = tui_transcript_text(transcript, id, &bytes);
        if (!text) return Status::QueryRejected;
        new (static_cast<void*>(desired.items + i))
            Desired{id, std::string(text, bytes),
                    std::string("transcript-msg-") + std::to_string(id)};
        ++desired.constructed;
      }
    } catch (const std::bad_alloc&) {
      return Status::ResourceExhausted;
    }

    struct SlotScratch {
      StagingAllocation staging;
      Slot** items = nullptr;
      ~SlotScratch() {
        if (items) {
          if (staging.deallocate) staging.deallocate(staging.context, items);
          else std::free(items);
        }
      }
    } planned{staging_};
    // A one-element local sentinel gives empty projections valid begin/end
    // iterators without requesting any resource allocation.
    Slot* empty_plan = nullptr;
    if (count) {
      void* memory = staging_.allocate
          ? staging_.allocate(staging_.context, count * sizeof(Slot*))
          : std::malloc(count * sizeof(Slot*));
      if (!memory) return Status::ResourceExhausted;
      planned.items = static_cast<Slot**>(memory);
    }
    Slot** next = count ? planned.items : &empty_plan;
    for (std::size_t i = 0u; i < count; ++i) {
      const Desired& item = desired.items[i];
      Slot* existing = nullptr;
      for (auto& slot : slots_)
        if (slot.message_id == item.id) { existing = &slot; break; }
      next[i] = existing;
    }
    for (std::size_t i = 0u; i < count; ++i) {
      if (next[i]) continue;
      for (auto& slot : slots_) {
        if (std::find(next, next + count, &slot) != next + count)
          continue;
        next[i] = &slot;
        break;
      }
      if (!next[i]) return Status::QueryRejected;
    }

    bool different = count != visible_.size();
    for (std::size_t i = 0u; i < count && !different; ++i)
      // Fixed Box slots can be recycled for a different message with
      // BYTE-IDENTICAL GFM. Pointer and Markdown equality alone cannot
      // establish presentation identity: skipping this rebind would leave
      // the old UiHandle/ID live while the height index already locates
      // the newly requested retained message.
      if (next[i] != visible_[i] ||
          next[i]->message_id != desired.items[i].id ||
          next[i]->markdown->markdown() != desired.items[i].text)
        different = true;
    // Anchor semantics are independent of the old viewport's scroll tail.
    // A different first visible message starts at the top; an unchanged
    // anchor retains its numeric scroll position across streaming reflow.
    const bool different_anchor = count != 0u &&
        (visible_.empty() || visible_[0]->message_id != desired.items[0].id);
    // Preallocate the Box child list before the height-index sync or the
    // first Spacer/UiHandle mutation. Even on a global new failure, the
    // previously projected window, styling, scroll and geometry stay intact.
    std::vector<flexUI::Element*> children;
    if (different) {
      try {
        children.reserve(count + (leading_spacer_ ? 2u : 0u));
        if (leading_spacer_) children.push_back(leading_spacer_);
        for (std::size_t i = 0u; i < count; ++i)
          children.push_back(next[i]->host);
        if (trailing_spacer_) children.push_back(trailing_spacer_);
      } catch (const std::bad_alloc&) {
        return Status::ResourceExhausted;
      }
    }

    if (!different)
      return refresh_unchanged_projection(transcript, count, policy, anchor);

    // Reject invalid or over-capacity geometry snapshots before modifying
    // a live widget ID or publishing a new Box topology. Unlike a full DOM,
    // the optional cache stores at most native retainer capacity records.
    if (heights_) {
      const auto result = heights_->sync(
          transcript, viewport_.width(), layout_revision_);
      if (result != TranscriptHeightIndex::Status::Updated &&
          result != TranscriptHeightIndex::Status::Unchanged)
        return Status::QueryRejected;
    }

    bool spacers_changed = false;
    if (!prepare_virtual_spacers(
            count ? desired.items[0].id : 0u, count, spacers_changed))
      return Status::QueryRejected;
    const auto follow = flexUI::StreamScrollFollow::before_content_update(
        viewport_);

    // Invalidate focus and capture on any recycled host *before* replacing
    // its message identity. A widget must not inherit the previous message's
    // interactions, and old generational UiHandles become unresolvable.
    for (auto& slot : slots_) {
      const auto it = std::find(next, next + count, &slot);
      const std::size_t index = static_cast<std::size_t>(it - next);
      const std::uint64_t next_id =
          it == next + count ? 0u : desired.items[index].id;
      if (slot.message_id != next_id) {
        if (has_ancestor(box_.focused_element(), slot.host))
          box_.set_focus(nullptr);
        if (has_ancestor(box_.capturing_element(), slot.host))
          box_.release_mouse_capture(box_.capturing_element());
        slot.host->set_element_id(
            next_id ? desired.items[index].name : std::string{});
        slot.message_id = next_id;
      }
      if (!next_id) {
        slot.markdown->set_markdown("");
        slot.markdown->update(0.0f, *slot.host);
      }
    }

    for (std::size_t i = 0u; i < count; ++i) {
      auto* slot = next[i];
      slot->markdown->set_markdown(desired.items[i].text);
      slot->markdown->update(0.0f, *slot->host);
    }
    viewport_.replace_children(children);
    // Capacity was reserved in the constructor: this only copies bounded
    // Slot* values and cannot allocate on the live mutation path.
    visible_.assign(next, next + count);
    box_.invalidate();
    box_.update();
    record_visible_heights();
    if (policy == ScrollPolicy::FollowTail) {
      (void)viewport_.set_scroll_offset(viewport_.scroll_x(),
                                        viewport_.max_scroll_y());
    } else if (policy == ScrollPolicy::AnchorTop) {
      if (different_anchor)
        (void)viewport_.set_scroll_offset(
            viewport_.scroll_x(), leading_spacer_height());
    } else if (policy == ScrollPolicy::PreserveAnchor && anchor) {
      (void)restore_anchor(*anchor);
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
  // Presentation-only UI-owner allocator; not consulted by native streams.
  StagingAllocation staging_;
  std::vector<Slot> slots_;
  std::vector<Slot*> visible_;
  std::vector<tui_transcript_message_t> scratch_;
  std::unique_ptr<TranscriptHeightIndex> heights_;
  std::uint64_t layout_revision_ = 0u;
  std::size_t in_message_scroll_fast_paths_ = 0u;
  std::size_t visible_height_sample_passes_ = 0u;
  flexUI::Element* leading_spacer_ = nullptr;
  flexUI::Element* trailing_spacer_ = nullptr;
};

}  // namespace flex::tui_backend
#endif  // FLEX_TUI_TRANSCRIPT_WINDOW_H
