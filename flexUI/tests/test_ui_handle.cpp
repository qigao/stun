#include <flexUI/box.h>

#include <tinytest.hpp>

#include <string>

spec("FlexUI Box-owned UI handles") {
  it("resolves the current indexed element") {
    flexUI::Box box(nullptr);
    auto *element = box.create("div", "status");

    const auto handle = box.handle_for(*element);

    check(static_cast<bool>(handle));
    check_equal(handle.id, std::string("status"));
    check_equal(box.resolve_handle(handle), element);
  }

  it("invalidates a handle when its element id changes") {
    flexUI::Box box(nullptr);
    auto *element = box.create("div", "before");
    const auto before = box.handle_for(*element);

    element->set_element_id("after");
    const auto after = box.handle_for(*element);

    check_null(box.resolve_handle(before));
    check(static_cast<bool>(after));
    check(after.generation != before.generation);
    check_equal(box.resolve_handle(after), element);
  }

  it("invalidates both overwritten and restored duplicate-id handles") {
    flexUI::Box box(nullptr);
    auto *first = box.create("div", "shared");
    const auto first_before_overwrite = box.handle_for(*first);
    auto *second = box.create("div", "shared");
    const auto second_while_indexed = box.handle_for(*second);

    check_false(static_cast<bool>(box.handle_for(*first)));
    check_null(box.resolve_handle(first_before_overwrite));
    check_equal(box.resolve_handle(second_while_indexed), second);

    second->set_element_id("unique");
    const auto first_after_restore = box.handle_for(*first);

    check_null(box.resolve_handle(second_while_indexed));
    check(static_cast<bool>(first_after_restore));
    check(first_after_restore.generation != first_before_overwrite.generation);
    check_equal(box.resolve_handle(first_after_restore), first);
  }

  it("does not revive a stale handle when an id is reused") {
    flexUI::Box box(nullptr);
    auto *first = box.create("div", "item");
    const auto stale = box.handle_for(*first);

    first->set_element_id("");
    auto *replacement = box.create("div", "item");
    const auto current = box.handle_for(*replacement);

    check_null(box.resolve_handle(stale));
    check(current.generation != stale.generation);
    check_equal(box.resolve_handle(current), replacement);
  }

  it("rejects id-less, shadowed, and foreign elements") {
    flexUI::Box box(nullptr);
    auto *id_less = box.create("div");
    auto *shadowed = box.create("div", "same");
    box.create("div", "same");
    flexUI::Box foreign_box(nullptr);
    auto *foreign = foreign_box.create("div", "foreign");

    check_false(static_cast<bool>(box.handle_for(*id_less)));
    check_false(static_cast<bool>(box.handle_for(*shadowed)));
    check_false(static_cast<bool>(box.handle_for(*foreign)));
  }
}
