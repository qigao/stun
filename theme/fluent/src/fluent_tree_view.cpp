#include <nanogui/fluent_tree_view.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/label.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

FluentTreeView::TreeNode::TreeNode(const std::string &label, int icon)
    : label(label), icon(icon), expanded(false), selected(false) {
}

void FluentTreeView::TreeNode::add_child(const std::string &child_label, int child_icon) {
    children.emplace_back(std::make_shared<TreeNode>(label, icon));
}

FluentTreeView::FluentTreeView(Widget *parent)
    : Widget(parent), m_indent(24) {
}

std::shared_ptr<FluentTreeView::TreeNode> FluentTreeView::add_root(const std::string &label, int icon) {
    auto node = std::make_shared<TreeNode>(label, icon);
    m_roots.push_back(node);
    return node;
}

void FluentTreeView::clear() {
    m_roots.clear();
}

Vector2i FluentTreeView::preferred_size_impl(NVGcontext *ctx) const {
    int height = count_visible_nodes() * 40;
    return Vector2i(m_parent ? m_parent->width() : 300, height);
}

int FluentTreeView::count_visible_nodes() const {
    int count = 0;
    for (const auto &root : m_roots) {
        count += count_visible_nodes_recursive(root);
    }
    return count;
}

int FluentTreeView::count_visible_nodes_recursive(const std::shared_ptr<TreeNode> &node) const {
    int count = 1; // Count this node
    
    if (node->expanded) {
        for (const auto &child : node->children) {
            count += count_visible_nodes_recursive(child);
        }
    }
    
    return count;
}

bool FluentTreeView::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    int row = (p.y() - m_pos.y()) / 40;
    int current_row = 0;
    
    for (auto &root : m_roots) {
        if (handle_click_recursive(root, row, current_row, 0, p.x() - m_pos.x())) {
            return true;
        }
    }
    
    return false;
}

bool FluentTreeView::handle_click_recursive(std::shared_ptr<TreeNode> &node, int target_row, 
                                               int &current_row, int depth, int click_x) {
    if (current_row == target_row) {
        int expand_icon_x = depth * m_indent + 8;
        
        // Check if click is on expand/collapse icon
        if (!node->children.empty() && click_x >= expand_icon_x && click_x < expand_icon_x + 16) {
            node->expanded = !node->expanded;
        } else {
            // Select node
            deselect_all_recursive(m_roots);
            node->selected = true;
            
            if (m_callback)
                m_callback(node);
        }
        
        return true;
    }
    
    current_row++;
    
    if (node->expanded) {
        for (auto &child : node->children) {
            if (handle_click_recursive(child, target_row, current_row, depth + 1, click_x)) {
                return true;
            }
        }
    }
    
    return false;
}

void FluentTreeView::deselect_all_recursive(std::vector<std::shared_ptr<TreeNode>> &nodes) {
    for (auto &node : nodes) {
        node->selected = false;
        deselect_all_recursive(node->children);
    }
}

void FluentTreeView::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    int y_offset = m_pos.y();
    
    for (const auto &root : m_roots) {
        draw_node_recursive(ctx, theme, root, 0, y_offset);
    }
}

void FluentTreeView::draw_node_recursive(NVGcontext *ctx, FluentTheme *theme,
                                           const std::shared_ptr<TreeNode> &node,
                                           int depth, int &y_offset) {
    int x = m_pos.x() + depth * m_indent;
    
    // Selection background
    if (node->selected) {
        nvgBeginPath(ctx);
        nvgRect(ctx, m_pos.x(), y_offset, m_size.x(), 40);
        nvgFillColor(ctx, Color(0.9f, 0.9f, 1.0f, 1.0f));
        nvgFill(ctx);
    }
    
    // Expand/collapse icon
    if (!node->children.empty()) {
        nvgFontSize(ctx, 16.0f);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface_color());
        nvgText(ctx, x + 8, y_offset + 20, node->expanded ? "▼" : "▶", nullptr);
    }
    
    // Node icon
    if (node->icon != 0) {
        nvgFontSize(ctx, 20.0f);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, node->selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
        
        char icon_str[8];
        snprintf(icon_str, sizeof(icon_str), "%c", (char)node->icon);
        nvgText(ctx, x + 32, y_offset + 20, icon_str, nullptr);
    }
    
    // Label
    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, node->selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
    nvgText(ctx, x + (node->icon != 0 ? 56 : 32), y_offset + 20, node->label.c_str(), nullptr);
    
    y_offset += 40;
    
    // Draw children if expanded
    if (node->expanded) {
        for (const auto &child : node->children) {
            draw_node_recursive(ctx, theme, child, depth + 1, y_offset);
        }
    }
}

NAMESPACE_END(nanogui)
