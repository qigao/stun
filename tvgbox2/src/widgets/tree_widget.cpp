/*
 * tvgbox2 - TreeWidget Implementation
 */

#include <tvgbox2/widgets/tree_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>

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

void TreeWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;
  Color bg_color = {255, 255, 255, 255};
  if (style) bg_color = style->get_variable_color("--tree-bg", bg_color);

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), 4, 4);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  float y = -scroll_y_;
  for (auto& root : roots_) {
    render_node(scene, elem, root.get(), y, 0);
  }
}

void TreeWidget::render_node(tvg::Scene* scene, const Element& elem, TreeNode* node, float& y, int depth) {
  if (y + item_height_ < 0) { y += item_height_; goto recurse; }
  if (y > elem.height()) return;

  {
    auto* style = elem.computed_style;
    float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
    std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
    Color text_color = {0, 0, 0, 255};
    Color selected_bg = {239, 246, 255, 255};
    Color hover_bg = {249, 250, 251, 255};
    if (style) {
      text_color = style->get_variable_color("--tree-text", text_color);
      selected_bg = style->get_variable_color("--tree-selected", selected_bg);
      hover_bg = style->get_variable_color("--tree-hover", hover_bg);
    }

    float x = depth * indent_ + 8;

    if (node == selected_) {
      auto bg = tvg::Shape::gen();
      bg->appendRect(0, y, elem.width(), item_height_);
      bg->fill(selected_bg.r, selected_bg.g, selected_bg.b, selected_bg.a);
      scene->push(std::move(bg));
    } else if (node == hover_) {
      auto bg = tvg::Shape::gen();
      bg->appendRect(0, y, elem.width(), item_height_);
      bg->fill(hover_bg.r, hover_bg.g, hover_bg.b, hover_bg.a);
      scene->push(std::move(bg));
    }

    if (!node->children.empty()) {
      auto arrow = tvg::Shape::gen();
      float ax = x + 4, ay = y + item_height_ / 2;
      if (node->expanded) {
        arrow->moveTo(ax - 4, ay - 2);
        arrow->lineTo(ax, ay + 3);
        arrow->lineTo(ax + 4, ay - 2);
      } else {
        arrow->moveTo(ax - 2, ay - 4);
        arrow->lineTo(ax + 3, ay);
        arrow->lineTo(ax - 2, ay + 4);
      }
      arrow->strokeFill(100, 100, 100, 255);
      arrow->strokeWidth(1.5f);
      arrow->strokeCap(tvg::StrokeCap::Round);
      arrow->strokeJoin(tvg::StrokeJoin::Round);
      scene->push(std::move(arrow));
    }

    auto text = tvg::Text::gen();
    text->font(font_family.c_str());
    text->size(font_size);
    text->text(node->label.c_str());
    text->fill(text_color.r, text_color.g, text_color.b);
    
    float tx, ty, tw, th;
    text->bounds(&tx, &ty, &tw, &th);
    
    // Vertical centering using bounds
    // ty is usually negative (top relative to baseline). th is height.
    // We want to align the center of the bounding box with the center of the row.
    float text_center_y = ty + th / 2.0f; 
    float row_center_y = y + item_height_ / 2.0f;
    float baseline_y = row_center_y - text_center_y;

    // Reduced gap from x+16 to x+12 for better visual spacing
    text->translate(x + 12, baseline_y);
    scene->push(text);
  }

  y += item_height_;

recurse:
  if (node->expanded) {
    for (auto& child : node->children) {
      render_node(scene, elem, child.get(), y, depth + 1);
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
