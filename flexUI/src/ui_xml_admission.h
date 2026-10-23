#pragma once

#include <flexUI/ui_document.h>
#include <cstddef>
#include <string_view>

namespace flexUI::detail {

UiDocumentError validate_ui_xml_utf8(std::string_view source);

inline SourceSpan xml_source_span(std::string_view source, std::ptrdiff_t offset) {
  SourceSpan span{1, 1, 0};
  if (offset <= 0) {
    return span;
  }
  const auto limit = static_cast<std::size_t>(offset) < source.size()
                         ? static_cast<std::size_t>(offset)
                         : source.size();
  for (std::size_t index = 0; index < limit; ++index) {
    if (source[index] == '\n') {
      ++span.line;
      span.column = 1;
    } else {
      ++span.column;
    }
  }
  return span;
}

} // namespace flexUI::detail
