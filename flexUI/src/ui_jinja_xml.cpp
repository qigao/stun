#include <flexUI/ui_jinja_xml.h>

#include <flexUI/ui_xml.h>
#include <flexUI/widget_registry.h>

#include <jinja_cmeta.h>
#include <vstr.h>

#include <algorithm>
#include <memory>
#include <new>
#include <string>
#include <string_view>

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
  if (!root_desc || !root || template_name.empty() ||
      template_name.find('\0') != std::string_view::npos ||
      template_name.size() > limits.max_string_bytes) {
    return fail(UiJinjaXmlStage::InvalidInput,
                "Jinja XML requires a named trusted source and immutable CMeta root");
  }
  if (template_source.size() > limits.max_source_bytes ||
      limits.max_source_bytes == 0) {
    return fail(UiJinjaXmlStage::SourceLimit,
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
    return fail(UiJinjaXmlStage::TemplateCompile,
                "Jinja XML environment: " + jinja_message(error),
                static_cast<int>(error.status), error.offset);
  }
  Template compiled(
      jinja_cmeta_env_compile(
          env.get(), vstr_from_buf(template_name.data(), template_name.size()),
          vstr_from_buf(template_source.data(), template_source.size()), &error),
      jinja_cmeta_release);
  if (!compiled) {
    return fail(UiJinjaXmlStage::TemplateCompile,
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
      return fail(UiJinjaXmlStage::OutputLimit,
                  "rendered Jinja XML exceeds the configured XML source budget",
                  static_cast<int>(status), error.offset);
    }
    if (stage.allocation_failed) {
      return fail(UiJinjaXmlStage::TemplateRender,
                  "Jinja XML staging allocation failed",
                  static_cast<int>(status), error.offset);
    }
    return fail(UiJinjaXmlStage::TemplateRender,
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
    return fail(UiJinjaXmlStage::XmlCompile,
                "rendered Jinja XML rejected: " + detail,
                0, 0, std::move(parsed.error));
  }

  // Typed tag/content preflight is required BEFORE any Widget factory or Box
  // mutation. Full property/child-role schema remains tracked in Stun #16.
  if (auto schema_error = registry.validate(*parsed.program, limits)) {
    const std::string detail = schema_error.message;
    return fail(UiJinjaXmlStage::RegistryValidate,
                "rendered Jinja XML schema rejected: " + detail,
                0, 0, std::move(schema_error));
  }

  UiJinjaXmlCompileResult result;
  result.program = std::move(parsed.program);
  return result;
}

} // namespace flexUI
