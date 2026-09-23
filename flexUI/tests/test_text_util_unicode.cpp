#include <flexUI/text_util.h>

#include <tinytest.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using namespace flexUI;

spec("FlexUI strict Unicode scalar scanning") {
  it("preserves scalar values and original UTF-8 byte ranges") {
    std::string text = std::string("A") + "\xC3\xA9" + "\xF0\x9F\x98\x80";
    text.push_back('\0');
    text.push_back('B');
    size_t cursor = 0;

    const auto ascii = utf8_next_scalar(text, cursor);
    check_equal(ascii.value, std::uint32_t{'A'});
    check_equal(ascii.byte_offset, std::size_t{0});
    check_equal(ascii.byte_length, std::size_t{1});
    check_equal(cursor, std::size_t{1});

    const auto latin = utf8_next_scalar(text, cursor);
    check_equal(latin.value, std::uint32_t{0x00E9});
    check_equal(latin.byte_offset, std::size_t{1});
    check_equal(latin.byte_length, std::size_t{2});
    check_equal(cursor, std::size_t{3});

    const auto emoji = utf8_next_scalar(text, cursor);
    check_equal(emoji.value, std::uint32_t{0x1F600});
    check_equal(emoji.byte_offset, std::size_t{3});
    check_equal(emoji.byte_length, std::size_t{4});
    check_equal(cursor, std::size_t{7});

    const auto nul = utf8_next_scalar(text, cursor);
    check_equal(nul.value, std::uint32_t{0});
    check_equal(nul.byte_offset, std::size_t{7});
    check_equal(nul.byte_length, std::size_t{1});
    check_equal(cursor, std::size_t{8});

    const auto final_ascii = utf8_next_scalar(text, cursor);
    check_equal(final_ascii.value, std::uint32_t{'B'});
    check_equal(cursor, text.size());
    check_equal(utf8_scalar_count(text), std::size_t{5});
  }

  it("preserves combining sequences and CJK without implicit normalization") {
    const std::string text = std::string("e") + "\xCC\x81" + "\xE4\xB8\xAD";
    size_t cursor = 0;
    check_equal(utf8_next_scalar(text, cursor).value, std::uint32_t{'e'});
    const auto combining = utf8_next_scalar(text, cursor);
    check_equal(combining.value, std::uint32_t{0x0301});
    check_equal(combining.byte_offset, std::size_t{1});
    check_equal(combining.byte_length, std::size_t{2});
    check_equal(utf8_next_scalar(text, cursor).value, std::uint32_t{0x4E2D});
    check_equal(cursor, text.size());
    check_equal(utf8_scalar_count(text), std::size_t{3});
    std::string joined;
    for (const auto &segment : segment_text(text)) {
      joined += segment.text;
    }
    check_equal(joined, text);
  }

  it("accepts valid scalar encoding boundaries without altering bytes") {
    struct Case {
      const char *bytes;
      std::size_t length;
      std::uint32_t value;
    };
    const Case cases[] = {
        {"\x7F", 1, 0x7F}, {"\xC2\x80", 2, 0x80},
        {"\xDF\xBF", 2, 0x7FF}, {"\xE0\xA0\x80", 3, 0x800},
        {"\xED\x9F\xBF", 3, 0xD7FF}, {"\xEE\x80\x80", 3, 0xE000},
        {"\xEF\xBF\xBF", 3, 0xFFFF}, {"\xF0\x90\x80\x80", 4, 0x10000},
        {"\xF4\x8F\xBF\xBF", 4, 0x10FFFF}};
    for (const auto &item : cases) {
      const std::string text(item.bytes, item.length);
      size_t cursor = 0;
      const auto scalar = utf8_next_scalar(text, cursor);
      check_equal(scalar.value, item.value);
      check_equal(scalar.byte_offset, std::size_t{0});
      check_equal(scalar.byte_length, item.length);
      check_equal(cursor, item.length);
      check_equal(utf8_scalar_count(text), std::size_t{1});
    }
  }

  it("rejects malformed UTF-8 without advancing the byte cursor") {
    const std::string malformed[] = {
        std::string("\xC0\xAF", 2), std::string("\xE0\x80\xAF", 3),
        std::string("\xF0\x80\x80\xAF", 4), std::string("\xED\xA0\x80", 3),
        std::string("\xED\xBF\xBF", 3), std::string("\xF4\x90\x80\x80", 4),
        std::string("\x80", 1), std::string("\xBF", 1),
        std::string("\xF5\x80\x80\x80", 4), std::string("\xFF", 1),
        std::string("\xC2", 1), std::string("\xE2\x82", 2),
        std::string("\xF0\x9F\x98", 3), std::string("\xE2\x28\xA1", 3),
        std::string("\xF0\x28\x8C\x28", 4)};
    for (const auto &text : malformed) {
      size_t cursor = 0;
      check_throws_as(utf8_next_scalar(text, cursor), std::invalid_argument);
      check_equal(cursor, std::size_t{0});
      check_throws_as(utf8_scalar_count(text), std::invalid_argument);
      check_throws_as(segment_text(text), std::invalid_argument);
    }
  }

  it("keeps cursor and assigned output unchanged after a valid prefix including NUL") {
    const std::string text = std::string("A\0", 2) + "\xED\xA0\x80";
    size_t cursor = 2;
    Utf8Scalar output{0x1234, 42, 43};
    check_throws_as(output = utf8_next_scalar(text, cursor), std::invalid_argument);
    check_equal(cursor, std::size_t{2});
    check_equal(output.value, std::uint32_t{0x1234});
    check_equal(output.byte_offset, std::size_t{42});
    check_equal(output.byte_length, std::size_t{43});
    check_throws_as(utf8_scalar_count(text), std::invalid_argument);
  }

  it("rejects end and out-of-range cursors without inventing a scalar") {
    const std::string text = "A";
    for (const std::size_t initial :
         {text.size(), text.size() + 1, std::numeric_limits<std::size_t>::max()}) {
      size_t cursor = initial;
      check_throws_as(utf8_next_scalar(text, cursor), std::out_of_range);
      check_equal(cursor, initial);
    }
    size_t cursor = 0;
    check_throws_as(utf8_next_scalar(std::string{}, cursor), std::out_of_range);
    check_equal(cursor, std::size_t{0});
    check_equal(utf8_scalar_count(std::string{}), std::size_t{0});
  }

  it("rejects a cursor inside a multibyte scalar transactionally") {
    const std::string text("\xF0\x9F\x98\x80", 4);
    for (size_t initial = 1; initial < text.size(); ++initial) {
      size_t cursor = initial;
      check_throws_as(utf8_next_scalar(text, cursor), std::invalid_argument);
      check_equal(cursor, initial);
    }
  }

  it("accepts an explicitly encoded replacement character as valid user text") {
    const std::string text("\xEF\xBF\xBD", 3);
    size_t cursor = 0;
    check_equal(utf8_next_scalar(text, cursor).value, std::uint32_t{0xFFFD});
    check_equal(cursor, text.size());
    check_equal(utf8_scalar_count(text), std::size_t{1});
  }


  it("uses Unicode 17 emoji properties without treating every Emoji scalar as presentation") {
    check_true(is_emoji(0x1F600u));
    check_true(is_emoji(static_cast<uint32_t>('1')));
    check_false(is_emoji(static_cast<uint32_t>('A')));
    check_true(is_emoji_modifier(0x1F3FBu));
    check_true(is_emoji_modifier(0xFE0Fu));
    check_true(is_emoji_modifier(0x200Du));

    const auto plain_digit = segment_text("1");
    check_equal(plain_digit.size(), std::size_t{1});
    check(plain_digit.front().type == TextSegmentType::Regular);

    const std::string keycap = std::string("1") + "\xEF\xB8\x8F\xE2\x83\xA3";
    const auto keycap_segments = segment_text(keycap);
    check_equal(keycap_segments.size(), std::size_t{1});
    check(keycap_segments.front().type == TextSegmentType::Emoji);
    check_equal(keycap_segments.front().text, keycap);
  }

  it("classifies complete grapheme clusters for emoji font policy") {
    const std::string woman_technologist =
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB";
    const std::string flag_us =
        "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
    const std::string heart_emoji =
        "\xE2\x9D\xA4\xEF\xB8\x8F";
    const std::string heart_text =
        "\xE2\x9D\xA4\xEF\xB8\x8E";

    for (const auto& emoji_cluster :
         {woman_technologist, flag_us, heart_emoji}) {
      const auto segments = segment_text(emoji_cluster);
      check_equal(segments.size(), std::size_t{1});
      check(segments.front().type == TextSegmentType::Emoji);
      check_equal(segments.front().text, emoji_cluster);
      check_true(has_emoji(emoji_cluster));
    }

    const auto text_segments = segment_text(heart_text);
    check_equal(text_segments.size(), std::size_t{1});
    check(text_segments.front().type == TextSegmentType::Regular);
    check_false(has_emoji(heart_text));
  }

  it("coalesces adjacent grapheme clusters with the same rendering policy") {
    const std::string grin = "\xF0\x9F\x98\x80";
    const std::string wave = "\xF0\x9F\x91\x8B";
    const std::string input = std::string("AB") + grin + wave + "CD";
    const auto segments = segment_text(input);

    check_equal(segments.size(), std::size_t{3});
    check(segments[0].type == TextSegmentType::Regular);
    check_equal(segments[0].text, "AB");
    check(segments[1].type == TextSegmentType::Emoji);
    check_equal(segments[1].text, grin + wave);
    check(segments[2].type == TextSegmentType::Regular);
    check_equal(segments[2].text, "CD");
  }

  it("validates malformed suffixes even after finding an emoji") {
    const std::string emoji("\xF0\x9F\x98\x80", 4);
    const std::string text = emoji + std::string("\0", 1) + "\xC0\xAF";
    check_throws_as(has_emoji(text), std::invalid_argument);
    check_throws_as(segment_text(text), std::invalid_argument);
  }

  it("preserves valid predicate and segmentation results across embedded NUL") {
    const std::string emoji("\xF0\x9F\x98\x80", 4);
    const std::string text = std::string("A\0", 2) + emoji + "B";
    check_false(has_emoji(std::string{}));
    check_false(has_emoji(std::string("A\0B", 3)));
    check_true(has_emoji(text));
    std::string joined;
    for (const auto &segment : segment_text(text)) {
      joined += segment.text;
    }
    check_equal(joined, text);
  }
}
