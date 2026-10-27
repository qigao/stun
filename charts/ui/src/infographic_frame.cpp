#include <charts/ui/infographic_frame.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/ui_document.h>
#include <flexUI/ui_xml.h>
#include <flexUI/widget.h>
#include <flexUI/widget_registry.h>
#include "infographic_frame_xml.inc"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace charts::ui {
namespace {

const flexUI::CompiledUiProgram& infographic_frame_template() {
  static auto compiled = [] {
    const auto result = flexUI::compile_ui_xml(kInfographicFrameXml);
    if (!result) {
      throw std::logic_error("invalid native Infographic XML template: " +
                             result.error.message);
    }
    return result.program;
  }();
  return *compiled;
}

std::string_view slot_name(const flexUI::UiNodeDefinition& node) {
  const auto it = node.properties.find("slot");
  if (it == node.properties.end()) return {};
  const auto *name = std::get_if<std::string>(&it->second);
  return name ? std::string_view(*name) : std::string_view{};
}

flexUI::Element* lookup_slot(const flexUI::UiDocumentTree& tree,
                             std::string_view slot) {
  // Post-commit operations must not allocate or fail.
  for (const auto& pair : tree.slots_by_name) {
    if (std::string_view(pair.first) == slot) return pair.second;
  }
  return nullptr;
}

} // namespace

InfographicFrame create_infographic_frame_with_plot(
    flexUI::Box& box, const InfographicFrameOptions& options,
    std::unique_ptr<flexUI::Widget> prepared_plot) {
  if (!prepared_plot) {
    throw std::invalid_argument("infographic requires a prepared native Plot");
  }
  if (!std::isfinite(options.width) || !std::isfinite(options.height) ||
      options.width <= 0.0f || options.height <= 0.0f ||
      options.accessible_label.empty()) {
    throw std::invalid_argument(
        "infographic requires finite positive dimensions and an accessible label");
  }

  auto definition = infographic_frame_template().definition();
  definition.root.properties["attr.aria-label"] = options.accessible_label;
  if (!options.view_id.empty()) {
    definition.root.properties["attr.data-infographic"] = options.view_id;
  }
  for (auto& child : definition.root.children) {
    const auto name = slot_name(child);
    if (name == "infographic-title") {
      child.properties["text"] = options.title;
    } else if (name == "infographic-surface") {
      child.properties["attr.aria-label"] = options.accessible_label;
      if (child.children.size() != 1 ||
          slot_name(child.children.front()) != "infographic-plot") {
        throw std::logic_error("Infographic XML has no unique Plot slot");
      }
      auto& plot = child.children.front();
      plot.properties["layout_width"] = options.width;
      plot.properties["layout_height"] = options.height;
    }
  }
  if (options.title.empty()) {
    auto& children = definition.root.children;
    children.erase(std::remove_if(children.begin(), children.end(),
          [](const flexUI::UiNodeDefinition& node) {
            return slot_name(node) == "infographic-title";
          }), children.end());
  }

  auto compiled = flexUI::compile_ui_definition(std::move(definition));
  if (!compiled) {
    throw std::invalid_argument("Infographic XML specialization failed: " +
                                compiled.error.message);
  }

  auto registry = flexUI::WidgetRegistry::builtins();
  auto pending = std::make_shared<std::unique_ptr<flexUI::Widget>>(
      std::move(prepared_plot));
  const auto registered = registry.register_widget(
      "infographic-plot",
      [pending](const flexUI::UiNodeDefinition&)
          -> std::unique_ptr<flexUI::Widget> {
        if (!*pending) {
          throw std::logic_error("infographic Plot factory invoked twice");
        }
        return std::move(*pending);
      }, flexUI::UiContentModel::Empty);
  if (registered) {
    throw std::logic_error("could not register native Infographic Plot: " +
                           registered.message);
  }

  flexUI::UiMountOptions mount;
  mount.mode = flexUI::UiMountMode::DetachedFragment;
  mount.id_policy = flexUI::UiIdPolicy::LocalOnly;
  mount.required_slots = {"infographic", "infographic-surface",
                          "infographic-plot"};
  if (!options.title.empty()) {
    mount.required_slots.emplace_back("infographic-title");
  }
  auto installed = flexUI::UiDocumentInstantiator::instantiate(
      box, *compiled.program, registry, mount);
  if (!installed) {
    throw std::invalid_argument("Infographic XML composition failed: " +
                                installed.error.message);
  }

  InfographicFrame result;
  result.root = installed.tree.root;
  result.title = lookup_slot(installed.tree, "infographic-title");
  result.surface = lookup_slot(installed.tree, "infographic-surface");
  result.plot = lookup_slot(installed.tree, "infographic-plot");
  return result;
}

} // namespace charts::ui
