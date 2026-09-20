#include <flexUI/text_util.h>

#include <tinytest.hpp>

#include <stdexcept>
#include <string>

using namespace flexUI;

spec("FlexUI Salts Unicode bridge") {
  it("decodes ASCII BMP non-BMP and combining scalars with byte-accurate lengths") {
    const std::string text = std::string("A") + "Ã©" + "ð" + "eÌ";
    size_t pos = 0;

    check_equal(utf8_decode(text, pos), uint32_t{'A'});
    check_equal(pos, size_t{1});

    check_equal(utf8_char_length(text, pos), size_t{2});
    check_equal(utf8_decode(text, pos), uint32_t{0x00E9});
    check_equal(pos, size_t{3});

    check_equal(utf8_char_length(text, pos), size_t{4});
    check_equal(utf8_decode(text, pos), uint32_t{0x1F600});
    check_equal(pos, size_t{7});

    check_equal(utf8_decode(text, pos), uint32_t{'e'});
    check_equal(utf8_decode(text, pos), uint32_t{0x0301});
    check_equal(pos, text.size());
  }

  it("preserves embedded NUL as a scalar instead of treating it as end of input") {
    const std::string text("A\0B", 3);
    size_t pos = 1;
    check_equal(utf8_decode(text, pos), uint32_t{0});
    check_equal(pos, size_t{2});
    check_equal(utf8_decode(text, pos), uint32_t{'B'});
    check_equal(pos, size_t{3});
  }

  it("rejects malformed UTF-8 without advancing the caller cursor") {
    const std::string malformed("\xF0\x28\x8C\x28", 4);
    size_t pos = 0;
    bool threw = false;
    try {
      static_cast<void>(utf8_decode(malformed, pos));
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    check_true(threw);
    check_equal(pos, size_t{0});
  }

  it("rejects truncated and overlong UTF-8 without replacement fallback") {
    for (const std::string malformed :
         {std::string("\xE2\x82", 2), std::string("\xC0\xAF", 2)}) {
      size_t pos = 0;
      bool threw = false;
      try {
        static_cast<void>(utf8_decode(malformed, pos));
      } catch (const std::invalid_argument &) {
        threw = true;
      }
      check_true(threw);
      check_equal(pos, size_t{0});
    }
  }

  it("propagates strict UTF-8 rejection through text segmentation") {
    const std::string malformed("\xED\xA0\x80", 3);
    bool threw = false;
    try {
      static_cast<void>(segment_text(malformed));
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    check_true(threw);
  }
}
