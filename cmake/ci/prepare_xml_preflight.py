"""One-shot source transformation for #35; removed from the published feature tree."""
from pathlib import Path
import hashlib
import re

EXPECTED = {
    'flexUI/include/flexUI/ui_document.h': '4d823acb951e1c218d30d42534a886d5c61ccf30',
    'flexUI/include/flexUI/widget_registry.h': '30cc7a9b8af05d92925719ab92b6bdcb2864e971',
    'flexUI/src/widget_registry.cpp': '46c29a16a36f8d7b84d0ef6fe8a141035bd26fd9',
    'flexUI/src/ui_document.cpp': 'f2bf967ccaf834ddabc4012b35c799d1f13bd732',
    'flexUI/src/ui_xml.cpp': '28b7f8e7800b47191d59e5c703f8b4d767222c45',
    'flexUI/modules/controller/application.cpp': 'f19079164fc3b5191fab01c40c41793cc6c16096',
    'flexUI/tests/test_widget_registry.cpp': 'fe06b0d4f239250b106de4482656100079741ea4',
    'flexUI/tests/test_desktop_application.cpp': '646a99f8ef29e2d8f9397e618246500e782ad127',
    'flexUI/tests/CMakeLists.txt': '074ccd6f1a4d5293a5e011d9ab3449b9e689ac36',
    '.github/workflows/unicode-runtime.yml': '59933dea6d7e4e78162d475ba35c4b19447e4160',
}

for filename, expected in EXPECTED.items():
    data = Path(filename).read_bytes()
    actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    assert actual == expected, (filename, actual)


def replace(filename, old, new):
    p = Path(filename)
    text = p.read_text()
    assert text.count(old) == 1, (filename, old)
    p.write_text(text.replace(old, new, 1))


replace('flexUI/include/flexUI/ui_node_schema.h', '#include <flexUI/ui_document.h>\n',
        '#include <flexUI/ui_document.h>\n\n#include <initializer_list>\n#include <utility>\n')
replace('flexUI/include/flexUI/ui_document.h', '  std::vector<UiNodeDefinition> children;\n',
        '  std::vector<UiNodeDefinition> children;\n  /// Source location of the owning tag; appended to preserve aggregate field order.\n  SourceSpan source;\n')
replace('flexUI/include/flexUI/ui_document.h', '/// Immutable result of UI parsing and semantic lowering.\n',
        '/// Immutable result of structural parsing and event/binding lowering.\n/// Registry content schemas are checked separately before typed instantiation.\n')
replace('flexUI/include/flexUI/ui_document.h', '  /// @param box Target ownership and ID-index boundary.\n',
        '  /// Structural-only overload: creates generic Elements, not registered widgets.\n  /// It does not establish registry schema acceptance (tracked by #16/#33).\n  /// @param box Target ownership and ID-index boundary.\n')
replace('flexUI/include/flexUI/widget_registry.h', '#include <flexUI/ui_document.h>',
        '#include <flexUI/ui_node_schema.h>')
replace('flexUI/include/flexUI/widget_registry.h', '  FactoryFailed,\n',
        '  FactoryFailed,\n  InvalidDescriptor,\n  InvalidContent,\n')
replace('flexUI/include/flexUI/widget_registry.h', '  WidgetRegistryError register_element(std::string tag);',
        '  WidgetRegistryError register_element(\n      std::string tag, UiContentModel content = UiContentModel::Children);')
replace('flexUI/include/flexUI/widget_registry.h', '  WidgetRegistryError register_widget(std::string tag, Factory factory);',
        '''  /// The default preserves the existing custom-widget contract. Built-ins
  /// declare their content model explicitly; no unknown tag is admitted.
  WidgetRegistryError register_widget(
      std::string tag, Factory factory,
      UiContentModel content = UiContentModel::TextAndChildren);''')
replace('flexUI/include/flexUI/widget_registry.h', '  bool contains(std::string_view tag) const;\n',
        '''  bool contains(std::string_view tag) const;

  /// Registry-owned immutable metadata, or nullptr for an unknown tag.
  const UiNodeDescriptor *descriptor(std::string_view tag) const;

  /// Whole-tree preflight, without factories or Box mutation. O(nodes) time,
  /// O(depth) auxiliary storage, bounded by the supplied document limits.
  /// This validates registered tags and content, not all property schemas (#16).
  UiDocumentError validate(const UiDocumentDefinition &definition,
                           const UiDocumentLimits &limits = {}) const;
  UiDocumentError validate(const CompiledUiProgram &program,
                           const UiDocumentLimits &limits = {}) const;
''')
replace('flexUI/include/flexUI/widget_registry.h', '  /// Creates the Widget associated with a definition.\n',
        '  /// Creates one Widget after checking its own node content. Whole-tree\n  /// callers must use validate() before invoking any factory.\n')
replace('flexUI/include/flexUI/widget_registry.h', '  struct Entry {\n    Factory factory;\n',
        '  struct Entry {\n    UiNodeDescriptor descriptor;\n    Factory factory;\n')

p = 'flexUI/src/widget_registry.cpp'
replace(p, 'void register_default_widget(WidgetRegistry &registry, const char *tag) {',
        'void register_default_widget(WidgetRegistry &registry, const char *tag,\n                             UiContentModel content) {')
replace(p, '      tag, [](const UiNodeDefinition &) { return std::make_unique<WidgetT>(); }));',
        '      tag, [](const UiNodeDefinition &) { return std::make_unique<WidgetT>(); }, content));')
replace(p, 'WidgetRegistryError WidgetRegistry::register_element(std::string tag) {',
        'WidgetRegistryError WidgetRegistry::register_element(std::string tag, UiContentModel content) {')
replace(p, '  if (!entries_.emplace(std::move(tag), Entry{}).second) {',
        '''  const UiNodeDescriptor schema{UiNodeKind::Container, content};
  if (!schema.valid()) {
    return make_error(WidgetRegistryErrorCode::InvalidDescriptor, "invalid container content model");
  }
  if (!entries_.emplace(std::move(tag), Entry{schema, {}}).second) {''')
replace(p, 'WidgetRegistryError WidgetRegistry::register_widget(std::string tag, Factory factory) {',
        'WidgetRegistryError WidgetRegistry::register_widget(std::string tag, Factory factory,\n                                                      UiContentModel content) {')
replace(p, '  if (!entries_.emplace(std::move(tag), Entry{std::move(factory)}).second) {',
        '''  const UiNodeDescriptor schema{UiNodeKind::Widget, content};
  if (!schema.valid()) {
    return make_error(WidgetRegistryErrorCode::InvalidDescriptor, "invalid widget content model");
  }
  if (!entries_.emplace(std::move(tag), Entry{schema, std::move(factory)}).second) {''')
replace(p, 'WidgetCreationResult WidgetRegistry::create(const UiNodeDefinition &definition) const {',
        '''const UiNodeDescriptor *WidgetRegistry::descriptor(std::string_view tag) const {
  const auto found = entries_.find(std::string(tag));
  return found == entries_.end() ? nullptr : &found->second.descriptor;
}

UiDocumentError WidgetRegistry::validate(const UiDocumentDefinition &definition,
                                         const UiDocumentLimits &limits) const {
  std::size_t visited = 0;
  const auto check_node = [&](const UiNodeDefinition &node, std::size_t depth) -> UiDocumentError {
    if (depth > limits.max_depth) {
      return {UiDocumentErrorCode::DepthLimitExceeded, "UI schema exceeds maximum tree depth",
              node.source.line, node.source.column};
    }
    if (visited >= limits.max_nodes) {
      return {UiDocumentErrorCode::NodeLimitExceeded, "UI schema exceeds maximum node count",
              node.source.line, node.source.column};
    }
    ++visited;
    const auto *schema = descriptor(node.tag);
    if (!schema) {
      return {UiDocumentErrorCode::UnknownElementTag, "unknown registered UI tag: " + node.tag,
              node.source.line, node.source.column};
    }
    return schema->validate_content(node);
  };
  if (auto error = check_node(definition.root, 1)) {
    return error;
  }
  struct Frame { const UiNodeDefinition *node; std::size_t next_child; };
  std::vector<Frame> path{{&definition.root, 0}};
  while (!path.empty()) {
    auto &frame = path.back();
    if (frame.next_child == frame.node->children.size()) {
      path.pop_back();
      continue;
    }
    const auto *child = &frame.node->children[frame.next_child++];
    if (auto error = check_node(*child, path.size() + 1)) {
      return error;
    }
    path.push_back({child, 0});
  }
  return {};
}

UiDocumentError WidgetRegistry::validate(const CompiledUiProgram &program,
                                         const UiDocumentLimits &limits) const {
  return validate(program.definition(), limits);
}

WidgetCreationResult WidgetRegistry::create(const UiNodeDefinition &definition) const {''')
replace(p, '  if (!found->second.factory) {\n',
        '''  if (const auto error = found->second.descriptor.validate_content(definition)) {
    return {{}, make_error(WidgetRegistryErrorCode::InvalidContent, error.message)};
  }
  if (!found->second.factory) {
''')
replace(p, '    require_registration(registry.register_element(tag));',
        '''    // Generic Elements already support scalar text and ordered children.
    // Preserve that explicit container contract, not an unknown-tag fallback.
    require_registration(registry.register_element(tag, UiContentModel::TextAndChildren));''')

DIRECT = {
    'button': 'TextAndChildren', 'label': 'Text', 'input': 'Empty',
    'checkbox': 'Text', 'radio': 'Text', 'switch': 'Text', 'slider': 'Empty',
    'progress': 'Empty', 'textarea': 'Text', 'select': 'Empty',
    'tooltip': 'TextAndChildren', 'modal': 'TextAndChildren', 'image': 'Empty',
    'badge': 'Text', 'toast': 'Text', 'dropdown': 'Empty', 'avatar': 'Empty',
    'pagination': 'Empty', 'stepper': 'Empty', 'markdown': 'Text',
}
DEFAULT = {
    'tabs': 'Children', 'spinner': 'Empty', 'divider': 'Empty', 'accordion': 'Children',
    'calendar': 'Empty', 'table': 'Children', 'tree': 'Children', 'colorpicker': 'Empty',
    'group-button': 'Children', 'toggle-group': 'Children', 'card': 'TextAndChildren',
    'breadcrumb': 'Children', 'gradient-editor': 'Empty',
}
text = Path(p).read_text()
for tag, model in DIRECT.items():
    pattern = r'(  require_registration\(registry\.register_widget\("' + re.escape(tag) + r'",.*?\n  \})\)\);'
    text, n = re.subn(pattern, lambda m: m[1] + ', UiContentModel::' + model + '));', text, flags=re.S)
    assert n == 1, tag
for tag, model in DEFAULT.items():
    pattern = r'(  register_default_widget<\w+>\(registry, "' + re.escape(tag) + r'")\);'
    text, n = re.subn(pattern, lambda m: m[1] + ', UiContentModel::' + model + ');', text)
    assert n == 1, tag
Path(p).write_text(text)

replace('flexUI/src/ui_xml.cpp',
        '  const auto location = source_span(source, xml_node.offset_debug());\n  for (const auto &attribute : xml_node.attributes()) {',
        '  const auto location = source_span(source, xml_node.offset_debug());\n  definition.source = SourceSpan{location.line, location.column, tag.size()};\n  for (const auto &attribute : xml_node.attributes()) {')
replace('flexUI/src/ui_document.cpp', '  result.error = validate_target(box, definition);',
        '''  if (registry) {
    result.error = registry->validate(definition);
    if (result.error) {
      return result;
    }
  }

  result.error = validate_target(box, definition);''')
replace('flexUI/modules/controller/application.cpp',
        '    if (!config.script_enabled && !compiled.program->event_bindings().empty()) {',
        '''    if (auto schema_error = config.registry.validate(*compiled.program, config.limits.document)) {
      auto error = fail(DesktopApplicationErrorCode::UiCompileFailed,
                        DesktopApplicationStage::UiCompile, schema_error.message);
      error.ui_error = std::move(schema_error);
      return {{}, std::move(error)};
    }

    if (!config.script_enabled && !compiled.program->event_bindings().empty()) {''')
replace('flexUI/tests/CMakeLists.txt', 'add_executable(test_text_util_unicode test_text_util_unicode.cpp)',
        '''add_executable(test_ui_content_models test_ui_content_models.cpp)
target_include_directories(test_ui_content_models PRIVATE ../include)
flexui_configure_tinytest(test_ui_content_models)
set_target_properties(test_ui_content_models PROPERTIES FOLDER "flexUI/tests")
add_test(NAME test_ui_content_models COMMAND test_ui_content_models)

add_executable(test_text_util_unicode test_text_util_unicode.cpp)''')
replace('.github/workflows/unicode-runtime.yml',
        '            test_text_util_unicode test_render_semantics test_widget_contracts\n',
        '            test_text_util_unicode test_render_semantics test_widget_contracts\n            test_ui_content_models test_ui_xml test_ui_document test_widget_registry\n')

with Path('flexUI/tests/test_widget_registry.cpp').open('a') as out:
    out.write(r'''

spec("FlexUI registry preflight rejects invalid content before construction") {
  it("classifies built-in leaves text widgets composites and structural containers") {
    const auto registry = flexUI::WidgetRegistry::builtins();
    for (const auto *tag : {"input", "image", "slider", "progress", "select", "spinner", "divider"}) {
      const auto *schema = registry.descriptor(tag);
      check_not_null(schema);
      if (schema) {
        check(schema->kind == flexUI::UiNodeKind::Widget);
        check(schema->content == flexUI::UiContentModel::Empty);
      }
    }
    for (const auto *tag : {"label", "textarea", "badge", "checkbox", "radio", "switch", "markdown"}) {
      const auto *schema = registry.descriptor(tag);
      check_not_null(schema);
      if (schema) check(schema->content == flexUI::UiContentModel::Text);
    }
    check(registry.descriptor("button")->content == flexUI::UiContentModel::TextAndChildren);
    check(registry.descriptor("div")->kind == flexUI::UiNodeKind::Container);
    check(registry.descriptor("div")->content == flexUI::UiContentModel::TextAndChildren);
    check_null(registry.descriptor("unregistered"));
  }

  it("rejects an unknown later sibling before calling an earlier factory") {
    int calls = 0;
    flexUI::WidgetRegistry registry;
    check_false(static_cast<bool>(registry.register_element("root")));
    check_false(static_cast<bool>(registry.register_widget("probe", [&](const flexUI::UiNodeDefinition &) {
      ++calls;
      return std::make_unique<flexUI::ButtonWidget>();
    }, flexUI::UiContentModel::Empty)));
    auto parsed = flexUI::compile_ui_xml("<ui name=\"Order\"><root id=\"root\"><probe id=\"first\"/><missing id=\"last\"/></root></ui>");
    check(static_cast<bool>(parsed));
    if (!parsed) return;
    flexUI::Box box(nullptr);
    for (const bool compiled : {false, true}) {
      const auto result = compiled
          ? flexUI::UiDocumentInstantiator::instantiate(box, *parsed.program, registry)
          : flexUI::UiDocumentInstantiator::instantiate(box, parsed.program->definition(), registry);
      check_false(static_cast<bool>(result));
      check(result.error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
      check_equal(calls, 0);
      check_null(box.root());
      check_null(box.get_by_id("first"));
      check_null(box.get_by_id("last"));
    }
  }

  it("rejects leaf children and text before allocating a typed tree") {
    const auto registry = flexUI::WidgetRegistry::builtins();
    for (const auto *xml : {
        "<ui name=\"Leaf\"><input id=\"root\"><label id=\"child\"/></input></ui>",
        "<ui name=\"Leaf\"><image id=\"root\" text=\"\"/></ui>",
        "<ui name=\"Leaf\"><label id=\"root\"><button id=\"child\"/></label></ui>"}) {
      const auto parsed = flexUI::compile_ui_xml(xml);
      check(static_cast<bool>(parsed));
      if (!parsed) continue;
      flexUI::Box box(nullptr);
      const auto result = flexUI::UiDocumentInstantiator::instantiate(box, *parsed.program, registry);
      check_false(static_cast<bool>(result));
      check_null(box.root());
      check_null(box.get_by_id("root"));
    }
  }

  it("reports exact node positions and bounded traversal errors") {
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto parsed = flexUI::compile_ui_xml("<ui name=\"Position\">\n  <div id=\"root\">\n    <unknown id=\"bad\"/>\n  </div>\n</ui>");
    check(static_cast<bool>(parsed));
    if (!parsed) return;
    auto error = registry.validate(*parsed.program);
    check(error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check_equal(error.line, 3);
    check_equal(error.column, 6);
    flexUI::UiDocumentLimits limits;
    limits.max_nodes = 1;
    error = registry.validate(*parsed.program, limits);
    check(error.code == flexUI::UiDocumentErrorCode::NodeLimitExceeded);
    limits.max_nodes = 2;
    limits.max_depth = 1;
    error = registry.validate(*parsed.program, limits);
    check(error.code == flexUI::UiDocumentErrorCode::DepthLimitExceeded);
  }

  it("checks direct factory content and preserves duplicate descriptors") {
    int calls = 0;
    flexUI::WidgetRegistry registry;
    const auto factory = [&](const flexUI::UiNodeDefinition &) {
      ++calls;
      return std::make_unique<flexUI::ButtonWidget>();
    };
    check_false(static_cast<bool>(registry.register_widget("leaf", factory, flexUI::UiContentModel::Empty)));
    check(registry.register_widget("leaf", factory, flexUI::UiContentModel::Text).code == flexUI::WidgetRegistryErrorCode::DuplicateTag);
    check(registry.descriptor("leaf")->content == flexUI::UiContentModel::Empty);
    flexUI::UiNodeDefinition invalid;
    invalid.tag = "leaf";
    invalid.id = "subject";
    invalid.properties["text"] = std::string("invalid");
    const auto result = registry.create(invalid);
    check(result.error.code == flexUI::WidgetRegistryErrorCode::InvalidContent);
    check_equal(calls, 0);
    check(registry.register_element("bad", static_cast<flexUI::UiContentModel>(99)).code == flexUI::WidgetRegistryErrorCode::InvalidDescriptor);
    check_false(registry.contains("bad"));
  }
}
''')

with Path('flexUI/tests/test_desktop_application.cpp').open('a') as out:
    out.write(r'''

spec("FlexUI desktop content preflight runs before stylesheet admission") {
  it("reports an unknown tag before an invalid stylesheet") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Schema\">\n  <unknown id=\"root\"/>\n</ui>")
           .stylesheet("#root { widht: 90px; }");
    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(built.error.ui_error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check_equal(built.error.ui_error.line, 2);
    check_equal(built.error.ui_error.column, 4);
    check_true(built.error.css_diagnostics.empty());
  }

  it("reports forbidden leaf children before an invalid stylesheet") {
    flexUI::DesktopApplicationBuilder builder(nullptr);
    builder.xml_entry("<ui name=\"Schema\"><input id=\"root\"><label id=\"child\"/></input></ui>")
           .stylesheet("#root { widht: 90px; }");
    const auto built = builder.build();
    check_false(static_cast<bool>(built));
    check_null(built.application.get());
    check(built.error.code == flexUI::DesktopApplicationErrorCode::UiCompileFailed);
    check(built.error.stage == flexUI::DesktopApplicationStage::UiCompile);
    check(built.error.ui_error.code == flexUI::UiDocumentErrorCode::InvalidNode);
    check_true(built.error.css_diagnostics.empty());
  }
}
''')

print('Prepared current-baseline content preflight; root integration tests remain required.')
