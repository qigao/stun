#pragma once

#include <nanovg_css.h>
#include "flexui/fwd.h" // Use forward declarations
#include "flexui/node.h" // Provides full definition of FlexNode and FlexNodeDesc
#include "flexui/spatial_hash.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexStyleSheet {
  std::string name;
  std::string contents;
};

class FlexDocument {
public:
  FlexDocument() = default;

  FlexNode &appendNode(const FlexNodeDesc &desc,
                       const std::string &parent_id = {});
  
  // New API: Add a pre-created node (allows subclasses)
  FlexNode &addNode(std::unique_ptr<FlexNode> node, const std::string &parent_id = {});

  template <typename T, typename... Args>
  T& createNode(const std::string& parent_id, Args&&... args) {
      auto node = std::make_unique<T>(std::forward<Args>(args)...);
      return static_cast<T&>(addNode(std::move(node), parent_id));
  }

  FlexNode *findNode(const std::string &id);
  const FlexNode *findNode(const std::string &id) const;

  bool removeNode(const std::string &id);
  bool removeChild(const std::string &parent_id, const std::string &child_id);

  void setRenderer(NVGCSSRenderer *renderer);
  NVGCSSRenderer *renderer() const { return m_renderer; }

  void addStyleSheet(FlexStyleSheet sheet);
  void clearStyleSheets();
  void setVariable(const std::string &name, const std::string &value);

  void setClass(const std::string &id, const std::string &class_name,
                bool enabled);
  void setText(const std::string &id, const std::string &text);
  void setPseudoState(const std::string &id, const std::string &state,
                      bool enabled);

  void setAttribute(const std::string &id, const std::string &attribute,
                    const std::string &value);

  bool hitTest(const std::string &id, float x, float y) const;
  bool hitTest(const FlexNode &node, float x, float y) const;
  FlexNode *hitTestTopmost(float x, float y);

  // Centralized event dispatch
  void handleEvent(const SDL_Event &event);

  void enableSpatialHash(bool enabled = true, float cell_size = 100.0f);
  void updateSpatialIndex();

  void traverse(const std::function<void(FlexNode &)> &callback);
  void traverse(const std::function<void(const FlexNode &)> &callback) const;

  const std::vector<std::unique_ptr<FlexNode>> &roots() const {
    return m_roots;
  }

private:
  void rebuild();
  void attachNode(FlexNode &node);
  void attachNodeRecursive(FlexNode &node);
  void applyStyleSheets();
  void applyVariables();

  std::vector<std::unique_ptr<FlexNode>> m_roots;
  std::unordered_map<std::string, FlexNode *> m_index;
  std::vector<FlexStyleSheet> m_stylesheets;
  std::unordered_map<std::string, std::string> m_variables;
  NVGCSSRenderer *m_renderer = nullptr;

  bool m_spatial_hash_enabled = false;
  SpatialHash m_spatial_hash;

  // Interaction state
  FlexNode* m_hovered_node = nullptr;
  FlexNode* m_active_node = nullptr;
};

} // namespace flexui
