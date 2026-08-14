#include <flexUI/tailwindcss.h>
#include <flexUI/utility_jit.h>

#include <nlohmann/json.hpp>
#include <tinytest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

nlohmann::json catalog() {
  return {{"tokens",
           {{"flex", {{"kind", "decl"}, {"decls", {{"display", "flex"}}}}},
            {"hover:bg-accent",
             {{"kind", "decl"},
              {"when", {{{"type", "state"}, {"name", "hover"}}}},
              {"decls", {{"background", "var(--accent)"}}}}}}}};
}

}  // namespace

spec("FlexUI TailwindCSS module is independently consumable") {
  it("emits escaped selectors and reports unknown tokens") {
    const auto definitions = catalog();
    const std::string css = flexUI::tailwind::emit_css(
        definitions, {"hover:bg-accent", "flex", "hover:bg-accent"});
    check(css.find(".hover\\:bg-accent:hover {") != std::string::npos);
    check(css.find(".flex {") != std::string::npos);

    const auto missing = flexUI::tailwind::find_missing_tokens(
        definitions, {"missing", "flex", "missing"});
    check(missing == std::vector<std::string>({"missing"}));
  }

  it("keeps the compatibility header and fail-fast catalog validation") {
    flexUI::tailwind::UtilityJit jit(catalog());
    const auto result = jit.replace_tokens({"flex"});
    check(result.changed);
    check(result.revision == 1);

    check_throws_as(flexUI::tailwind::emit_css(nlohmann::json::object(), {}),
                    std::runtime_error);
  }
}
