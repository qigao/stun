#include <flexUI/ui_jinja_xml.h>

#include <flexUI/box.h>
#include <flexUI/ui_document.h>
#include <flexUI/widget_registry.h>
#include <flexUI/widgets/button_widget.h>

#include <jinja_cmeta.h>
#include <cmeta/data_reflect.h>
#include <tinytest.hpp>

#include <string>
#include <string_view>

namespace {

struct StudioInputs {
  int count;
};

cmeta_reflect_data(StudioInputs, "stun.test.StudioInputs",
    cmeta_data_field(int, count, &cmeta_data_int, &cmeta_type_int));

struct StudioTextInputs {
  vstr title;
};

cmeta_reflect_data(StudioTextInputs, "stun.test.StudioTextInputs",
    // vstr is not a builtin CMeta storage type. The exact public Jinja
    // DataDesc and its storage witness must both be supplied; otherwise
    // cmeta_data_desc_valid() rejects the complete root metadata at render.
    cmeta_data_field_id(vstr, title, "stun.test.StudioTextInputs.title",
                        jinja_cmeta_vstr_data(),
                        static_cast<const cmeta_type_desc *>(
                            jinja_cmeta_vstr_data()->storage_type)));

flexUI::UiJinjaXmlCompileResult compile(
    std::string_view source, const StudioInputs& inputs,
    const flexUI::UiDocumentLimits& limits = {}) {
  static const auto registry = flexUI::WidgetRegistry::builtins();
  return flexUI::compile_jinja_ui_xml(
      "studio.ui.xml.jinja", source,
      cmeta_reflected_data(StudioInputs), &inputs, registry, limits);
}

} // namespace

spec("FlexUI uses installed JinjaCMeta only as a staged XML author compiler") {
  it("compiles typed CMeta if/for data through the real XML and WidgetRegistry") {
    const StudioInputs input{2};
    const auto compiled = compile(R"(
      <ui name="JinjaStudio" xmlns:on="urn:flexui:event">
        <main id="root" class="flex flex-col">
          {% if count > 0 %}
            {% for i in range(count) %}
              <button id="action-{{ i }}" on:click="activate">Save</button>
            {% endfor %}
          {% endif %}
        </main>
      </ui>
    )", input);
    check_true(static_cast<bool>(compiled));
    if (compiled) {
      check_equal(compiled.program->name(), "JinjaStudio");
      check_equal(compiled.program->definition().root.children.size(), 2u);
      check_equal(compiled.program->definition().root.children[0].id, "action-0");
      check_equal(compiled.program->definition().root.children[1].id, "action-1");
      check_equal(compiled.program->event_bindings().size(), 2u);

    // Real, unmodified production Widget factory and Box staging path.
    auto registry = flexUI::WidgetRegistry::builtins();
    flexUI::Box box(nullptr);
    const auto installed =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check_true(static_cast<bool>(installed));
    check_not_null(box.root());
    check_not_null(box.get_by_id("action-0"));
    check_not_null(box.get_by_id("action-1"));
    check_not_null(dynamic_cast<flexUI::ButtonWidget*>(
        box.get_by_id("action-0")->widget));
    }
  }

  it("renders zero matching branches without an extra UI root") {
    const auto compiled = compile(R"(
      <ui name="Empty">
        <div id="root">
          {% if count > 0 %}<button id="shown">On</button>{% endif %}
        </div>
      </ui>
    )", StudioInputs{0});
    check_true(static_cast<bool>(compiled));
    if (compiled) {
      check_equal(compiled.program->definition().root.children.size(), 0u);
    }
  }

  it("autoescapes interpolated XML markup instead of creating injected nodes") {
    const auto compiled = compile(R"(
      <ui name="Escaped">
        <label id="title">{{ '<button id="injected" />' }}</label>
      </ui>
    )", StudioInputs{1});
    check_true(static_cast<bool>(compiled));
    if (compiled) {
      const auto& root = compiled.program->definition().root;
      check_equal(root.tag, "label");
      check_equal(root.children.size(), 0u);
      check_equal(std::get<std::string>(root.properties.at("text")),
                  "<button id=\"injected\" />");
    }
  }

  it("escapes borrowed CMeta strings in XML attributes and text nodes") {
    const std::string title = "<button id='injected'/> & \"quoted\"";
    const StudioTextInputs data{vstr_from_buf(title.data(), title.size())};
    auto registry = flexUI::WidgetRegistry::builtins();

    const auto attr = flexUI::compile_jinja_ui_xml(
        "author.ui.xml.jinja",
        R"(<ui name="EscapedAttribute"><label id="safe" text="{{ title }}"/></ui>)",
        cmeta_reflected_data(StudioTextInputs), &data, registry);
    if (!attr) {
      info("Jinja attribute stage=%d template_status=%d ui_code=%d "
           "template_offset=%zu message=%s",
           static_cast<int>(attr.error.stage), attr.error.template_status,
           static_cast<int>(attr.error.ui_error.code),
           attr.error.template_offset, attr.error.message.c_str());
    }
    check_true(static_cast<bool>(attr));
    if (attr) {
      const auto& root = attr.program->definition().root;
      check_equal(root.children.size(), 0u);
      check_equal(std::get<std::string>(root.properties.at("text")), title);
    }

    const auto text = flexUI::compile_jinja_ui_xml(
        "author.ui.xml.jinja",
        R"(<ui name="EscapedText"><label id="safe">{{ title }}</label></ui>)",
        cmeta_reflected_data(StudioTextInputs), &data, registry);
    check_true(static_cast<bool>(text));
    if (text) {
      const auto& root = text.program->definition().root;
      check_equal(root.children.size(), 0u);
      check_equal(std::get<std::string>(root.properties.at("text")), title);
    }
  }

  it("reports template syntax and strict-undefined failures separately") {
    const auto syntax = compile(
        "{% if count %}<ui name='Bad'><div/></ui>", StudioInputs{1});
    check_false(static_cast<bool>(syntax));
    check(syntax.error.stage == flexUI::UiJinjaXmlStage::TemplateCompile);
    check(syntax.error.template_status == JINJA_CMETA_ERR_SYNTAX);
    check_equal(syntax.error.source_name, std::string("studio.ui.xml.jinja"));
    check_equal(syntax.error.template_line, 1);
    check_equal(syntax.error.template_column, 1);
    check_equal(syntax.error.ui_error.line, 0);

    const auto second_line = compile(
        "<ui name='Unused'/>\r\n{% endif %}", StudioInputs{1});
    check_false(static_cast<bool>(second_line));
    check(second_line.error.stage == flexUI::UiJinjaXmlStage::TemplateCompile);
    check_equal(second_line.error.template_line, 2);
    check_equal(second_line.error.template_column, 1);

    const auto undefined = compile(
        "<ui name='Missing'><label>{{ missing_name }}</label></ui>",
        StudioInputs{1});
    check_false(static_cast<bool>(undefined));
    check(undefined.error.stage == flexUI::UiJinjaXmlStage::TemplateRender);
  }

  it("rejects malformed XML and duplicate generated IDs before publication") {
    const auto malformed = compile(
        "<ui name='Bad'><div></ui>", StudioInputs{1});
    check_false(static_cast<bool>(malformed));
    check(malformed.error.stage == flexUI::UiJinjaXmlStage::XmlCompile);
    check(malformed.error.ui_error.code == flexUI::UiDocumentErrorCode::ParseError);
    check_equal(malformed.error.source_name, std::string("studio.ui.xml.jinja"));
    check_equal(malformed.error.template_line, 0);
    check_equal(malformed.error.template_column, 0);
    check(malformed.error.ui_error.line > 0);

    const auto duplicate = compile(R"(
      <ui name="Duplicate"><main>
        {% for i in range(count) %}<label id="same"/>{% endfor %}
      </main></ui>
    )", StudioInputs{2});
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.stage == flexUI::UiJinjaXmlStage::XmlCompile);
    check(duplicate.error.ui_error.code ==
          flexUI::UiDocumentErrorCode::DuplicateElementId);
  }

  it("rejects unknown Widget and invalid Widget child content in registry") {
    const auto unknown = compile(
        "<ui name='Unknown'><unsupported id='x'/></ui>", StudioInputs{1});
    check_false(static_cast<bool>(unknown));
    check(unknown.error.stage == flexUI::UiJinjaXmlStage::RegistryValidate);
    check(unknown.error.ui_error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);

    const auto bad_children = compile(
        "<ui name='Invalid'><input id='name'><label/></input></ui>",
        StudioInputs{1});
    check_false(static_cast<bool>(bad_children));
    check(bad_children.error.stage == flexUI::UiJinjaXmlStage::RegistryValidate);
    check(bad_children.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidNode);
  }

  it("bounds source and expanded output separately and keeps failed Box empty") {
    flexUI::UiDocumentLimits limits;
    limits.max_source_bytes = 128;
    const auto source_limit = compile(
        "<ui name='Source'><div id='root'/></ui>"
        "<!-- pad enough to exceed the configured limit 0123456789012345"
        "6789012345678901234567890123456789012345678901234567890123456 -->",
        StudioInputs{1}, limits);
    check_false(static_cast<bool>(source_limit));
    check(source_limit.error.stage == flexUI::UiJinjaXmlStage::SourceLimit);

    const auto expanded = compile(
        "<ui name='Limit'><div>{% for i in range(40) %}"
        "<label id='row-{{i}}'/>{% endfor %}</div></ui>",
        StudioInputs{1}, limits);
    check_false(static_cast<bool>(expanded));
    check(expanded.error.stage == flexUI::UiJinjaXmlStage::OutputLimit);

    // The compiler neither takes a Box nor invokes a Widget factory.
    flexUI::Box box(nullptr);
    check_null(box.root());
    check_null(box.get_by_id("row-0"));

    const auto recovered = compile(
        "<ui name='Recovered'><button id='save'>Save</button></ui>",
        StudioInputs{1});
    check_true(static_cast<bool>(recovered));
    if (recovered) {
      const auto installed = flexUI::UiDocumentInstantiator::instantiate(
          box, *recovered.program, flexUI::WidgetRegistry::builtins());
      check_true(static_cast<bool>(installed));
      check_not_null(box.get_by_id("save"));
    }
  }


  it("maps canonical XML byte-column errors back into staged output, not CRLF Jinja source") {
    // The canonical Jinja compiler normalizes source CRLF; it does not change
    // the actual UTF-8 scalars. The XML parser counts generated bytes.
    const std::string author =
        "<ui name='Unicode'>\r\n"
        "  <main>\r\n"
        "    <label id='title'>\xE4\xB8\xAD</label>\r\n"
        "    <unsupported id='broken'/>\r\n"
        "  </main>\r\n"
        "</ui>";
    const std::string generated =
        "<ui name='Unicode'>\n"
        "  <main>\n"
        "    <label id='title'>\xE4\xB8\xAD</label>\n"
        "    <unsupported id='broken'/>\n"
        "  </main>\n"
        "</ui>";
    const auto wrong_widget = compile(author, StudioInputs{1});
    check_false(static_cast<bool>(wrong_widget));
    check(wrong_widget.error.stage == flexUI::UiJinjaXmlStage::RegistryValidate);
    check(wrong_widget.error.ui_error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check_equal(wrong_widget.error.ui_error.line, 4);
    // PugiXML's canonical offset_debug() points to the first NAME byte
    // after '<', not to the '<' delimiter. Never normalize that position.
    check_equal(wrong_widget.error.ui_error.column, 6);
    check_equal(wrong_widget.error.generated_offset, generated.find("<unsupported") + 1u);
    check(wrong_widget.error.generated_offset != author.find("<unsupported") + 1u);
    check_equal(wrong_widget.error.template_line, 0);
    check_equal(wrong_widget.error.template_column, 0);

    const std::string one_line =
        "<ui name='Unicode'><main><label id='title'>\xE4\xB8\xAD</label>"
        "<unsupported id='broken'/></main></ui>";
    const auto utf8 = compile(one_line, StudioInputs{1});
    check_false(static_cast<bool>(utf8));
    check(utf8.error.stage == flexUI::UiJinjaXmlStage::RegistryValidate);
    check_equal(utf8.error.generated_offset, one_line.find("<unsupported") + 1u);
    check_equal(utf8.error.ui_error.line, 1);
    check_equal(static_cast<std::size_t>(utf8.error.ui_error.column),
                one_line.find("<unsupported") + 2u);
  }

  it("does not invent generated or original source offsets for errors with no position") {
    const auto missing_root = compile("<div id='orphan'/>", StudioInputs{1});
    check_false(static_cast<bool>(missing_root));
    check(missing_root.error.stage == flexUI::UiJinjaXmlStage::XmlCompile);
    check(missing_root.error.ui_error.code == flexUI::UiDocumentErrorCode::MissingDocument);
    check_equal(missing_root.error.ui_error.line, 0);
    check_equal(missing_root.error.ui_error.column, 0);
    check_equal(missing_root.error.generated_offset, std::string_view::npos);
    check_equal(missing_root.error.template_line, 0);
    check_equal(missing_root.error.template_column, 0);

    const std::string incomplete = "<ui name='Broken'><main>";
    const auto truncated = compile(incomplete, StudioInputs{1});
    check_false(static_cast<bool>(truncated));
    check(truncated.error.stage == flexUI::UiJinjaXmlStage::XmlCompile);
    check(truncated.error.ui_error.code == flexUI::UiDocumentErrorCode::ParseError);
    if (truncated.error.ui_error.line > 0 && truncated.error.ui_error.column > 0) {
      check(truncated.error.generated_offset != std::string_view::npos);
      check(truncated.error.generated_offset <= incomplete.size());
    }
    check_equal(truncated.error.template_line, 0);
    check_equal(truncated.error.template_column, 0);
  }

  it("disables implicit Jinja loader and rejects invalid context descriptors") {
    const auto loader = compile(
        "{% include 'other.ui.xml.jinja' %}<ui name='NoLoader'><div/></ui>",
        StudioInputs{1});
    check_false(static_cast<bool>(loader));
    check(loader.error.stage == flexUI::UiJinjaXmlStage::TemplateRender);

    const auto invalid = flexUI::compile_jinja_ui_xml(
        "studio.ui.xml.jinja", "<ui name='Ok'><div/></ui>",
        nullptr, nullptr, flexUI::WidgetRegistry::builtins());
    check_false(static_cast<bool>(invalid));
    check(invalid.error.stage == flexUI::UiJinjaXmlStage::InvalidInput);
  }
}
