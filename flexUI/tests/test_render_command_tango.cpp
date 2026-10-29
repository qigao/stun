#include <tinytest.hpp>
#undef group

#include <backends/tui/init.h>
#include <flexUI/render_command.h>
#include <flexUI/renderer.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kTangoColumns = 12;
constexpr int kTangoRows = 4;
constexpr float kTangoColumnPixels = 8.0f;
constexpr float kTangoRowPixels = 16.0f;

using TangoBuffer = std::unique_ptr<tui_buffer_t, decltype(&tui_buffer_destroy)>;

TangoBuffer tango_test_buffer() {
  return {tui_buffer_create(kTangoColumns, kTangoRows), &tui_buffer_destroy};
}

void tango_test_seed(tui_buffer_t* buffer) {
  tui_buffer_clear_color(buffer, TUI_BLUE);
  tui_buffer_set(buffer, 0, 0,
      tui_cell_styled('S', TUI_YELLOW, TUI_BLUE, TUI_STYLE_BOLD));
  constexpr char wide[] = "\xe4\xb8\xad";
  constexpr char combining[] = "e\xcc\x81";
  check(tui_buffer_set_grapheme(buffer, 2, 1, wide, sizeof(wide) - 1,
                                TUI_GREEN, TUI_MAGENTA, TUI_STYLE_UNDERLINE));
  check(tui_buffer_set_grapheme(buffer, 6, 2, combining, sizeof(combining) - 1,
                                TUI_CYAN, TUI_BLACK, TUI_STYLE_ITALIC));
  const auto* lead = tui_buffer_at_const(buffer, 2, 1);
  const auto* continuation = tui_buffer_at_const(buffer, 3, 1);
  check(lead && lead->ch == 0x4e2d && lead->span == TUI_CELL_SPAN_WIDE);
  check(continuation && continuation->span == TUI_CELL_SPAN_CONTINUATION);
}

struct TangoBufferSnapshot {
  int width;
  int height;
  int dirty_min_x;
  int dirty_min_y;
  int dirty_max_x;
  int dirty_max_y;
  std::vector<tui_cell_t> cells;
};

TangoBufferSnapshot tango_test_snapshot(const tui_buffer_t* buffer) {
  const auto count = static_cast<std::size_t>(buffer->width) *
                     static_cast<std::size_t>(buffer->height);
  return {buffer->width, buffer->height, buffer->dirty_min_x, buffer->dirty_min_y,
          buffer->dirty_max_x, buffer->dirty_max_y,
          std::vector<tui_cell_t>(buffer->cells, buffer->cells + count)};
}

bool tango_test_matches(const tui_buffer_t* buffer, const TangoBufferSnapshot& snapshot) {
  if (buffer->width != snapshot.width || buffer->height != snapshot.height ||
      buffer->dirty_min_x != snapshot.dirty_min_x ||
      buffer->dirty_min_y != snapshot.dirty_min_y ||
      buffer->dirty_max_x != snapshot.dirty_max_x ||
      buffer->dirty_max_y != snapshot.dirty_max_y) return false;
  for (std::size_t index = 0; index != snapshot.cells.size(); ++index) {
    // Compare semantic fields, including colors, styles and grapheme bytes;
    // struct padding is not part of a terminal-cell contract.
    if (!tui_cell_eq(&buffer->cells[index], &snapshot.cells[index])) return false;
  }
  return true;
}

flexUI::RenderViewport tango_test_viewport() {
  return {kTangoColumns * kTangoColumnPixels, kTangoRows * kTangoRowPixels, 1.0f};
}

void tango_test_recovery(flexUI::Renderer& renderer, tui_buffer_t* buffer) {
  flexUI::RenderCommandList normal(renderer.capabilities());
  normal.begin_frame(tango_test_viewport());
  normal.clear(flex::Color::Black);
  normal.draw_text("OK\xe4\xb8\xad", 0, 0, "", 14.0f, false, flex::Color::White);
  normal.end_frame();
  check_nothrow(normal.replay(renderer));
  check(tui_buffer_at_const(buffer, 0, 0)->ch == 'O');
  check(tui_buffer_at_const(buffer, 1, 0)->ch == 'K');
  const auto* lead = tui_buffer_at_const(buffer, 2, 0);
  const auto* continuation = tui_buffer_at_const(buffer, 3, 0);
  check(lead->ch == 0x4e2d && lead->span == TUI_CELL_SPAN_WIDE);
  check(continuation->span == TUI_CELL_SPAN_CONTINUATION);
}

} // namespace

// These tests use the production Tango renderer and cell storage. They do not
// create a terminal session or make a GPU/PTY acceptance claim.
spec("Render command rejection preserves real Tango cells") {
  it("rejects late active effects before immediate or retained replay changes the target") {
    for (bool retained : {false, true}) {
      for (bool shadow : {false, true}) {
        auto buffer = tango_test_buffer();
        check(buffer != nullptr);
        if (!buffer) return;
        tango_test_seed(buffer.get());
        auto backend = flex::tui_backend::create_buffer_renderer(buffer.get());
        check(backend != nullptr);
        if (!backend) return;
        flexUI::Renderer renderer(backend.get());
        check(!renderer.capabilities().shadow && !renderer.capabilities().blur);
        const auto snapshot = tango_test_snapshot(buffer.get());
        const auto prior_viewport = backend->viewport();

        flexUI::RenderCommandList candidate(renderer.capabilities());
        candidate.begin_frame(tango_test_viewport());
        candidate.clear(flex::Color::Red);
        candidate.draw_rect(0, 0, 32, 16, 0,
            flex::Paint::solid(flex::Color::White), flex::Paint::none(), 0);
        if (shadow) candidate.set_shadow(flex::Shadow::drop(1, 2, 3, flex::Color::Black));
        else candidate.set_blur(flex::BlurFilter{4});
        candidate.end_frame();
        const auto result = candidate.validate_replay(
            renderer, flexUI::RenderValidationScope::CompleteFrame);
        check(result.code == flexUI::RenderValidationError::UnsupportedOperation);
        check(result.command_index == 3);
        check(!result);
        flexUI::RenderCommandCache cache;
        if (retained) {
          check_throws_as(candidate.replay_retained(renderer, cache),
                          flexUI::RenderValidationException);
        } else {
          check_throws_as(candidate.replay(renderer), flexUI::RenderValidationException);
        }
        check(cache.empty());
        check(tango_test_matches(buffer.get(), snapshot));
        check(backend->viewport().width == prior_viewport.width);
        check(backend->viewport().height == prior_viewport.height);
        tango_test_recovery(renderer, buffer.get());
      }
    }
  }

  it("rejects invalid raw effect values before a frame clears seeded cells") {
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(), -1.0f}) {
      auto buffer = tango_test_buffer();
      check(buffer != nullptr);
      if (!buffer) return;
      tango_test_seed(buffer.get());
      auto backend = flex::tui_backend::create_buffer_renderer(buffer.get());
      check(backend != nullptr);
      if (!backend) return;
      flexUI::Renderer renderer(backend.get());
      const auto snapshot = tango_test_snapshot(buffer.get());
      flexUI::RenderCommandList candidate(renderer.capabilities());
      candidate.begin_frame(tango_test_viewport());
      candidate.clear(flex::Color::Red);
      candidate.append(std::vector<flexUI::RenderCommand>{
          flexUI::SetBlurCommand{flex::BlurFilter{invalid}}});
      candidate.end_frame();
      const auto result = candidate.validate_replay(renderer);
      check(result.code == flexUI::RenderValidationError::InvalidValue);
      check(result.command_index == 2);
      check_throws_as(candidate.replay(renderer), flexUI::RenderValidationException);
      check(tango_test_matches(buffer.get(), snapshot));
      tango_test_recovery(renderer, buffer.get());
    }
  }

  it("makes direct production Tango effect calls fail instead of silently succeeding") {
    auto buffer = tango_test_buffer();
    check(buffer != nullptr);
    if (!buffer) return;
    tango_test_seed(buffer.get());
    auto backend = flex::tui_backend::create_buffer_renderer(buffer.get());
    check(backend != nullptr);
    if (!backend) return;
    const auto snapshot = tango_test_snapshot(buffer.get());
    check_throws_as(backend->set_blur(flex::BlurFilter{4}), std::logic_error);
    check_throws_as(backend->set_shadow(flex::Shadow::drop(1, 2, 3, flex::Color::Black)),
                    std::logic_error);
    check_throws_as(backend->set_shadow(flex::Shadow::inner(1, 2, 3, flex::Color::Black)),
                    std::logic_error);
    check(tango_test_matches(buffer.get(), snapshot));
    flexUI::Renderer renderer(backend.get());
    tango_test_recovery(renderer, buffer.get());
  }

  it("rejects every nonfinite direct shadow field and negative blur before no-effect shortcuts") {
    auto buffer = tango_test_buffer();
    check(buffer != nullptr);
    if (!buffer) return;
    tango_test_seed(buffer.get());
    auto backend = flex::tui_backend::create_buffer_renderer(buffer.get());
    check(backend != nullptr);
    if (!backend) return;
    const auto snapshot = tango_test_snapshot(buffer.get());
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity()}) {
      check_throws_as(backend->set_blur(flex::BlurFilter{invalid}), std::invalid_argument);
      for (const auto member : {&flex::Shadow::offset_x, &flex::Shadow::offset_y,
                                &flex::Shadow::blur, &flex::Shadow::spread}) {
        auto shadow = flex::Shadow::drop(0, 0, 0, flex::Color::Transparent);
        shadow.*member = invalid;
        check_throws_as(backend->set_shadow(shadow), std::invalid_argument);
      }
      for (const auto member : {&flex::Color::r, &flex::Color::g,
                                &flex::Color::b, &flex::Color::a}) {
        flex::Shadow shadow;
        shadow.color.*member = invalid;
        check_throws_as(backend->set_shadow(shadow), std::invalid_argument);
      }
    }
    check_throws_as(backend->set_blur(flex::BlurFilter{-1}), std::invalid_argument);
    check_throws_as(backend->set_shadow(flex::Shadow::drop(0, 0, -1, flex::Color::Transparent)),
                    std::invalid_argument);
    check(tango_test_matches(buffer.get(), snapshot));
  }

  it("preserves identity and transparent effect calls and then renders ordinary cells") {
    auto buffer = tango_test_buffer();
    check(buffer != nullptr);
    if (!buffer) return;
    tango_test_seed(buffer.get());
    auto backend = flex::tui_backend::create_buffer_renderer(buffer.get());
    check(backend != nullptr);
    if (!backend) return;
    const auto snapshot = tango_test_snapshot(buffer.get());
    auto transparent = flex::Shadow::drop(-3, -4, 5, flex::Color::Transparent);
    transparent.spread = -2;
    check_nothrow(backend->set_shadow(flex::Shadow{}));
    check_nothrow(backend->set_shadow(transparent));
    check_nothrow(backend->set_blur(flex::BlurFilter{0}));
    check_nothrow(backend->clear_shadow());
    check_nothrow(backend->clear_blur());
    check(tango_test_matches(buffer.get(), snapshot));
    flexUI::Renderer renderer(backend.get());
    tango_test_recovery(renderer, buffer.get());
  }
}
