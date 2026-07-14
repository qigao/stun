/*
 * flexUI - TreeWidget Implementation
 */

#include <flexUI/widgets/tree_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <stb_sprintf.h>
#include <functional>

namespace flexUI {

namespace {

struct TreeBridgeStats {
  size_t node_count = 0;
  size_t expanded_count = 0;
};

void accumulate_tree_stats(const std::vector<std::shared_ptr<TreeNode>>& nodes,
                           TreeBridgeStats& stats) {
  for (const auto& node : nodes) {
    if (!node) {
      continue;
    }
    ++stats.node_count;
    if (node->expanded) {
      ++stats.expanded_count;
    }
    accumulate_tree_stats(node->children, stats);
  }
}

bool subtree_contains(TreeNode* root, const TreeNode* target) {
  if (!root || !target) {
    return false;
  }
  if (root == target) {
    return true;
  }
  for (const auto& child : root->children) {
    if (subtree_contains(child.get(), target)) {
      return true;
    }
  }
  return false;
}

void sync_tree_host(Element* host,
                    const std::vector<std::shared_ptr<TreeNode>>& roots,
                    const TreeNode* selected) {
  if (!host) {
    return;
  }

  TreeBridgeStats stats;
  accumulate_tree_stats(roots, stats);
  const bool has_selection = selected != nullptr;

  host->set_attribute("role", "tree");
  host->set_attribute("data-state", stats.node_count == 0
                                        ? "empty"
                                        : has_selection ? "selected"
                                                        : stats.expanded_count > 0
                                                              ? "expanded"
                                                              : "idle");
  host->set_attribute("data-node-count", std::to_string(stats.node_count));
  host->set_attribute("data-expanded-count",
                      std::to_string(stats.expanded_count));
  host->set_attribute("data-selected-count", has_selection ? "1" : "0");
  host->set_attribute("aria-multiselectable", "false");

  if (has_selection) {
    host->set_attribute("data-selected-id", selected->id);
    host->set_attribute("aria-activedescendant", selected->id);
  } else {
    host->remove_attribute("data-selected-id");
    host->remove_attribute("aria-activedescendant");
  }

  host->set_state("selected", has_selection);
  host->set_state("open", stats.expanded_count > 0);
}

} // namespace

TreeWidget::TreeWidget() {}

std::shared_ptr<TreeNode> TreeWidget::add_node(const std::string& id, const std::string& label, TreeNode* parent) {
  auto node = std::make_shared<TreeNode>();
  node->id = id;
  node->label = label;
  node->parent = parent;

  if (parent) {
    parent->children.push_back(node);
  } else {
    roots_.push_back(node);
  }
  dirty_ = true;
  sync_tree_host(host_element(), roots_, selected_);
  return node;
}

void TreeWidget::remove_node(const std::string& id) {
  auto remove_from = [&](std::vector<std::shared_ptr<TreeNode>>& nodes) -> bool {
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      if ((*it)->id == id) {
        TreeNode* removed = it->get();
        if (subtree_contains(removed, selected_)) {
          selected_ = nullptr;
        }
        if (subtree_contains(removed, hover_)) {
          hover_ = nullptr;
        }
        nodes.erase(it);
        return true;
      }
    }
    return false;
  };

  if (remove_from(roots_)) {
    dirty_ = true;
    sync_tree_host(host_element(), roots_, selected_);
    return;
  }

  for (auto& root : roots_) {
    std::function<bool(TreeNode*)> search = [&](TreeNode* node) -> bool {
      if (remove_from(node->children)) return true;
      for (auto& child : node->children) {
        if (search(child.get())) return true;
      }
      return false;
    };
    if (search(root.get())) {
      dirty_ = true;
      sync_tree_host(host_element(), roots_, selected_);
      return;
    }
  }
}

void TreeWidget::clear() {
  roots_.clear();
  selected_ = nullptr;
  hover_ = nullptr;
  dirty_ = true;
  sync_tree_host(host_element(), roots_, selected_);
}

TreeNode* TreeWidget::find_node(const std::string& id, TreeNode* root) {
  if (!root) {
    for (auto& r : roots_) {
      if (r->id == id) return r.get();
      auto found = find_node(id, r.get());
      if (found) return found;
    }
    return nullptr;
  }

  for (auto& child : root->children) {
    if (child->id == id) return child.get();
    auto found = find_node(id, child.get());
    if (found) return found;
  }
  return nullptr;
}

void TreeWidget::set_selected(const std::string& id) {
  selected_ = find_node(id);
  dirty_ = true;
  sync_tree_host(host_element(), roots_, selected_);
}

void TreeWidget::expand(const std::string& id) {
  if (auto node = find_node(id)) {
    node->expanded = true;
    dirty_ = true;
    sync_tree_host(host_element(), roots_, selected_);
  }
}

void TreeWidget::collapse(const std::string& id) {
  if (auto node = find_node(id)) {
    node->expanded = false;
    dirty_ = true;
    sync_tree_host(host_element(), roots_, selected_);
  }
}

void TreeWidget::toggle(const std::string& id) {
  if (auto node = find_node(id)) {
    node->expanded = !node->expanded;
    dirty_ = true;
    sync_tree_host(host_element(), roots_, selected_);
  }
}

void TreeWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  auto* style = elem.computed_style;
  sync_tree_host(host_element(), roots_, selected_);
  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  if (style) bg_color = style->get_variable_color("--tree-bg", bg_color);

  commands.draw_rect(0, 0, elem.width(), elem.height(), 4,
                     Paint::solid(bg_color), Paint::none(), 0);

  float y = -scroll_y_;
  for (auto& root : roots_) {
    render_node(commands, elem, root.get(), y, 0);
  }
}

void TreeWidget::render_node(RenderCommandList& commands, const Element& elem, TreeNode* node, float& y, int depth) {
  if (y + item_height_ < 0) { y += item_height_; goto recurse; }
  if (y > elem.height()) return;

  {
    auto* style = elem.computed_style;
    Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
    Color selected_bg = {0.94f, 0.96f, 1.0f, 1.0f};
    Color hover_bg = {0.98f, 0.98f, 0.98f, 1.0f};
    if (style) {
      text_color = style->get_variable_color("--tree-text", text_color);
      selected_bg = style->get_variable_color("--tree-selected", selected_bg);
      hover_bg = style->get_variable_color("--tree-hover", hover_bg);
    }

    float x = depth * indent_ + 8;

    if (node == selected_) {
      commands.draw_rect(0, y, elem.width(), item_height_, 0,
                         Paint::solid(selected_bg), Paint::none(), 0);
    } else if (node == hover_) {
      commands.draw_rect(0, y, elem.width(), item_height_, 0,
                         Paint::solid(hover_bg), Paint::none(), 0);
    }

    if (!node->children.empty()) {
      float ax = x + 4, ay = y + item_height_ / 2;
      char path[128];
      if (node->expanded) {
        stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
                 ax - 4, ay - 2, ax, ay + 3, ax + 4, ay - 2);
      } else {
        stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
                 ax - 2, ay - 4, ax + 3, ay, ax - 2, ay + 4);
      }
      commands.stroke_path(path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}), 1.5f);
    }
    if (style) {
      const auto text_block = layout_text_block(
          style, node->label, x + 12.0f, y,
          std::max(0.0f, elem.width() - x - 20.0f), item_height_, text_color,
          TextVerticalAlign::Middle);
      emit_text_block(commands, text_block);
    }
  }

  y += item_height_;

recurse:
  if (node->expanded) {
    for (auto& child : node->children) {
      render_node(commands, elem, child.get(), y, depth + 1);
    }
  }
}

TreeNode* TreeWidget::hit_test(float y, TreeNode* root, float& current_y, int depth) {
  if (y >= current_y && y < current_y + item_height_) {
    return root;
  }
  current_y += item_height_;

  if (root->expanded) {
    for (auto& child : root->children) {
      auto hit = hit_test(y, child.get(), current_y, depth + 1);
      if (hit) return hit;
    }
  }
  return nullptr;
}

bool TreeWidget::handle_event(const Event& event, Element& elem) {
  const flex::Vec2 local_pos =
      detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
  float local_y = local_pos.y + scroll_y_;

  if (event.type == EventType::MouseDown) {
    float current_y = 0;
    for (auto& root : roots_) {
      auto hit = hit_test(local_y, root.get(), current_y, 0);
      if (hit) {
        float local_x = local_pos.x;
        int depth = 0;
        for (auto p = hit->parent; p; p = p->parent) depth++;
        float arrow_x = depth * indent_ + 8;

        if (!hit->children.empty() && local_x >= arrow_x && local_x < arrow_x + 16) {
          hit->expanded = !hit->expanded;
        } else {
          selected_ = hit;
          if (on_select_) on_select_(hit);
        }
        dirty_ = true;
        sync_tree_host(host_element(), roots_, selected_);
        elem.mark_paint_dirty();
        return true;
      }
    }
  }

  if (event.type == EventType::MouseMove) {
    float current_y = 0;
    TreeNode* new_hover = nullptr;
    for (auto& root : roots_) {
      new_hover = hit_test(local_y, root.get(), current_y, 0);
      if (new_hover) break;
    }
    if (new_hover != hover_) {
      hover_ = new_hover;
      elem.mark_paint_dirty();
    }
  }

  if (event.type == EventType::MouseWheel) {
    float total_height = 0;
    std::function<void(TreeNode*)> count = [&](TreeNode* n) {
      total_height += item_height_;
      if (n->expanded) for (auto& c : n->children) count(c.get());
    };
    for (auto& r : roots_) count(r.get());

    float max_scroll = std::max(0.0f, total_height - elem.height());
    scroll_y_ = std::clamp(scroll_y_ - event.delta_y * 30, 0.0f, max_scroll);
    elem.mark_paint_dirty();
    return true;
  }

  return false;
}

void TreeWidget::update(float delta_ms, Element& elem) {}

} // namespace flexUI
