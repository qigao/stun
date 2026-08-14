#pragma once

#include "flex/dsl/flex_ast.h"
#include <string>

namespace flex {
namespace runtime {

class AssetResolver {
public:
  virtual ~AssetResolver() = default;
  virtual std::string resolve(const std::string &base_dir,
                              const std::string &path) const = 0;
};

class DefaultAssetResolver final : public AssetResolver {
public:
  std::string resolve(const std::string &base_dir,
                      const std::string &path) const override;
};

const AssetResolver &default_asset_resolver();

std::string get_directory(const std::string &path);
std::string join_path(const std::string &dir, const std::string &file);
std::string normalize_path(const std::string &path);

void rebase_program_asset_paths(
    const std::string &base_dir,
    parser::AstProgram &program,
    const AssetResolver &resolver = default_asset_resolver());

} // namespace runtime
} // namespace flex
