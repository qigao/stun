#pragma once

#include <memory>

namespace flex {
class Group;
class Instance;
}

namespace flexUI {
class Widget;
}

namespace charts::ui {

/**
 * Adapt a legacy Flex node tree into flexUI's backend-neutral command stream.
 *
 * The widget owns the Instance lifetime. The supplied Group must be allocated
 * by that Instance and must not already be attached to another Scene.
 */
std::unique_ptr<flexUI::Widget> create_flex_node_plot_widget(
    std::shared_ptr<flex::Instance> instance,
    flex::Group* plot,
    float width,
    float height,
    bool perform_layout = true);

}  // namespace charts::ui
