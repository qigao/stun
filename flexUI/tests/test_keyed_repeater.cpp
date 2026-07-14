#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/keyed_repeater.h>

#include <nlohmann/json.hpp>
#include <tinytest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Item {
  std::string key;
  std::string label;
};

flexUI::UiRepeaterResult reconcile_items(flexUI::UiKeyedRepeater& repeater,
                                         const std::vector<Item>& items) {
  return repeater.reconcile(
      items, [](const Item& item) { return item.key; },
      [](flexUI::Box& box, const Item& item) {
        return box.create("div", item.key);
      },
      [](flexUI::Element& element, const Item& item) {
        element.set_text(item.label);
      });
}

}  // namespace

spec("UiKeyedRepeater reconciles rectangle-tree children by stable key") {
  it("creates children in item order and updates item data") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    box.set_root(container);
    flexUI::UiKeyedRepeater repeater(box, *container);

    const auto first = reconcile_items(
        repeater, {{"alpha", "Alpha"}, {"beta", "Beta"}});
    check_size_eq(first.created, 2);
    check_size_eq(first.reused, 0);
    check_size_eq(container->child_count(), 2);
    check_ptr_eq(container->child_at(0), repeater.find("alpha"));
    check_ptr_eq(container->child_at(1), repeater.find("beta"));

    reconcile_items(repeater, {{"alpha", "Updated"}, {"beta", "Beta"}});
    check_string_eq(repeater.find("alpha")->text(), "Updated");
  }

  it("reorders active keys without replacing element identity") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}, {"beta", "Beta"}});
    auto* alpha = repeater.find("alpha");
    auto* beta = repeater.find("beta");

    const auto result =
        reconcile_items(repeater, {{"beta", "Beta 2"}, {"alpha", "Alpha 2"}});
    check_size_eq(result.created, 0);
    check_size_eq(result.reused, 2);
    check_size_eq(result.moved, 2);
    check_ptr_eq(container->child_at(0), beta);
    check_ptr_eq(container->child_at(1), alpha);
  }

  it("retains removed keys and restores their element state") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* alpha = repeater.find("alpha");
    alpha->set_attribute("data-local", "kept");

    const auto removed = reconcile_items(repeater, {});
    check_size_eq(removed.retired, 1);
    check_size_eq(repeater.active_count(), 0);
    check_size_eq(repeater.retained_count(), 1);
    check_null(alpha->parent_elem());

    const auto restored = reconcile_items(repeater, {{"alpha", "Restored"}});
    check_size_eq(restored.reused, 1);
    check_ptr_eq(repeater.find("alpha"), alpha);
    check_string_eq(*alpha->attribute("data-local"), "kept");
    check_string_eq(alpha->text(), "Restored");
  }

  it("clears focus capture and interaction state when retiring a subtree") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    box.set_root(container);
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* alpha = repeater.find("alpha");
    alpha->focusable = true;
    alpha->set_hover(true);
    alpha->set_active(true);
    box.set_focus(alpha);
    box.set_mouse_capture(alpha);

    repeater.clear();
    check_null(box.focused_element());
    check_null(box.capturing_element());
    check_false(alpha->is_focus());
    check_false(alpha->is_hover());
    check_false(alpha->is_active());
  }

  it("keeps Tailwind JIT synchronized with retired and restored subtrees") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* container = box.create("div", "list");
    root->append(container);
    box.set_root(root);
    box.set_viewport(100.0f, 100.0f);
    box.enable_utility_jit(nlohmann::json{
        {"tokens",
         {{"rounded-md",
           {{"kind", "decl"},
            {"decls", {{"border-radius", "0.375rem"}}}}}}}});
    flexUI::UiKeyedRepeater repeater(box, *container);
    const std::vector<Item> items{{"alpha", "Alpha"}};
    const auto reconcile = [&] {
      return repeater.reconcile(
          items, [](const Item& item) { return item.key; },
          [](flexUI::Box& owner, const Item& item) {
            auto* row = owner.create("div", item.key);
            row->add_class("rounded-md");
            return row;
          },
          [](flexUI::Element& row, const Item& item) {
            row.set_text(item.label);
          });
    };

    reconcile();
    auto* alpha = repeater.find("alpha");
    box.update();
    check_float_eq(alpha->computed_style->border_radius[0], 6.0f, 0.001f);

    repeater.clear();
    box.update();
    check_null(alpha->parent_elem());

    reconcile();
    box.update();
    check_ptr_eq(repeater.find("alpha"), alpha);
    check_float_eq(alpha->computed_style->border_radius[0], 6.0f, 0.001f);
  }
}

spec("UiKeyedRepeater fails fast without corrupting managed structure") {
  it("rejects empty and duplicate keys before invoking callbacks") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    int creates = 0;
    const auto run = [&](const std::vector<Item>& items) {
      return repeater.reconcile(
          items, [](const Item& item) { return item.key; },
          [&](flexUI::Box& owner, const Item&) {
            ++creates;
            return owner.create("div");
          },
          [](flexUI::Element&, const Item&) {});
    };

    check_throws_as(run({{"", "Empty"}}), std::invalid_argument);
    check_throws_as(run({{"same", "One"}, {"same", "Two"}}),
                    std::invalid_argument);
    check_int_eq(creates, 0);
    check_size_eq(container->child_count(), 0);
  }

  it("rejects capacity overflow before changing the current children") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container, 1);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* alpha = repeater.find("alpha");

    check_throws_as(
        reconcile_items(repeater, {{"alpha", "Alpha"}, {"beta", "Beta"}}),
        std::length_error);
    check_size_eq(container->child_count(), 1);
    check_ptr_eq(container->child_at(0), alpha);
  }

  it("rejects invalid create results and non-Box containers") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    const std::vector<Item> items{{"alpha", "Alpha"}};
    check_throws_as(
        repeater.reconcile(
            items, [](const Item& item) { return item.key; },
            [](flexUI::Box&, const Item&) -> flexUI::Element* { return nullptr; },
            [](flexUI::Element&, const Item&) {}),
        std::invalid_argument);
    check_size_eq(container->child_count(), 0);

    flexUI::Element unowned;
    check_throws_as(flexUI::UiKeyedRepeater(box, unowned),
                    std::invalid_argument);
  }

  it("rejects container and retained elements under a different key") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* alpha = repeater.find("alpha");
    repeater.clear();
    const std::vector<Item> items{{"beta", "Beta"}};

    check_throws_as(
        repeater.reconcile(
            items, [](const Item& item) { return item.key; },
            [&](flexUI::Box&, const Item&) { return alpha; },
            [](flexUI::Element&, const Item&) {}),
        std::invalid_argument);
    check_throws_as(
        repeater.reconcile(
            items, [](const Item& item) { return item.key; },
            [&](flexUI::Box&, const Item&) { return container; },
            [](flexUI::Element&, const Item&) {}),
        std::invalid_argument);
    check_size_eq(repeater.active_count(), 0);
    check_size_eq(repeater.retained_count(), 1);
    check_size_eq(container->child_count(), 0);
  }

  it("rejects an initially non-empty container") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    container->append(box.create("div", "existing"));
    check_throws_as(flexUI::UiKeyedRepeater(box, *container),
                    std::invalid_argument);
  }

  it("detects external direct-child changes before reconciliation") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* external = box.create("aside", "external");
    container->append(external);

    check_throws_as(reconcile_items(repeater, {{"alpha", "Alpha 2"}}),
                    std::logic_error);
    check_size_eq(container->child_count(), 2);
    check_ptr_eq(container->child_at(1), external);
  }

  it("detects direct-child changes made by callbacks before tree commit") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* alpha = repeater.find("alpha");
    const std::vector<Item> items{{"alpha", "Alpha"}, {"beta", "Beta"}};

    check_throws_as(
        repeater.reconcile(
            items, [](const Item& item) { return item.key; },
            [](flexUI::Box& owner, const Item& item) {
              return owner.create("div", item.key);
            },
            [&](flexUI::Element&, const Item& item) {
              if (item.key == "beta") {
                container->append(box.create("aside", "external"));
              }
            }),
        std::logic_error);
    check_ptr_eq(repeater.find("alpha"), alpha);
    check_size_eq(repeater.active_count(), 1);
    check_size_eq(repeater.retained_count(), 1);
    check_size_eq(container->child_count(), 2);
  }

  it("keeps the old child structure when an update callback throws") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}});
    auto* alpha = repeater.find("alpha");
    const std::vector<Item> items{{"alpha", "Mutated"}, {"beta", "Beta"}};

    check_throws_as(
        repeater.reconcile(
            items, [](const Item& item) { return item.key; },
            [](flexUI::Box& owner, const Item& item) {
              return owner.create("div", item.key);
            },
            [](flexUI::Element& element, const Item& item) {
              element.set_text(item.label);
              if (item.key == "beta") {
                throw std::runtime_error("update failed");
              }
            }),
        std::runtime_error);
    check_size_eq(container->child_count(), 1);
    check_ptr_eq(container->child_at(0), alpha);
    check_string_eq(alpha->text(), "Mutated");
    check_size_eq(repeater.active_count(), 1);
    check_size_eq(repeater.retained_count(), 1);
  }

  it("clear retires all active nodes") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    flexUI::UiKeyedRepeater repeater(box, *container);
    reconcile_items(repeater, {{"alpha", "Alpha"}, {"beta", "Beta"}});

    const auto result = repeater.clear();
    check_size_eq(result.retired, 2);
    check_size_eq(result.active, 0);
    check_size_eq(result.retained, 2);
    check_size_eq(container->child_count(), 0);
  }

  it("destructor detaches managed children and preserves external children") {
    flexUI::Box box(nullptr);
    auto* container = box.create("div", "list");
    auto* external = box.create("aside", "external");
    flexUI::Element* alpha = nullptr;
    {
      flexUI::UiKeyedRepeater repeater(box, *container);
      reconcile_items(repeater, {{"alpha", "Alpha"}});
      alpha = repeater.find("alpha");
      container->append(external);
    }

    check_null(alpha->parent_elem());
    check_size_eq(container->child_count(), 1);
    check_ptr_eq(container->child_at(0), external);
  }
}
