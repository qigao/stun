#include <flexUI/utility_jit.h>

#include <flexUI/shadcn_ir.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace flexUI::tailwind {

namespace {

using TokenSet = std::set<std::string>;

TokenSet unique_tokens(const std::vector<std::string>& class_tokens,
                       const UtilityJitOptions& options) {
  if (class_tokens.size() > options.max_tokens_per_update) {
    throw std::length_error("utility update exceeds max_tokens_per_update");
  }

  TokenSet result;
  for (const auto& token : class_tokens) {
    if (token.size() > options.max_token_length) {
      throw std::length_error("utility token exceeds max_token_length");
    }
    if (!token.empty()) {
      result.insert(token);
    }
  }
  return result;
}

std::vector<std::string> set_difference(const TokenSet& lhs,
                                        const TokenSet& rhs) {
  std::vector<std::string> result;
  std::set_difference(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                      std::back_inserter(result));
  return result;
}

}  // namespace

struct UtilityCatalog::Impl {
  explicit Impl(nlohmann::json value) : utility_whitelist(std::move(value)) {
    if (!utility_whitelist.contains("tokens") ||
        !utility_whitelist.at("tokens").is_object()) {
      throw std::invalid_argument(
          "utility whitelist must contain an object named 'tokens'");
    }
  }

  nlohmann::json utility_whitelist;
};

UtilityCatalog::UtilityCatalog(nlohmann::json utility_whitelist)
    : impl_(std::make_unique<Impl>(std::move(utility_whitelist))) {}

UtilityCatalog::~UtilityCatalog() = default;
UtilityCatalog::UtilityCatalog(UtilityCatalog&&) noexcept = default;
UtilityCatalog& UtilityCatalog::operator=(UtilityCatalog&&) noexcept = default;

bool UtilityCatalog::contains(std::string_view token) const {
  return impl_->utility_whitelist.at("tokens").contains(std::string(token));
}

const nlohmann::json& UtilityCatalog::definitions() const {
  return impl_->utility_whitelist;
}

struct UtilityJit::Impl {
  Impl(std::shared_ptr<const UtilityCatalog> catalog_value,
       UtilityJitOptions limits)
      : utility_catalog(std::move(catalog_value)), options(limits) {
    if (!utility_catalog) {
      throw std::invalid_argument("utility catalog must not be null");
    }
    if (options.max_token_length == 0 ||
        options.max_tokens_per_update == 0 ||
        options.max_active_tokens == 0) {
      throw std::invalid_argument("utility JIT limits must be non-zero");
    }
  }

  std::string compile_token(const std::string& token) {
    const auto cached = rule_cache.find(token);
    if (cached != rule_cache.end()) {
      return cached->second;
    }

    std::string css =
        shadcn_ir::emit_utility_css(utility_catalog->definitions(), {token});
    if (css.empty()) {
      throw std::runtime_error("utility token emitted no CSS: " + token);
    }
    rule_cache.emplace(token, css);
    return css;
  }

  std::vector<std::string> ordered_tokens(const TokenSet& tokens) const {
    std::vector<std::string> result(tokens.begin(), tokens.end());
    const auto& definitions = utility_catalog->definitions().at("tokens");
    std::sort(result.begin(), result.end(), [&definitions](const auto& lhs,
                                                           const auto& rhs) {
      const auto lhs_it = definitions.find(lhs);
      const auto rhs_it = definitions.find(rhs);
      const std::int64_t lhs_order =
          lhs_it != definitions.end() ? lhs_it->value("order", 0) : 0;
      const std::int64_t rhs_order =
          rhs_it != definitions.end() ? rhs_it->value("order", 0) : 0;
      return lhs_order != rhs_order ? lhs_order < rhs_order : lhs < rhs;
    });
    return result;
  }

  UtilityCompileResult update(const std::vector<std::string>& class_tokens,
                              bool replace_active) {
    const TokenSet requested = unique_tokens(class_tokens, options);
    const auto missing =
        shadcn_ir::missing_utility_tokens(
            utility_catalog->definitions(),
            std::vector<std::string>(requested.begin(), requested.end()));
    const std::unordered_set<std::string> missing_set(missing.begin(),
                                                       missing.end());

    TokenSet valid_requested;
    for (const auto& token : requested) {
      if (missing_set.count(token) == 0) {
        valid_requested.insert(token);
      }
    }

    TokenSet next = replace_active ? valid_requested : active_tokens;
    if (!replace_active) {
      next.insert(valid_requested.begin(), valid_requested.end());
    }
    if (next.size() > options.max_active_tokens) {
      throw std::length_error("utility program exceeds max_active_tokens");
    }

    UtilityCompileResult result;
    result.added_tokens = set_difference(next, active_tokens);
    result.removed_tokens = set_difference(active_tokens, next);
    result.missing_tokens = missing;
    result.changed = next != active_tokens;

    if (result.changed) {
      std::string next_stylesheet;
      for (const auto& token : ordered_tokens(next)) {
        next_stylesheet += compile_token(token);
      }
      active_tokens = std::move(next);
      compiled_stylesheet = std::move(next_stylesheet);
      ++program_revision;
    }

    result.stylesheet = compiled_stylesheet;
    result.active_tokens = ordered_tokens(active_tokens);
    result.revision = program_revision;
    return result;
  }

  std::shared_ptr<const UtilityCatalog> utility_catalog;
  UtilityJitOptions options;
  TokenSet active_tokens;
  std::unordered_map<std::string, std::string> rule_cache;
  std::string compiled_stylesheet;
  std::uint64_t program_revision = 0;
};

UtilityJit::UtilityJit(nlohmann::json utility_whitelist,
                       UtilityJitOptions options)
    : UtilityJit(
          std::make_shared<const UtilityCatalog>(std::move(utility_whitelist)),
          options) {}

UtilityJit::UtilityJit(
    std::shared_ptr<const UtilityCatalog> utility_catalog,
    UtilityJitOptions options)
    : impl_(std::make_unique<Impl>(std::move(utility_catalog), options)) {}

UtilityJit::~UtilityJit() = default;
UtilityJit::UtilityJit(UtilityJit&&) noexcept = default;
UtilityJit& UtilityJit::operator=(UtilityJit&&) noexcept = default;

UtilityCompileResult UtilityJit::ensure_tokens(
    const std::vector<std::string>& class_tokens) {
  return impl_->update(class_tokens, false);
}

UtilityCompileResult UtilityJit::replace_tokens(
    const std::vector<std::string>& class_tokens) {
  return impl_->update(class_tokens, true);
}

void UtilityJit::reset() {
  const bool program_changed = !impl_->active_tokens.empty();
  impl_->active_tokens.clear();
  impl_->rule_cache.clear();
  impl_->compiled_stylesheet.clear();
  if (program_changed) {
    ++impl_->program_revision;
  }
}

const std::string& UtilityJit::stylesheet() const {
  return impl_->compiled_stylesheet;
}

std::uint64_t UtilityJit::revision() const {
  return impl_->program_revision;
}

std::size_t UtilityJit::active_token_count() const {
  return impl_->active_tokens.size();
}

std::size_t UtilityJit::cached_token_count() const {
  return impl_->rule_cache.size();
}

bool UtilityJit::contains(std::string_view token) const {
  return impl_->utility_catalog->contains(token);
}

}  // namespace flexUI::tailwind
