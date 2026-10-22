#include <flexUI/syntax_highlighter.h>
#include <flexUI/ts_plugin.h>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace flexUI {
bool load_ts_plugin(const std::string& path);
void load_ts_plugins_from_dir(const std::string& dir);
void unload_all_ts_plugins();
std::shared_ptr<const TSPluginInfo> acquire_ts_plugin(const std::string& name);
}
#define EXPECT(value) do { if (!(value)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << " failed: " << #value << '\n'; \
  return 1; } } while (false)

class LocalHighlighter final : public flexUI::SyntaxHighlighter {
public:
  std::vector<flexUI::HighlightToken> highlight(std::string_view source) override {
    if (source.empty()) return {};
    return {{flexUI::HighlightType::Keyword, 0u, 1u}};
  }
  const char* language() const override { return "contract-local"; }
};

static bool highlights(const std::shared_ptr<const TSPluginInfo>& lease) {
  if (!lease || !lease->highlight || !lease->free_result) return false;
  TSHighlightResult* r = lease->highlight("abc", 3u);
  if (!r) return false;
  const bool ok = r->count == 1u && r->tokens &&
                  r->tokens[0].start == 0u && r->tokens[0].length == 1u;
  lease->free_result(r);
  return ok;
}

int main(int argc, char** argv) {
  EXPECT(argc == 2);
  const std::filesystem::path path = argv[1];
  EXPECT(std::filesystem::exists(path));
#ifdef _WIN32
  EXPECT(path.extension() == ".dll");
#elif defined(__APPLE__)
  EXPECT(path.extension() == ".dylib");
#else
  EXPECT(path.extension() == ".so");
#endif

  flexUI::unload_all_ts_plugins();
  // Discovery must pick up the platform-native SHARED extension, not
  // merely work when passed a direct path.
  flexUI::load_ts_plugins_from_dir(path.parent_path().string());
  auto lease = flexUI::acquire_ts_plugin("contract");
  EXPECT(highlights(lease));
  flexUI::unload_all_ts_plugins();
  EXPECT(!flexUI::acquire_ts_plugin("contract"));
  EXPECT(highlights(lease)); // active lease survives unload request

  std::atomic<unsigned> errors{0u};
  std::thread reloader([&]() {
    for (unsigned i=0; i<200u; ++i) {
      flexUI::unload_all_ts_plugins();
      if (!flexUI::load_ts_plugin(path.string())) ++errors;
    }
  });
  std::thread reader([&]() {
    for (unsigned i=0; i<1000u; ++i) {
      auto current=flexUI::acquire_ts_plugin("contract");
      if (current && !highlights(current)) ++errors;
    }
  });
  reloader.join();
  reader.join();
  EXPECT(errors.load() == 0u);
  EXPECT(highlights(lease));

  auto& factory=flexUI::HighlighterFactory::instance();
  factory.register_highlighter("contract-local",
                              std::make_unique<LocalHighlighter>());
  auto saved=factory.acquire("contract-local");
  EXPECT(saved != nullptr);
  std::atomic<unsigned> factory_errors{0u};
  std::thread replacing([&]() {
    for (unsigned i=0; i<500u; ++i)
      factory.register_highlighter("contract-local",
                                  std::make_unique<LocalHighlighter>());
  });
  std::thread rendering([&]() {
    for (unsigned i=0; i<1000u; ++i) {
      auto current=factory.acquire("contract-local");
      if (!current) { ++factory_errors; continue; }
      const auto tokens=current->highlight("x");
      if (tokens.size()!=1u || tokens.front().length!=1u) ++factory_errors;
    }
  });
  replacing.join();
  rendering.join();
  EXPECT(factory_errors.load()==0u);
  EXPECT(saved->highlight("x").size()==1u);

  EXPECT(flexUI::load_ts_plugin(path.string()));
  auto cached=factory.acquire("contract");
  EXPECT(cached != nullptr);
  flexUI::unload_all_ts_plugins();
  EXPECT(!cached->highlight("abc").empty());
  EXPECT(highlights(lease));
  std::cout << "Highlighter platform extension and concurrent DSO leases PASS\n";
  return 0;
}
