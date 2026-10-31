#include <flexUI/ui_jinja_xml.h>

#include <flexUI/ui_xml.h>
#include <flexUI/widget_registry.h>

#include <jinja_cmeta.h>
#include <vstr.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace flexUI {
namespace {

struct StagedXml {
  std::string bytes;
  std::size_t max_bytes = 0;
  bool exceeded_limit = false;
  bool allocation_failed = false;
};

// Jinja's sink is not transactional: NEVER write directly into a Box,
// filesystem, terminal or renderer. All output stays private until the
// production XML compiler has accepted the complete render.
int stage_xml(const char* bytes, std::size_t length, void* opaque) noexcept {
  auto* stage = static_cast<StagedXml*>(opaque);
  if (!stage) return -1;
  // Avoid passing a null pointer to std::string::append even when length=0.
  if (length == 0) return 0;
  if (!bytes || stage->bytes.size() > stage->max_bytes) return -1;
  if (length > stage->max_bytes - stage->bytes.size()) {
    stage->exceeded_limit = true;
    return -1;
  }
  try {
    stage->bytes.append(bytes, length);
  } catch (const std::bad_alloc&) {
    stage->allocation_failed = true;
    return -1;
  } catch (...) {
    stage->allocation_failed = true;
    return -1;
  }
  return 0;
}

int escape_xml_interpolations(void*, vstr) {
  // All author templates use XML text/attribute escaping by default. No
  // untrusted template sources or host-callable registration are admitted.
  return 1;
}

std::string jinja_message(const JINJA_CMETA_ERROR& error) {
  const char* begin = error.message;
  const char* end = std::find(begin, begin + sizeof(error.message), '\0');
  return std::string(begin, end);
}

// The Jinja provider reports original byte offsets. Resolve their position
// only for template-stage errors; the XML parser reports GENERATED locations.
std::pair<int, int> original_position(std::string_view source,
                                       std::size_t offset) noexcept {
  if (offset > source.size()) return {0, 0};
  std::size_t line = 1, column = 1;
  for (std::size_t i = 0; i < offset; ++i) {
    if (source[i] == '\r') {
      ++line;
      column = 1;
      if (i + 1 < offset && source[i + 1] == '\n') ++i;
    } else if (source[i] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  if (line > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      column > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return {0, 0};
  return {static_cast<int>(line), static_cast<int>(column)};
}


/* Inverse of the canonical XML parser's xml_source_span: one byte per column,
 * including CR, with only LF advancing the line. No XML scanning/parser or
 * original Jinja source heuristics. The generated stage owns all input bytes. */
std::size_t generated_xml_byte_offset(std::string_view generated,
                                      int line, int column) noexcept {
  if (line <= 0 || column <= 0) return std::string_view::npos;
  std::size_t begin = 0;
  for (int row = 1; row < line; ++row) {
    const std::size_t lf = generated.find('\n', begin);
    if (lf == std::string_view::npos) return std::string_view::npos;
    begin = lf + 1u;
  }
  const std::size_t lf = generated.find('\n', begin);
  const std::size_t end = lf == std::string_view::npos ? generated.size() : lf;
  const std::size_t zero_based_column = static_cast<std::size_t>(column - 1);
  if (zero_based_column > end - begin) return std::string_view::npos;
  return begin + zero_based_column;
}

UiJinjaXmlCompileResult fail(
    UiJinjaXmlStage stage, std::string message,
    int template_status = 0, std::size_t offset = 0,
    UiDocumentError ui_error = {}) {
  UiJinjaXmlCompileResult result;
  result.error.stage = stage;
  result.error.template_status = template_status;
  result.error.template_offset = offset;
  result.error.ui_error = std::move(ui_error);
  result.error.message = std::move(message);
  return result;
}

} // namespace

UiJinjaXmlCompileResult compile_jinja_ui_xml(
    std::string_view template_name, std::string_view template_source,
    const cmeta_data_desc* root_desc, const void* root,
    const WidgetRegistry& registry, const UiDocumentLimits& limits) {
  const auto reject = [&](UiJinjaXmlStage stage, std::string message,
                          int template_status = 0, std::size_t offset = 0,
                          UiDocumentError ui_error = {}) {
    auto rejected = fail(stage, std::move(message), template_status, offset,
                         std::move(ui_error));
    if (!template_name.empty() &&
        template_name.size() <= limits.max_string_bytes &&
        template_name.find('\0') == std::string_view::npos)
      rejected.error.source_name.assign(template_name);
    if (stage == UiJinjaXmlStage::TemplateCompile ||
        stage == UiJinjaXmlStage::TemplateRender ||
        stage == UiJinjaXmlStage::OutputLimit) {
      const auto [line, column] = original_position(template_source, offset);
      rejected.error.template_line = line;
      rejected.error.template_column = column;
    }
    return rejected;
  };
  if (!root_desc || !root || template_name.empty() ||
      template_name.find('\0') != std::string_view::npos ||
      template_name.size() > limits.max_string_bytes) {
    return reject(UiJinjaXmlStage::InvalidInput,
                "Jinja XML requires a named trusted source and immutable CMeta root");
  }
  if (template_source.size() > limits.max_source_bytes ||
      limits.max_source_bytes == 0) {
    return reject(UiJinjaXmlStage::SourceLimit,
                "Jinja XML source exceeds the configured XML source budget");
  }

  JINJA_CMETA_ERROR error = JINJA_CMETA_ERROR_INIT;
  JINJA_CMETA_ENV_OPTIONS env_options = JINJA_CMETA_ENV_OPTIONS_INIT;
  env_options.undefined_policy = JINJA_CMETA_UNDEFINED_STRICT;
  env_options.autoescape_selector = escape_xml_interpolations;

  using Environment = std::unique_ptr<JINJA_CMETA_ENV, decltype(&jinja_cmeta_env_destroy)>;
  using Template = std::unique_ptr<JINJA_CMETA_TEMPLATE, decltype(&jinja_cmeta_release)>;

  Environment env(jinja_cmeta_env_create(&env_options, &error),
                  jinja_cmeta_env_destroy);
  if (!env) {
    return reject(UiJinjaXmlStage::TemplateCompile,
                "Jinja XML environment: " + jinja_message(error),
                static_cast<int>(error.status), error.offset);
  }
  Template compiled(
      jinja_cmeta_env_compile(
          env.get(), vstr_from_buf(template_name.data(), template_name.size()),
          vstr_from_buf(template_source.data(), template_source.size()), &error),
      jinja_cmeta_release);
  if (!compiled) {
    return reject(UiJinjaXmlStage::TemplateCompile,
                "Jinja XML source: " + jinja_message(error),
                static_cast<int>(error.status), error.offset);
  }

  StagedXml stage;
  stage.max_bytes = limits.max_source_bytes;
  JINJA_CMETA_RENDER_OPTIONS render_options = JINJA_CMETA_RENDER_OPTIONS_INIT;
  render_options.max_string_bytes = limits.max_source_bytes;
  // Bound the Jinja intermediate activation graph to the same admitted
  // node budget as the canonical XML compiler.
  render_options.max_nodes = limits.max_nodes ? limits.max_nodes : 1u;
  render_options.max_render_depth = 16;
  const JINJA_CMETA_RENDERER renderer = {stage_xml};
  const JINJA_CMETA_STATUS status = jinja_cmeta_render(
      compiled.get(), root_desc, root, &render_options, &renderer, &stage, &error);
  if (status != JINJA_CMETA_OK) {
    if (stage.exceeded_limit) {
      return reject(UiJinjaXmlStage::OutputLimit,
                  "rendered Jinja XML exceeds the configured XML source budget",
                  static_cast<int>(status), error.offset);
    }
    if (stage.allocation_failed) {
      return reject(UiJinjaXmlStage::TemplateRender,
                  "Jinja XML staging allocation failed",
                  static_cast<int>(status), error.offset);
    }
    return reject(UiJinjaXmlStage::TemplateRender,
                "Jinja XML render: " + jinja_message(error),
                static_cast<int>(status), error.offset);
  }

  // Reuse the one production XML parser and immutable UI semantic compiler.
  // In particular, an embedded NUL and XML entities are checked by that parser
  // without strlen() truncation or a second custom XML tokenizer.
  auto parsed = compile_ui_xml(
      std::string_view(stage.bytes.data(), stage.bytes.size()), limits);
  if (!parsed) {
    const std::string detail = parsed.error.message;
    auto rejected = reject(UiJinjaXmlStage::XmlCompile,
                "rendered Jinja XML rejected: " + detail,
                0, 0, std::move(parsed.error));
    rejected.error.generated_offset = generated_xml_byte_offset(
        std::string_view(stage.bytes.data(), stage.bytes.size()),
        rejected.error.ui_error.line, rejected.error.ui_error.column);
    return rejected;
  }

  // Typed tag/content preflight is required BEFORE any Widget factory or Box
  // mutation. Full property/child-role schema remains tracked in Stun #16.
  if (auto schema_error = registry.validate(*parsed.program, limits)) {
    const std::string detail = schema_error.message;
    auto rejected = reject(UiJinjaXmlStage::RegistryValidate,
                "rendered Jinja XML schema rejected: " + detail,
                0, 0, std::move(schema_error));
    rejected.error.generated_offset = generated_xml_byte_offset(
        std::string_view(stage.bytes.data(), stage.bytes.size()),
        rejected.error.ui_error.line, rejected.error.ui_error.column);
    return rejected;
  }

  UiJinjaXmlCompileResult result;
  result.program = std::move(parsed.program);
  return result;
}

} // namespace flexUI
