#pragma once

#include <flexUI/style_property_id.h>
#include <flex/core/types.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace flexUI::tailwind {

// A deliberately limited, per-element paint profile. This is not the complete
// Tailwind/NativePixel contract and does not perform backend effect admission.
inline constexpr std::string_view native_paint_profile = "stun-native-paint-v1";
inline constexpr std::string_view upstream_reference_version = "4.1.16";
inline constexpr std::string_view upstream_reference_commit =
    "cbbbe844759a35f5bd2a976bb6e66e1f7104c574";

struct NativeSourceRange {
  std::size_t offset = 0;
  std::size_t length = 0;
};

enum class NativeError {
  None,
  InvalidLimits,
  ResourceLimit,
  MalformedSyntax,
  UnsupportedUtility,
  UnsupportedVariant,
  UnsupportedValue,
  InvalidValue,
  InvalidTheme,
  UnknownThemeToken,
  PropertyTypeMismatch,
  AllocationFailure,
};

struct NativeDiagnostic {
  NativeError code = NativeError::None;
  NativeSourceRange source;
  std::string message;
};

struct NativeCompileLimits {
  std::size_t max_source_bytes = 64 * 1024;
  std::size_t max_token_bytes = 512;
  std::size_t max_tokens = 4096;  // Counted before deduplication.
  std::size_t max_rules = 4096;
  std::size_t max_variant_depth = 8;
  std::size_t max_theme_tokens = 256;
  std::size_t max_theme_name_bytes = 128;
  std::size_t max_program_bytes = 1024 * 1024;
};

struct NativeColorToken {
  std::string name;
  flex::Color value;
};

struct NativeTheme {
  // Straight-alpha sRGB values. Compilation validates and copies referenced
  // values; an immutable program never borrows this vector or its strings.
  std::vector<NativeColorToken> colors;
};

using NativeValue = std::variant<float, flex::Color>;

struct NativeDeclaration {
  flexUI::detail::StylePropertyId property = flexUI::detail::StylePropertyId::Unknown;
  NativeValue value = 0.0f;
};

enum NativeStateBit : std::uint16_t {
  NativeFocusWithin = 1u << 0,
  NativeHover = 1u << 1,
  NativeFocus = 1u << 2,
  NativeFocusVisible = 1u << 3,
  NativeActive = 1u << 4,
  NativeDisabled = 1u << 5,
  NativeDark = 1u << 6,
};

struct NativeState {
  bool focus_within = false;
  bool hover = false;
  bool focus = false;
  bool focus_visible = false;
  bool active = false;
  bool disabled = false;
  bool dark = false;  // Host-resolved effective theme, not system preference.
  bool hover_capable = false;
};

struct NativeCondition {
  std::uint16_t required = 0;
  bool matches(const NativeState& state) const noexcept;
};

struct NativeRule {
  NativeDeclaration declaration;
  NativeCondition condition;
  bool important = false;
  std::uint8_t specificity = 0;  // Supported state pseudos; dark adds none.
  std::string token;
  NativeSourceRange source;
};

class NativeProgram;
struct NativeCompileResult {
  std::shared_ptr<const NativeProgram> program;
  NativeDiagnostic diagnostic;
  explicit operator bool() const noexcept { return bool(program); }
};

NativeCompileResult compile_native_class_list(
    std::string_view classes, const NativeTheme& theme = {},
    const NativeCompileLimits& limits = {});

class NativeProgram final {
 public:
  NativeProgram(const NativeProgram&) = delete;
  NativeProgram& operator=(const NativeProgram&) = delete;
  NativeProgram(NativeProgram&&) = delete;
  NativeProgram& operator=(NativeProgram&&) = delete;

  const std::vector<NativeRule>& rules() const noexcept { return rules_; }
  // Accounted object/vector/string capacities; allocator bookkeeping is not
  // included. Compilation work is separately bounded by source/token limits.
  std::size_t retained_bytes() const noexcept { return retained_bytes_; }

 private:
  NativeProgram(std::vector<NativeRule> rules, std::size_t retained_bytes)
      : rules_(std::move(rules)), retained_bytes_(retained_bytes) {}
  std::vector<NativeRule> rules_;
  std::size_t retained_bytes_;
  friend NativeCompileResult compile_native_class_list(
      std::string_view, const NativeTheme&, const NativeCompileLimits&);
};

}  // namespace flexUI::tailwind
