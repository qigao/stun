#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace flexUI::tailwind {

struct UtilityJitOptions {
  std::size_t max_token_length = 512;
  std::size_t max_tokens_per_update = 4096;
  std::size_t max_active_tokens = 4096;
};

// A stylesheet snapshot produced from the currently active utility tokens.
// The snapshot is complete so callers can atomically replace one stylesheet.
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
 * Incremental Tailwind-style utility compiler.
 *
 * Utility definitions are parsed lazily and cached per token. CSS remains the
 * runtime style language: this compiler only produces a deterministic CSS
 * stylesheet and never owns or mutates the UI tree.
 */
class UtilityJit {
 public:
  explicit UtilityJit(nlohmann::json utility_whitelist,
                      UtilityJitOptions options = {});
  ~UtilityJit();

  UtilityJit(const UtilityJit&) = delete;
  UtilityJit& operator=(const UtilityJit&) = delete;
  UtilityJit(UtilityJit&&) noexcept;
  UtilityJit& operator=(UtilityJit&&) noexcept;

  // Add newly discovered tokens to the active program.
  UtilityCompileResult ensure_tokens(
      const std::vector<std::string>& class_tokens);

  // Replace the active program with the result of a complete source scan.
  UtilityCompileResult replace_tokens(
      const std::vector<std::string>& class_tokens);

  void reset();

  const std::string& stylesheet() const;
  std::uint64_t revision() const;
  std::size_t active_token_count() const;
  std::size_t cached_token_count() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace flexUI::tailwind
