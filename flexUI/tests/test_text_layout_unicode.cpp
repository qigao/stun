#include <tinytest.hpp>
#include "test_support.h"
#include <flexUI/text_layout.h>
#include <flexUI/render_command.h>
#include <stdexcept>
#include <variant>
using namespace flexUI;

spec("Text fallback measurement uses Unicode grapheme spacing units") {
  it("counts combining sequences as one letter-spacing unit") {
    ComputedStyle base;
    base.font_size = 10.0f;

    ComputedStyle spaced = base;
    spaced.letter_spacing = 4.0f;

    const std::string text = std::string("e") + "\xCC\x81" + "X";
    const float natural = approximate_text_width(&base, text);
    const float with_spacing = approximate_text_width(&spaced, text);

    check(approx_eq(with_spacing - natural, 4.0f, 0.001f));
  }

  it("does not scale non-ASCII fallback width by UTF-8 byte length") {
    ComputedStyle style;
    style.font_size = 10.0f;

    const float ascii = approximate_text_width(&style, "A");
    const float cjk =
        approximate_text_width(&style, "\xE4\xB8\xAD");
    const float combining =
        approximate_text_width(&style, std::string("e") + "\xCC\x81");

    check(approx_eq(cjk, ascii, 0.001f));
    check(approx_eq(combining, ascii, 0.001f));
  }

  it("measures an emoji ZWJ sequence as one grapheme fallback unit") {
    ComputedStyle style;
    style.font_size = 10.0f;
    const std::string woman_technologist =
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB";

    check(approx_eq(
        approximate_segmented_text_width(&style, woman_technologist),
        10.0f, 0.001f));
  }

  it("applies word spacing to Unicode White_Space graphemes") {
    ComputedStyle base;
    base.font_size = 10.0f;

    ComputedStyle spaced = base;
    spaced.word_spacing = 8.0f;

    const std::string text = std::string("A") + "\xC2\xA0" + "B";
    const float natural = approximate_text_width(&base, text);
    const float with_spacing = approximate_text_width(&spaced, text);

    check(approx_eq(with_spacing - natural, 8.0f, 0.001f));
  }
}

spec("Unicode segmented text command spacing") {
  it("advances past Unicode whitespace without emitting its bytes as a separate glyph") {
    ComputedStyle style;
    style.font_size = 10.0f;
    style.word_spacing = 8.0f;
    const std::string text = std::string("A") + "\xC2\xA0" + "B";
    RenderCommandList commands(flex::RendererCapabilities{});
    const float advance = emit_segmented_text_line(
        commands, &style, text, 2.0f, 20.0f, Color{0, 0, 0, 1}, false);
    check_within(advance, approximate_segmented_text_width(&style, text), 0.001f);
    check_equal(commands.commands().size(), std::size_t{2});
    const auto* first = std::get_if<DrawTextCommand>(&commands.commands()[0]);
    const auto* second = std::get_if<DrawTextCommand>(&commands.commands()[1]);
    check_not_null(first);
    check_not_null(second);
    if (!first || !second) return;
    check_equal(first->text, "A");
    check_equal(second->text, "B");
    ComputedStyle base = style;
    base.word_spacing = 0.0f;
    const float expected_x = 2.0f + approximate_text_width(&base, "A") +
        approximate_text_width(&base, "\xC2\xA0") + 8.0f;
    check_within(second->x, expected_x, 0.001f);
  }

  it("rejects malformed suffixes in both approximate width entry points") {
    ComputedStyle style;
    const std::string text = std::string("A") + "\xED\xA0\x80";
    check_throws_as(approximate_text_width(&style, text), std::invalid_argument);
    check_throws_as(approximate_segmented_text_width(&style, text), std::invalid_argument);
  }
}
