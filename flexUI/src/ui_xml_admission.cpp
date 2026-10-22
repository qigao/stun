#include "ui_xml_admission.h"
#include <salts_unicode.h>

namespace flexUI::detail {

UiDocumentError validate_ui_xml_utf8(std::string_view source) {
  const vstr input = vstr_from_buf(source.data(), source.size());
  size_t cursor = 0;
  salts_unicode_scalar scalar{};
  while (cursor < source.size()) {
    const auto status = salts_unicode_utf8_next(input, &cursor, &scalar);
    if (status == SALTS_UNICODE_OK) continue;
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
      const auto location = xml_source_span(source, static_cast<std::ptrdiff_t>(cursor));
      return {UiDocumentErrorCode::InvalidUtf8, "UI XML source contains invalid UTF-8",
              location.line, location.column};
    }
    return {UiDocumentErrorCode::InvalidUtf8, "UI XML source could not be validated as UTF-8"};
  }
  return {};
}

} // namespace flexUI::detail
