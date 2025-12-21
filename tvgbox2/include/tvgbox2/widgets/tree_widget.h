#ifndef TVGBOX2_TREE_WIDGET_H
#define TVGBOX2_TREE_WIDGET_H

#include "../widget.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace tvgbox2 {

struct TreeNode {
  std::string id;
  std::string label;
  bool expanded = false;
  std::vector<std::shared_ptr<TreeNode>> children;
  TreeNode* parent = nullptr;
};

class TreeWidget : public Widget {
public:
  TreeWidget();

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TreeWidget"; }

  std::shared_ptr<TreeNode> add_node(const std::string& id, const std::string& label, TreeNode* parent = nullptr);
  void remove_node(const std::string& id);
  void clear();

  TreeNode* selected_node() const { return selected_; }
  void set_selected(const std::string& id);
  void expand(const std::string& id);
  void collapse(const std::string& id);
  void toggle(const std::string& id);

  using SelectCallback = std::function<void(TreeNode* node)>;
  void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }

private:
  void render_node(tvg::Scene* scene, const Element& elem, TreeNode* node, float& y, int depth);
  TreeNode* find_node(const std::string& id, TreeNode* root = nullptr);
  TreeNode* hit_test(float y, TreeNode* root, float& current_y, int depth);

  std::vector<std::shared_ptr<TreeNode>> roots_;
  TreeNode* selected_ = nullptr;
  TreeNode* hover_ = nullptr;
  float scroll_y_ = 0;
  float item_height_ = 32.0f;
  float indent_ = 20.0f;
  SelectCallback on_select_;
};

} // namespace tvgbox2
#endif
