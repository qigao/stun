#include "asset_resolver.h"

#include <cctype>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

namespace flex {
namespace runtime {
namespace {

bool is_ascii_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_absolute_path(const std::string &path) {
  if (path.empty()) {
    return false;
  }
  if (path[0] == '/' || path[0] == '\\') {
    return true;
  }
  return path.size() >= 2 && is_ascii_alpha(path[0]) && path[1] == ':';
}

bool has_uri_scheme(const std::string &path) {
  size_t colon = path.find(':');
  if (colon == std::string::npos || colon == 0) {
    return false;
  }

  size_t slash = path.find_first_of("/\\");
  if (slash != std::string::npos && colon > slash) {
    return false;
  }

  for (size_t i = 0; i < colon; ++i) {
    unsigned char c = static_cast<unsigned char>(path[i]);
    if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') {
      return false;
    }
  }
  return true;
}

} // namespace

std::string get_directory(const std::string &path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return ".";
  }
  return path.substr(0, pos);
}

std::string join_path(const std::string &dir, const std::string &file) {
  if (dir.empty() || dir == ".") {
    return file;
  }
  char last = dir.back();
  if (last == '/' || last == '\\') {
    return dir + file;
  }
  return dir + PATH_SEP + file;
}

std::string normalize_path(const std::string &path) {
  std::string result = path;
  for (char &c : result) {
    if (c == '\\') c = '/';
  }
  return result;
}

std::string DefaultAssetResolver::resolve(const std::string &base_dir,
                                          const std::string &path) const {
  if (path.empty() || is_absolute_path(path) || has_uri_scheme(path)) {
    return path;
  }
  return normalize_path(join_path(base_dir, path));
}

const AssetResolver &default_asset_resolver() {
  static DefaultAssetResolver resolver;
  return resolver;
}

void rebase_program_asset_paths(const std::string &base_dir,
                                parser::AstProgram &program,
                                const AssetResolver &resolver) {
  if (!program.assets) {
    return;
  }

  for (auto &asset : program.assets->assets) {
    asset.path = resolver.resolve(base_dir, asset.path);
  }
}

} // namespace runtime
} // namespace flex
