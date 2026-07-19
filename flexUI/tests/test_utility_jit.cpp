#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/utility_jit.h>

#include <nlohmann/json.hpp>
#include <tinytest.h>

#include <stdexcept>
#include <string>
#include <vector>

using nlohmann::json;

namespace {

json utility_fixture() {
  return json{{"tokens",
               {{"bg-accent",
                 {{"kind", "decl"},
                  {"order", 20},
                  {"decls", {{"background-color", "var(--accent)"}}}}},
                {"bg-primary",
                 {{"kind", "decl"},
                  {"order", 10},
                  {"decls", {{"background-color", "var(--primary)"}}}}},
                {"button-base",
                 {{"kind", "macro"},
                  {"expand", {"inline-flex", "rounded-md"}}}},
                {"hover:bg-accent",
                 {{"kind", "decl"},
                  {"when", {{{"type", "state"}, {"name", "hover"}}}},
                  {"decls", {{"background-color", "var(--accent)"}}}}},
                {"flex",
                 {{"kind", "decl"},
                  {"decls", {{"display", "flex"}}}}},
                {"inline-flex",
                 {{"kind", "decl"},
                  {"decls", {{"display", "inline-flex"}}}}},
                {"rounded-md",
                 {{"kind", "decl"},
                  {"decls", {{"border-radius", "0.375rem"}}}}}}}};
}

}  // namespace

spec("UtilityJit incrementally compiles deterministic CSS snapshots") {
  it("preserves class source text without breaking Symbol compatibility") {
    flexUI::Element elem;
    elem.add_class("rounded-md");
    elem.add_class(flex::Symbol("legacy-only"));

    check(elem.has_class(flex::Symbol("rounded-md")));
    check(elem.has_class(flex::Symbol("legacy-only")));
    check(elem.class_names().count("rounded-md") == 1);
    check(elem.class_names().count("legacy-only") == 0);

    elem.remove_class("rounded-md");
    check_false(elem.has_class(flex::Symbol("rounded-md")));
    elem.remove_class(flex::Symbol("legacy-only"));
    check_false(elem.has_class(flex::Symbol("legacy-only")));

    elem.add_class("bg-primary");
    elem.remove_class(flex::Symbol("bg-primary"));
    check_false(elem.has_class(flex::Symbol("bg-primary")));
    check(elem.class_names().count("bg-primary") == 0);
  }

  it("caches rules and ignores repeated tokens") {
    flexUI::tailwind::UtilityJit jit(utility_fixture());

    const auto first = jit.ensure_tokens(
        {"hover:bg-accent", "button-base", "button-base", "missing"});
    check(first.changed);
    check(first.revision == 1);
    check(first.active_tokens ==
          std::vector<std::string>({"button-base", "hover:bg-accent"}));
    check(first.added_tokens == first.active_tokens);
    check(first.missing_tokens == std::vector<std::string>({"missing"}));
    check(first.stylesheet.find(".button-base {") != std::string::npos);
    check(first.stylesheet.find(".hover\\:bg-accent:hover {") !=
          std::string::npos);
    check(jit.cached_token_count() == 2);

    const auto repeated = jit.ensure_tokens({"button-base"});
    check_false(repeated.changed);
    check(repeated.revision == first.revision);
    check(repeated.stylesheet == first.stylesheet);
    check(jit.cached_token_count() == 2);
  }

  it("uses canonical token ordering independent of discovery order") {
    flexUI::tailwind::UtilityJit lhs(utility_fixture());
    flexUI::tailwind::UtilityJit rhs(utility_fixture());

    lhs.ensure_tokens({"bg-primary"});
    const auto lhs_result = lhs.ensure_tokens({"bg-accent"});
    const auto rhs_result =
        rhs.ensure_tokens({"bg-accent", "bg-primary"});

    check(lhs_result.stylesheet == rhs_result.stylesheet);
    check(lhs_result.active_tokens == rhs_result.active_tokens);
    check(lhs_result.active_tokens ==
          std::vector<std::string>({"bg-primary", "bg-accent"}));
    check(lhs_result.stylesheet.find(".bg-primary {") <
          lhs_result.stylesheet.find(".bg-accent {"));
  }

  it("replaces a complete scan and reports removed tokens") {
    flexUI::tailwind::UtilityJit jit(utility_fixture());
    jit.ensure_tokens({"bg-accent", "bg-primary", "rounded-md"});

    const auto result = jit.replace_tokens({"bg-primary"});
    check(result.changed);
    check(result.active_tokens == std::vector<std::string>({"bg-primary"}));
    check(result.removed_tokens ==
          std::vector<std::string>({"bg-accent", "rounded-md"}));
    check(result.stylesheet.find(".bg-primary {") != std::string::npos);
    check(result.stylesheet.find(".bg-accent {") == std::string::npos);
  }

  it("atomically replaces the utility stylesheet used by the rectangle tree") {
    flexUI::tailwind::UtilityJit jit(utility_fixture());
    flexUI::Box box(nullptr, flexUI::BoxOptions::legacy_without_jit());
    auto* root = box.create("div", "root");
    root->add_class("rounded-md");
    box.set_root(root);
    box.set_viewport(100.0f, 100.0f);

    const auto initial = jit.replace_tokens({"rounded-md"});
    flexUI::CssLoadOptions load_options;
    load_options.source = "<tailwind-jit>";
    load_options.strict = true;
    const auto loaded = box.load_stylesheet(initial.stylesheet, load_options);
    check(loaded.applied);
    box.update();
    check(root->computed_style->border_radius[0] == 6.0f);

    const auto replacement = jit.replace_tokens({"bg-primary"});
    const auto replaced = box.replace_stylesheet(
        loaded.stylesheet_id, replacement.stylesheet, load_options);
    check(replaced.applied);
    box.update();
    check(root->computed_style->border_radius[0] == 0.0f);
  }

  it("lets Box scan class changes and update the utility stylesheet") {
    flexUI::Box box(nullptr, flexUI::BoxOptions::legacy_without_jit());
    auto* root = box.create("div", "root");
    auto* child = box.create("div", "child");
    child->set_classes("rounded-md unknown-utility");
    root->append(child);
    box.set_root(root);
    box.set_viewport(100.0f, 100.0f);
    box.enable_utility_jit(utility_fixture());

    box.update();
    check_true(box.utility_jit_enabled());
    check_float_eq(child->computed_style->border_radius[0], 6.0f, 0.001f);
    check_true(box.missing_utility_tokens().empty());

    child->set_classes("inline-flex");
    box.update();
    check_float_eq(child->computed_style->border_radius[0], 0.0f, 0.001f);
    check(child->computed_style->display == flexUI::Display::Flex);
    check_true(box.missing_utility_tokens().empty());

    box.disable_utility_jit();
    box.update();
    check_false(box.utility_jit_enabled());
    check(child->computed_style->display == flexUI::Display::Block);
  }

  it("enables the built-in catalog and explicit utility API by default") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    root->add_utilities("flex rounded-md bg-background");
    box.set_root(root);
    box.set_viewport(100.0f, 100.0f);

    box.update();
    check_true(box.utility_jit_enabled());
    check(root->computed_style->display == flexUI::Display::Flex);
    check_float_eq(root->computed_style->border_radius[0], 6.0f, 0.001f);
    check(root->utility_names().count("bg-background") == 1);
    check_throws_as(root->add_utility("not-in-the-built-in-catalog"),
                    std::invalid_argument);
  }

  it("applies system light and dark theme modes at the root boundary") {
    flexUI::BoxOptions options;
    options.theme = flexUI::ThemeMode::Light;
    flexUI::Box box(nullptr, options);
    auto* root = box.create("div", "root");
    root->add_utility("bg-background");
    box.set_root(root);

    check(root->attribute("data-theme") != nullptr);
    check(*root->attribute("data-theme") == "light");
    box.set_theme_mode(flexUI::ThemeMode::Dark);
    check(*root->attribute("data-theme") == "dark");
    box.set_theme_mode(flexUI::ThemeMode::System);
    check_false(root->has_attribute("data-theme"));
    flexUI::MediaEnvironment media;
    media.prefers_dark_scheme = true;
    box.set_media_environment(media);
    box.update();
    check_string_eq(root->computed_style->get_variable(flex::Symbol("--background")),
                    "#0f172a");
    check_float_eq(root->computed_style->background_color.r,
                   0x0f / 255.0f, 0.001f);
    check_float_eq(root->computed_style->background_color.g,
                   0x17 / 255.0f, 0.001f);
    check_float_eq(root->computed_style->background_color.b,
                   0x2a / 255.0f, 0.001f);
  }

  it("keeps application styles after the default JIT cascade slot") {
    flexUI::Box box(nullptr);
    box.load_css(".flex { display: none; }");
    auto* root = box.create("div", "root");
    root->add_utility("flex");
    box.set_root(root);
    box.update();
    check(root->computed_style->display == flexUI::Display::None);
  }

  it("revises only when the active utility program changes") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    root->add_utilities("flex rounded-md");
    box.set_root(root);
    box.update();

    const auto initial_revision = box.utility_jit_revision();
    check(initial_revision > 0);
    check(box.active_utility_count() == 2);
    check(box.utility_stylesheet_size() > 0);
    check(box.is_known_utility("bg-background"));
    check_false(box.is_known_utility("not-a-utility"));

    root->set_attribute("data-state", "open");
    root->set_custom_property("--accent", "#3366ff");
    box.update();
    check(box.utility_jit_revision() == initial_revision);

    root->add_utility("bg-background");
    box.update();
    check(box.utility_jit_revision() == initial_revision + 1);
    check(box.active_utility_count() == 3);
  }

  it("rejects explicit utilities when JIT is intentionally disabled") {
    flexUI::Box box(nullptr, flexUI::BoxOptions::legacy_without_jit());
    auto* root = box.create("div", "root");
    check_throws_as(root->add_utility("flex"), std::logic_error);
  }

  it("keeps the JIT stylesheet at its enable-time cascade position") {
    flexUI::Box box(nullptr);
    box.enable_utility_jit(utility_fixture());
    box.load_css(".hidden { display: none; }");

    auto* pane = box.create("div", "pane");
    pane->set_classes("flex hidden");
    box.set_root(pane);
    box.set_viewport(100.0f, 100.0f);

    box.update();
    check(pane->computed_style->display == flexUI::Display::None);
    check_false(pane->is_visible());

    pane->remove_class("hidden");
    box.update();
    check(pane->computed_style->display == flexUI::Display::Flex);
    check_true(pane->is_visible());

    pane->add_class("hidden");
    box.update();
    check(pane->computed_style->display == flexUI::Display::None);
    check_false(pane->is_visible());
  }

  it("allows later attribute state CSS to hide utility flex elements") {
    flexUI::Box box(nullptr);
    box.enable_utility_jit(utility_fixture());
    box.load_css(".tab-page[data-state=inactive] { display: none; }");

    auto* page = box.create("div", "page");
    page->set_classes("tab-page flex");
    page->set_attribute("data-state", "inactive");
    box.set_root(page);
    box.set_viewport(100.0f, 100.0f);

    box.update();
    check(page->computed_style->display == flexUI::Display::None);
    check_false(page->is_visible());

    page->set_attribute("data-state", "active");
    box.update();
    check(page->computed_style->display == flexUI::Display::Flex);
    check_true(page->is_visible());
  }

  it("rejects malformed utility registries") {
    bool threw = false;
    try {
      flexUI::tailwind::UtilityJit jit(json::object());
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    check(threw);
  }

  it("preserves the active Box JIT when reconfiguration is invalid") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    root->add_class("rounded-md");
    box.set_root(root);
    box.set_viewport(100.0f, 100.0f);
    box.enable_utility_jit(utility_fixture());
    box.update();
    check(root->computed_style->border_radius[0] == 6.0f);

    bool threw = false;
    try {
      box.enable_utility_jit(json::object());
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    check(threw);
    check(box.utility_jit_enabled());

    box.update();
    check(root->computed_style->border_radius[0] == 6.0f);
  }

  it("fails fast when configured resource limits are exceeded") {
    flexUI::tailwind::UtilityJitOptions options;
    options.max_token_length = 32;
    options.max_tokens_per_update = 2;
    options.max_active_tokens = 1;
    flexUI::tailwind::UtilityJit jit(utility_fixture(), options);

    bool long_token_threw = false;
    try {
      jit.ensure_tokens({std::string(33, 'x')});
    } catch (const std::length_error&) {
      long_token_threw = true;
    }
    check(long_token_threw);

    bool update_size_threw = false;
    try {
      jit.ensure_tokens({"missing1", "missing2", "missing3"});
    } catch (const std::length_error&) {
      update_size_threw = true;
    }
    check(update_size_threw);

    bool active_size_threw = false;
    try {
      jit.ensure_tokens({"rounded-md", "bg-accent"});
    } catch (const std::length_error&) {
      active_size_threw = true;
    }
    check(active_size_threw);
    check(jit.active_token_count() == 0);
    check(jit.revision() == 0);
  }
}
