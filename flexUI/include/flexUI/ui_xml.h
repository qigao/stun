#pragma once

#include <flexUI/ui_document.h>

#include <string>
#include <string_view>
#include <vector>

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


/**
 * Immutable, host-admitted module source bytes. The caller owns the storage
 * for the duration of the synchronous compilation. This is NOT a loader or
 * file-system/network capability: the app's explicit <import> must name a
 * matching relative source and verified SHA-256 before its declarations can
 * enter the one WidgetRegistry / CompiledUiProgram pipeline.
 */
struct UiXmlModuleSource {
  std::string source;
  std::string xml;
};

/**
 * M2c: compile explicit, SHA-256-pinned <imports> from a bounded immutable
 * source catalog. Each module source is <module namespace="urn:..." version="1">
 * <components>...</components></module>; only its native component definitions
 * are admitted. Imported tags are bound by <import prefix="studio"
 * namespace="urn:studio:ui" source="components/editor.ui.xml"
 * version="1" sha256="lowercase64hex"/> and a matching ui xmlns:studio.
 *
 * All parsing uses the existing XML adapter and static scoped component
 * compiler. No implicit filesystem/network imports, recursive module
 * dependencies, dynamic module runtime, or fallback. The prior overloads
 * remain source-compatible and reject undeclared imported tags.
 */
UiDocumentCompileResult compile_ui_xml(
    std::string_view source, const WidgetRegistry &registry,
    const std::vector<UiXmlModuleSource> &modules,
    const UiDocumentLimits &limits = {});

} // namespace flexUI
