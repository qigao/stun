#include <flexUI/text_layout.h>
#include <tinytest.h>

#include <string>

using namespace flexUI;

spec("FlexUI consumes Salts Unicode text boundaries") {
  it("truncates only at extended grapheme boundaries") {
    ComputedStyle style;
    style.font_size = 10.0f;
    style.variables[Symbol("--white-space")] = "nowrap";
    style.variables[Symbol("--text-overflow")] = "ellipsis";

    const std::string combining = std::string("e") + "\xCC\x81";
    const std::string text = combining + "X";
    const float width =
        approximate_text_width(&style, combining + "...") + 0.01f;

    const auto block =
        layout_text_block(&style, text, 0.0f, 0.0f, width, 20.0f, Color{});
    check_equal(block.lines.size(), std::size_t{1});
    check_equal(block.lines.front().text, combining + "...");

    const std::string woman_technologist =
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB";
    const std::string emoji_text = woman_technologist + "X";
    const float emoji_width =
        approximate_text_width(&style, woman_technologist + "...") + 0.01f;

    const auto emoji_block = layout_text_block(
        &style, emoji_text, 0.0f, 0.0f, emoji_width, 20.0f, Color{});
    check_equal(emoji_block.lines.size(), std::size_t{1});
    check_equal(emoji_block.lines.front().text, woman_technologist + "...");
  }

  it("uses Unicode P2 P3 paragraph direction for plaintext") {
    ComputedStyle style;
    style.direction = Direction::Rtl;
    style.unicode_bidi = UnicodeBidi::Plaintext;

    check_equal(resolve_text_direction_for_content(&style, "ABC"),
                Direction::Ltr);
    check_equal(resolve_text_direction_for_content(
                    &style, "\xD7\x90"),  // Hebrew ALEF
                Direction::Rtl);

    // U+0590 has no explicit DerivedBidiClass record. Unicode 17 assigns
    // it R through the Hebrew @missing range.
    check_equal(resolve_text_direction_for_content(
                    &style, "\xD6\x90"),
                Direction::Rtl);

    // Strong text inside an isolate must not determine the outer paragraph.
    const std::string isolated_rtl =
        "\xE2\x81\xA7"  // RLI
        "\xD7\x90"      // Hebrew ALEF
        "\xE2\x81\xA9" // PDI
        " A";
    check_equal(resolve_text_direction_for_content(&style, isolated_rtl),
                Direction::Ltr);

    // P3 defaults a paragraph with no strong type to level 0 / LTR.
    check_equal(resolve_text_direction_for_content(&style, "123 ()"),
                Direction::Ltr);
  }
}
