/*
 * tvgbox2 - TreeWidget Implementation
 */

#include <tvgbox2/widgets/tree_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cstdio>
#include <functional>

namespace tvgbox2 {

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
  return node;
}

void TreeWidget::remove_node(const std::string& id) {
  auto remove_from = [&](std::vector<std::shared_ptr<TreeNode>>& nodes) -> bool {
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      if ((*it)->id == id) {
        nodes.erase(it);
        return true;
      }
    }
    return false;
  };

  if (remove_from(roots_)) { dirty_ = true; return; }

  for (auto& root : roots_) {
    std::function<bool(TreeNode*)> search = [&](TreeNode* node) -> bool {
      if (remove_from(node->children)) return true;
      for (auto& child : node->children) {
        if (search(child.get())) return true;
      }
      return false;
    };
    if (search(root.get())) { dirty_ = true; return; }
  }
}

void TreeWidget::clear() {
  roots_.clear();
  selected_ = nullptr;
  hover_ = nullptr;
  dirty_ = true;
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
}

void TreeWidget::expand(const std::string& id) {
  if (auto node = find_node(id)) { node->expanded = true; dirty_ = true; }
}

void TreeWidget::collapse(const std::string& id) {
  if (auto node = find_node(id)) { node->expanded = false; dirty_ = true; }
}

void TreeWidget::toggle(const std::string& id) {
  if (auto node = find_node(id)) { node->expanded = !node->expanded; dirty_ = true; }
}

void TreeWidget::render(const Element& elem, Renderer& renderer) {
  auto& r = renderer.flex();
  auto* style = elem.computed_style;
  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  if (style) bg_color = style->get_variable_color("--tree-bg", bg_color);

  r.draw_rect(0, 0, elem.width(), elem.height(), 4, Paint::solid(bg_color), Paint::none(), 0);

  float y = -scroll_y_;
  for (auto& root : roots_) {
    render_node(r, elem, root.get(), y, 0);
  }
}

void TreeWidget::render_node(flex::Renderer& r, const Element& elem, TreeNode* node, float& y, int depth) {
  if (y + item_height_ < 0) { y += item_height_; goto recurse; }
  if (y > elem.height()) return;

  {
    auto* style = elem.computed_style;
    float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
    std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
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
      r.draw_rect(0, y, elem.width(), item_height_, 0, Paint::solid(selected_bg), Paint::none(), 0);
    } else if (node == hover_) {
      r.draw_rect(0, y, elem.width(), item_height_, 0, Paint::solid(hover_bg), Paint::none(), 0);
    }

    if (!node->children.empty()) {
      float ax = x + 4, ay = y + item_height_ / 2;
      char path[128];
      if (node->expanded) {
        snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
                 ax - 4, ay - 2, ax, ay + 3, ax + 4, ay - 2);
      } else {
        snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
                 ax - 2, ay - 4, ax + 3, ay, ax - 2, ay + 4);
      }
      r.stroke_path(path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}), 1.5f);
    }

    float text_y = y + item_height_ / 2 + font_size / 3;
    r.draw_text(node->label, x + 12, text_y, font_family, font_size, false, text_color);
  }

  y += item_height_;

recurse:
  if (node->expanded) {
    for (auto& child : node->children) {
      render_node(r, elem, child.get(), y, depth + 1);
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
  float local_y = event.y - elem.absolute_y() + scroll_y_;

  if (event.type == EventType::MouseDown) {
    float current_y = 0;
    for (auto& root : roots_) {
      auto hit = hit_test(local_y, root.get(), current_y, 0);
      if (hit) {
        float local_x = event.x - elem.absolute_x();
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

} // namespace tvgbox2
