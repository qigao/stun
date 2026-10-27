#include <charts/ui/dotgraph_frame.h>

#include <flexUI/ui_xml.h>
#include "dotgraph_frame_xml.inc"

#include <stdexcept>

namespace charts::ui {

flexUI::UiDocumentDefinition dotgraph_frame_definition() {
  static const auto compiled = [] {
    auto result = flexUI::compile_ui_xml(kDotGraphFrameXml);
    if (!result) {
      throw std::logic_error("invalid native DotGraph XML root: " +
                             result.error.message);
    }
    return result.program;
  }();
  return compiled.program->definition();
}

}  // namespace charts::ui
