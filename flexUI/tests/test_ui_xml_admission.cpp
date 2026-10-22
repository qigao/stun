#include "../src/ui_xml_admission.h"
#include <tinytest.hpp>
#include <array>
#include <string>

using flexUI::detail::validate_ui_xml_utf8;
using flexUI::UiDocumentErrorCode;

suite("Production XML UTF-8 admission") {
  it("accepts complete Unicode input without normalizing bytes") {
    const std::string text = "<ui name=\"\xE4\xB8\xAD\xF0\x9F\x98\x80\">e\xCC\x81</ui>";
    const auto saved = text;
    check_false(static_cast<bool>(validate_ui_xml_utf8(text)));
    check_equal(text, saved);
    check_false(static_cast<bool>(validate_ui_xml_utf8({})));
  }
  it("rejects invalid scalars at their first source byte") {
    const std::array<std::string, 8> invalid = {
      std::string("\x80", 1), std::string("\xC0\xAF", 2),
      std::string("\xE0\x80\xAF", 3), std::string("\xED\xA0\x80", 3),
      std::string("\xF4\x90\x80\x80", 4), std::string("\xF0\x9F", 2),
      std::string("\xE2\x28\xA1", 3), std::string("\xFF", 1)};
    for (const auto& bytes : invalid) {
      const auto error = validate_ui_xml_utf8("<ui>\n  " + bytes);
      check_true(error.code == UiDocumentErrorCode::InvalidUtf8);
      check_equal(error.line, 2);
      check_equal(error.column, 3);
    }
  }
  it("checks a malformed suffix after an otherwise complete document") {
    const auto error = validate_ui_xml_utf8("<ui/>\n\xFF");
    check_true(error.code == UiDocumentErrorCode::InvalidUtf8);
    check_equal(error.line, 2);
    check_equal(error.column, 1);
  }
  it("keeps byte columns and CRLF source positions deterministic") {
    const auto error = validate_ui_xml_utf8("\xE4\xB8\xAD\r\n  \xFF");
    check_true(error.code == UiDocumentErrorCode::InvalidUtf8);
    check_equal(error.line, 2);
    check_equal(error.column, 3);
    const auto same_line = validate_ui_xml_utf8("\xE4\xB8\xAD\xFF");
    check_equal(same_line.column, 4);
  }
  it("leaves embedded NUL rejection to the existing XML structure boundary") {
    check_false(static_cast<bool>(validate_ui_xml_utf8(std::string("a\0b", 3))));
  }
}
