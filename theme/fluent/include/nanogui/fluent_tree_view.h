#pragma once

#include <nanogui/widget.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class FluentTheme;

/**
 * @brief Fluent Design Tree View
 * 
 * Displays hierarchical data in an expandable tree structure.
 * Supports icons, selection, and expand/collapse.
 */
class NANOGUI_EXPORT FluentTreeView : public Widget {
public:
    struct TreeNode {
        std::string label;
        int icon;
        bool expanded;
        bool selected;
        std::vector<std::shared_ptr<TreeNode>> children;
        
        TreeNode(const std::string &label, int icon = 0);
        
        /// Add child node
        void add_child(const std::string &label, int icon = 0);
    };
    
    FluentTreeView(Widget *parent);
    
    /// Add root node
    std::shared_ptr<TreeNode> add_root(const std::string &label, int icon = 0);
    
    /// Clear all nodes
    void clear();
    
    /// Indent per level
    int indent() const { return m_indent; }
    void set_indent(int indent) { m_indent = indent; }
    
    /// Selection callback
    std::function<void(std::shared_ptr<TreeNode>)> callback() const { return m_callback; }
    void set_callback(const std::function<void(std::shared_ptr<TreeNode>)> &callback) { 
        m_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    int count_visible_nodes() const;
    int count_visible_nodes_recursive(const std::shared_ptr<TreeNode> &node) const;
    bool handle_click_recursive(std::shared_ptr<TreeNode> &node, int target_row, 
                                int &current_row, int depth, int click_x);
    void deselect_all_recursive(std::vector<std::shared_ptr<TreeNode>> &nodes);
    void draw_node_recursive(NVGcontext *ctx, FluentTheme *theme,
                            const std::shared_ptr<TreeNode> &node,
                            int depth, int &y_offset);
    
    std::vector<std::shared_ptr<TreeNode>> m_roots;
    int m_indent;
    std::function<void(std::shared_ptr<TreeNode>)> m_callback;
};

NAMESPACE_END(nanogui)
