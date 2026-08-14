#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace flexUI::tailwind {

class UtilityCatalog {
 public:
  explicit UtilityCatalog(nlohmann::json utility_whitelist);
  ~UtilityCatalog();

  UtilityCatalog(const UtilityCatalog&) = delete;
  UtilityCatalog& operator=(const UtilityCatalog&) = delete;
  UtilityCatalog(UtilityCatalog&&) noexcept;
  UtilityCatalog& operator=(UtilityCatalog&&) noexcept;

  bool contains(std::string_view token) const;
  const nlohmann::json& definitions() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct UtilityJitOptions {
  std::size_t max_token_length = 512;
  std::size_t max_tokens_per_update = 4096;
  std::size_t max_active_tokens = 4096;
};

// Complete snapshot suitable for atomically replacing one runtime stylesheet.
struct UtilityCompileResult {
  std::string stylesheet;
  std::vector<std::string> active_tokens;
  std::vector<std::string> added_tokens;
  std::vector<std::string> removed_tokens;
  std::vector<std::string> missing_tokens;
  std::uint64_t revision = 0;
  bool changed = false;
};

/**
 * Compile Tailwind-style catalog tokens into deterministic CSS.
 *
 * @param utility_catalog JSON object containing an object-valued `tokens` field.
 * @param class_tokens Utility tokens to emit in the supplied order; duplicates
 *        are ignored.
 * @return CSS rules for known tokens. Unknown tokens emit no rule.
 * @throws std::runtime_error if the catalog shape is invalid.
 */
std::string emit_css(const nlohmann::json& utility_catalog,
                     const std::vector<std::string>& class_tokens);

/**
 * Find catalog misses without compiling CSS.
 *
 * @param utility_catalog JSON object containing an object-valued `tokens` field.
 * @param class_tokens Utility tokens to inspect.
 * @return Unique unknown tokens in first-seen order.
 * @throws std::runtime_error if the catalog shape is invalid.
 */
std::vector<std::string> find_missing_tokens(
    const nlohmann::json& utility_catalog,
    const std::vector<std::string>& class_tokens);

/**
 * Incremental immediate-mode Tailwind-style utility compiler.
 *
 * Definitions are compiled lazily and cached per token. The compiler owns no
 * UI tree or stylesheet slot; callers remain responsible for applying the
 * returned complete snapshot.
 */
class UtilityJit {
 public:
  explicit UtilityJit(nlohmann::json utility_whitelist,
                      UtilityJitOptions options = {});
  explicit UtilityJit(std::shared_ptr<const UtilityCatalog> utility_catalog,
                      UtilityJitOptions options = {});
  ~UtilityJit();

  UtilityJit(const UtilityJit&) = delete;
  UtilityJit& operator=(const UtilityJit&) = delete;
  UtilityJit(UtilityJit&&) noexcept;
  UtilityJit& operator=(UtilityJit&&) noexcept;

  UtilityCompileResult ensure_tokens(
      const std::vector<std::string>& class_tokens);
  UtilityCompileResult replace_tokens(
      const std::vector<std::string>& class_tokens);

  void reset();

  const std::string& stylesheet() const;
  std::uint64_t revision() const;
  std::size_t active_token_count() const;
  std::size_t cached_token_count() const;
  bool contains(std::string_view token) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace flexUI::tailwind
