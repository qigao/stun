#pragma once

#include <flexUI/ui_document.h>

namespace charts::ui {

/// Copies the immutable native XML root definition; the DOT domain adapter
/// appends individually keyed semantic cluster/edge/node Elements before
/// the single UiDocument detached mount. No BytesVM or second runtime tree.
flexUI::UiDocumentDefinition dotgraph_frame_definition();

}  // namespace charts::ui
