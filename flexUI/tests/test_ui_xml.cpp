#include <flexUI.h>

#include <tinytest.hpp>

#include <string>

using flexUI::UiDocumentErrorCode;

namespace {

constexpr const char *kXmlDocument = R"(
<ui name="MainWindow"
    xmlns:on="urn:flexui:event"
    xmlns:bind="urn:flexui:binding"
    xmlns:attr="urn:flexui:attribute">
  <resources>
    <resource type="image" id="logo" path="images/logo.png"
              preload="true" scale="2" />
  </resources>
  <div id="root" class="app-shell" utility="flex" role="application">
    <button id="save" text="Save" focusable="true" tab_index="2"
            attr:label="Save document" on:click="save_document"
            bind:class_active="${enabled}" disabled="false" />
  </div>
</ui>
)";

} // namespace

spec("FlexUI XML adapter produces the shared immutable UI program") {
  it("lowers structure events bindings and resources") {
    const auto compiled = flexUI::compile_ui_xml(kXmlDocument);
    check(static_cast<bool>(compiled));
    check_equal(compiled.program->name(), "MainWindow");
    check_equal(compiled.program->definition().root.tag, "div");
    check_equal(compiled.program->definition().root.id, "root");
    check_equal(compiled.program->definition().root.children.size(), 1);

    const auto &button = compiled.program->definition().root.children.front();
    check_equal(button.tag, "button");
    check_equal(std::get<bool>(button.properties.at("focusable")), true);
    check_equal(std::get<float>(button.properties.at("tab_index")), 2.0F);
    check_equal(std::get<std::string>(button.properties.at("attr.label")), "Save document");

    check_equal(compiled.program->event_bindings().size(), 1);
    check_equal(compiled.program->event_bindings().front().handler, "save_document");
    check(compiled.program->event_bindings().front().source.line > 0);
    check_equal(compiled.program->bindings().size(), 1);
    check_equal(compiled.program->bindings().front().target_name, "active");
    check_equal(compiled.program->bindings().front().dependencies.front(), "enabled");

    check_equal(compiled.program->resources().size(), 1);
    const auto &resource = compiled.program->resources().front();
    check_equal(resource.type, "image");
    check_equal(resource.id, "logo");
    check_equal(resource.path, "images/logo.png");
    check_equal(std::get<bool>(resource.options.at("preload")), true);
    check_equal(std::get<float>(resource.options.at("scale")), 2.0F);
  }

  it("matches legacy frontend semantic lowering") {
    const auto legacy = flexUI::compile_ui_document(R"(
      assets { image logo: "images/logo.png" { preload: true scale: 2 } }
      ui MainWindow {
        div root {
          class: "app-shell", utility: "flex", role: "application",
          button save {
            text: "Save", focusable: true, tab_index: 2,
            attr.label: "Save document", on.click: "save_document",
            bind.class_active: "${enabled}", disabled: false
          }
        }
      }
    )");
    const auto xml = flexUI::compile_ui_xml(kXmlDocument);
    check(static_cast<bool>(legacy));
    check(static_cast<bool>(xml));

    check_equal(xml.program->definition().root.id, legacy.program->definition().root.id);
    check_equal(xml.program->event_bindings().size(), legacy.program->event_bindings().size());
    check_equal(xml.program->event_bindings().front().handler,
                legacy.program->event_bindings().front().handler);
    check_equal(xml.program->bindings().size(), legacy.program->bindings().size());
    check_equal(xml.program->bindings().front().expression,
                legacy.program->bindings().front().expression);
    check_equal(xml.program->resources().size(), legacy.program->resources().size());
  }

  it("returns a parser-independent definition") {
    const auto parsed = flexUI::parse_ui_xml(kXmlDocument);
    check(static_cast<bool>(parsed));
    check_equal(parsed.definition->name, "MainWindow");
    check_equal(parsed.definition->root.children.front().id, "save");
  }

  it("rejects malformed and truncated sources") {
    const auto malformed = flexUI::compile_ui_xml("<ui name=\"Bad\"><div id=\"root\"></ui>");
    check_false(static_cast<bool>(malformed));
    check(malformed.error.code == UiDocumentErrorCode::ParseError);
    check(malformed.error.line > 0);

    std::string embedded_nul = "<ui name=\"A\"><div id=\"root\"/></ui>";
    embedded_nul.push_back('\0');
    embedded_nul += "<ui name=\"Hidden\"><div id=\"hidden\"/></ui>";
    const auto truncated = flexUI::compile_ui_xml(embedded_nul);
    check_false(static_cast<bool>(truncated));
    check(truncated.error.code == UiDocumentErrorCode::ParseError);
  }


  it("rejects malformed UTF-8 before XML parsing with source location") {
    std::string invalid =
        "<ui name=\"Bad\">\n"
        "  <div id=\"root\" text=\"";
    invalid.push_back(static_cast<char>(0xFF));
    invalid += "\"/>\n</ui>";

    const auto compiled = flexUI::compile_ui_xml(invalid);
    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::InvalidUtf8);
    check_equal(compiled.error.line, 2);
    check_equal(compiled.error.column, 24);

    const auto parsed = flexUI::parse_ui_xml(invalid);
    check_false(static_cast<bool>(parsed));
    check(parsed.error.code == UiDocumentErrorCode::InvalidUtf8);
    check_equal(parsed.error.line, 2);
    check_equal(parsed.error.column, 24);
  }

  it("validates the complete XML byte view after an otherwise complete document") {
    std::string invalid_suffix = "<ui name=\"A\"><div id=\"root\"/></ui>";
    invalid_suffix.push_back(static_cast<char>(0xE2));
    invalid_suffix.push_back(static_cast<char>(0x82));

    const auto compiled = flexUI::compile_ui_xml(invalid_suffix);
    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::InvalidUtf8);
    check_equal(compiled.error.line, 1);
    check(compiled.error.column > 1);
  }

  it("rejects invalid structure duplicate ids and text nodes") {
    const auto roots =
        flexUI::compile_ui_xml("<ui name=\"Two\"><div id=\"a\"/><div id=\"b\"/></ui>");
    check_false(static_cast<bool>(roots));
    check(roots.error.code == UiDocumentErrorCode::InvalidRootCount);

    const auto duplicate =
        flexUI::compile_ui_xml("<ui name=\"Dup\"><div id=\"same\"><span id=\"same\"/></div></ui>");
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == UiDocumentErrorCode::DuplicateElementId);

    const auto text =
        flexUI::compile_ui_xml("<ui name=\"Text\"><div id=\"root\">content</div></ui>");
    check_false(static_cast<bool>(text));
    check(text.error.code == UiDocumentErrorCode::InvalidNode);

    const auto duplicate_attribute =
        flexUI::compile_ui_xml("<ui name=\"Attr\"><div id=\"root\" class=\"a\" class=\"b\"/></ui>");
    check_false(static_cast<bool>(duplicate_attribute));
    check(duplicate_attribute.error.code == UiDocumentErrorCode::ParseError ||
          duplicate_attribute.error.code == UiDocumentErrorCode::InvalidProperty);

    const auto unknown_namespace = flexUI::compile_ui_xml(
        "<ui name=\"Ns\" xmlns:x=\"urn:unknown\"><div id=\"root\" x:value=\"1\"/></ui>");
    check_false(static_cast<bool>(unknown_namespace));
    check(unknown_namespace.error.code == UiDocumentErrorCode::InvalidProperty);
  }

  it("applies document resource and property limits") {
    flexUI::UiDocumentLimits source_limits;
    source_limits.max_source_bytes = 8;
    const auto source = flexUI::compile_ui_xml(kXmlDocument, source_limits);
    check_false(static_cast<bool>(source));
    check(source.error.code == UiDocumentErrorCode::SourceTooLarge);

    flexUI::UiDocumentLimits resource_limits;
    resource_limits.max_resources = 0;
    const auto resource = flexUI::compile_ui_xml(kXmlDocument, resource_limits);
    check_false(static_cast<bool>(resource));
    check(resource.error.code == UiDocumentErrorCode::ResourceLimitExceeded);

    flexUI::UiDocumentLimits property_limits;
    property_limits.max_properties_per_node = 1;
    const auto property = flexUI::compile_ui_xml(kXmlDocument, property_limits);
    check_false(static_cast<bool>(property));
    check(property.error.code == UiDocumentErrorCode::PropertyLimitExceeded);
  }
}
