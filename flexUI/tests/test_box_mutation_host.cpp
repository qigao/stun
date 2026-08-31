#include <flexUI/box.h>
#include <flexUI/box_mutation_host.h>
#include <flexUI/mutation.h>

#include <tinytest.hpp>

#include <string>

namespace {

void require_append(flexUI::UiMutationBatch& batch,
                    flexUI::UiMutation mutation) {
  check(batch.append(std::move(mutation)));
}

} // namespace

spec("FlexUI Box mutation host") {
  it("keeps prepared state private until commit or discard") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    root->set_text("before");
    flexUI::UiMutationBatch batch;
    require_append(batch,
                   flexUI::SetTextMutation{box.handle_for(*root), "after"});
    flexUI::BoxMutationHost host(box);

    auto discarded = host.prepare(batch);
    check(discarded);
    check_equal(root->text(), std::string("before"));
    discarded.prepared.reset();
    check_equal(root->text(), std::string("before"));

    auto committed = host.prepare(batch);
    check(committed);
    check_equal(root->text(), std::string("before"));
    committed.prepared->commit();
    check_equal(root->text(), std::string("after"));
    committed.prepared->commit();
    check_equal(root->text(), std::string("after"));
  }

  it("atomically commits element and existing typed input mutations") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    auto* status = box.create("label", "status");
    root->append(status);
    box.bindings().inputs().set_number("progress", 1.0);
    box.bindings().inputs().set_bool("enabled", false);
    box.bindings().inputs().set_string("message", "before");

    flexUI::UiMutationBatch batch;
    require_append(batch,
                   flexUI::SetTextMutation{box.handle_for(*status), "ready"});
    require_append(batch, flexUI::SetAttributeMutation{
                              box.handle_for(*status), "data-state", "open"});
    require_append(batch, flexUI::SetClassesMutation{
                              box.handle_for(*status), "panel active"});
    require_append(batch, flexUI::SetUtilitiesMutation{
                              box.handle_for(*status), "flex hidden"});
    require_append(
        batch, flexUI::SetBindingInputNumberMutation{"progress", 0.75});
    require_append(batch,
                   flexUI::SetBindingInputBoolMutation{"enabled", true});
    require_append(batch, flexUI::SetBindingInputStringMutation{
                              "message", "committed"});

    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine engine(host);
    const auto result = engine.apply(batch);

    check(result);
    check_equal(status->text(), std::string("ready"));
    check_equal(*status->attribute("data-state"), std::string("open"));
    check(status->has_class("panel"));
    check(status->has_class("active"));
    check(status->has_class("flex"));
    check(status->has_class("hidden"));
    check_equal(box.bindings().inputs().number("progress"), 0.75);
    check(box.bindings().inputs().boolean("enabled"));
    check_equal(box.bindings().inputs().string("message"),
                std::string("committed"));
  }

  it("rejects stale and detached targets without changing state") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    auto* attached = box.create("label", "attached");
    root->append(attached);
    attached->set_text("before");
    const auto stale = box.handle_for(*attached);
    attached->set_element_id("renamed");
    auto* detached = box.create("label", "detached");
    detached->set_text("detached-before");

    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine engine(host);
    flexUI::UiMutationBatch stale_batch;
    require_append(stale_batch,
                   flexUI::SetTextMutation{stale, "stale-write"});
    const auto stale_result = engine.apply(stale_batch);
    check_false(static_cast<bool>(stale_result));
    check(stale_result.error.code == flexUI::MutationErrorCode::InvalidTarget);
    check_equal(attached->text(), std::string("before"));

    flexUI::UiMutationBatch detached_batch;
    require_append(detached_batch, flexUI::SetTextMutation{
                                       box.handle_for(*detached), "hidden-write"});
    const auto detached_result = engine.apply(detached_batch);
    check_false(static_cast<bool>(detached_result));
    check(detached_result.error.code ==
          flexUI::MutationErrorCode::InvalidTarget);
    check_equal(detached->text(), std::string("detached-before"));
  }

  it("keeps every staged value unchanged when a utility is invalid") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    root->set_text("before");
    root->set_attribute("data-state", "closed");

    flexUI::UiMutationBatch batch;
    require_append(batch,
                   flexUI::SetTextMutation{box.handle_for(*root), "after"});
    require_append(batch, flexUI::SetAttributeMutation{
                              box.handle_for(*root), "data-state", "open"});
    require_append(batch, flexUI::SetUtilitiesMutation{
                              box.handle_for(*root), "not-in-catalog"});

    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine engine(host);
    const auto result = engine.apply(batch);

    check_false(static_cast<bool>(result));
    check(result.error.code == flexUI::MutationErrorCode::InvalidValue);
    check_equal(root->text(), std::string("before"));
    check_equal(*root->attribute("data-state"), std::string("closed"));
    check(root->utility_names().empty());
  }

  it("rejects missing or mismatched input schema before UI commit") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    root->set_text("before");
    box.bindings().inputs().set_number("count", 1.0);

    flexUI::BoxMutationHost host(box);
    flexUI::UiMutationEngine engine(host);
    flexUI::UiMutationBatch wrong_type;
    require_append(wrong_type,
                   flexUI::SetTextMutation{box.handle_for(*root), "after"});
    require_append(wrong_type,
                   flexUI::SetBindingInputStringMutation{"count", "one"});
    const auto type_result = engine.apply(wrong_type);
    check_false(static_cast<bool>(type_result));
    check(type_result.error.code == flexUI::MutationErrorCode::InvalidType);
    check_equal(root->text(), std::string("before"));
    check_equal(box.bindings().inputs().number("count"), 1.0);

    flexUI::UiMutationBatch missing;
    require_append(
        missing, flexUI::SetBindingInputBoolMutation{"unknown", true});
    const auto missing_result = engine.apply(missing);
    check_false(static_cast<bool>(missing_result));
    check(missing_result.error.code == flexUI::MutationErrorCode::InvalidName);
    check_false(box.bindings().inputs().contains("unknown"));
  }
}
