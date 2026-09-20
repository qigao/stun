#include <flexUI/text_util.h>

#include <tinytest.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace flexUI;

spec("FlexUI strict Unicode scalar scanning") {
  it("preserves scalar values and original UTF-8 byte ranges") {
    std::string text = u8"Aé😀";
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

    const auto final_ascii = utf8_next_scalar(text, cursor);
    check_equal(final_ascii.value, std::uint32_t{'B'});
    check_equal(cursor, text.size());
    check_equal(utf8_scalar_count(text), std::size_t{5});
  }

  it("rejects malformed UTF-8 without advancing the byte cursor") {
    const std::string overlong("\xC0\xAF", 2);
    size_t cursor = 0;
    check_throws_as(utf8_next_scalar(overlong, cursor), std::invalid_argument);
    check_equal(cursor, std::size_t{0});

    const std::string truncated("\xF0\x9F", 2);
    check_throws_as(utf8_next_scalar(truncated, cursor), std::invalid_argument);
    check_equal(cursor, std::size_t{0});

    const std::string bad_continuation("\xE2\x28\xA1", 3);
    check_throws_as(utf8_scalar_count(bad_continuation), std::invalid_argument);
  }

  it("rejects an end cursor instead of inventing a replacement scalar") {
    const std::string text = "A";
    size_t cursor = text.size();
    check_throws_as(utf8_next_scalar(text, cursor), std::out_of_range);
    check_equal(cursor, text.size());
  }

  it("propagates strict UTF-8 failure through emoji segmentation") {
    const std::string invalid("\xF0\x28\x8C\x28", 4);
    check_throws_as(segment_text(invalid), std::invalid_argument);
    check_throws_as(has_emoji(invalid), std::invalid_argument);
  }
};
