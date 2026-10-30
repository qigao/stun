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

/// Typed XML admission through the SAME parser and the caller's immutable
/// WidgetRegistry descriptor. String Props remain strings even when they look
/// numeric; unknown or invalid declared Props fail before a Widget is built.
UiDocumentParseResult parse_ui_xml(std::string_view source,
                                   const WidgetRegistry &registry,
                                   const UiDocumentLimits &limits);

/// Parses and semantically lowers one XML UI document into an immutable program.
UiDocumentCompileResult compile_ui_xml(std::string_view source,
                                       const UiDocumentLimits &limits = {});

/// Compile via the one XML compiler and preflight typed Registry schemas
/// without calling any factory, mounting a Box, or publishing a partial UI.
/// The registry-aware source may also declare one local <components> section:
/// <component name="Panel"><interface><prop name="title" type="string"
/// required="true"/><slot name="actions"/></interface>
/// <template><div><label text="$props.title"/><slot name="actions"/>
/// </div></template></component>
/// Invocations <Panel id="x" title="T"><fill name="actions"><button/>
/// </fill></Panel> become a structural host and a statically scoped
/// component using the SAME compile_scoped_ui_components pipeline.
/// M2b: no external imports, nested calls in fills, dynamic props, optional
/// slots or multiple-root fills. The untyped overload is unchanged.
UiDocumentCompileResult compile_ui_xml(std::string_view source,
                                       const WidgetRegistry &registry,
                                       const UiDocumentLimits &limits);

} // namespace flexUI
