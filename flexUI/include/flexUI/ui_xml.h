#pragma once

#include <flexUI/ui_document.h>

#include <string_view>

namespace flexUI {

/// Parses one `<ui>` XML document into a parser-independent definition.
///
/// The document must contain a non-empty `name`, exactly one widget root and
/// at most one optional `<resources>` section. Widget elements require an `id`.
/// Event, binding and raw attributes use `on:`, `bind:` and `attr:` prefixes.
UiDocumentParseResult parse_ui_xml(std::string_view source, const UiDocumentLimits &limits = {});

/// Parses and semantically lowers one XML UI document into an immutable program.
UiDocumentCompileResult compile_ui_xml(std::string_view source,
                                       const UiDocumentLimits &limits = {});

} // namespace flexUI
