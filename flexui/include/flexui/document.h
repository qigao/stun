#pragma once

#include "flexui/node.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct NVGCSSRenderer;

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
  FlexNode *findNode(const std::string &id);
  const FlexNode *findNode(const std::string &id) const;

  void setRenderer(NVGCSSRenderer *renderer);
  NVGCSSRenderer *renderer() const { return m_renderer; }

  void addStyleSheet(FlexStyleSheet sheet);
  void clearStyleSheets();
  void setVariable(const std::string &name, const std::string &value);

  void setClass(const std::string &id, const std::string &class_name,
                bool enabled);
  void setStyle(const std::string &id, const std::string &property,
                const std::string &value);
  void setText(const std::string &id, const std::string &text);
  void setAttribute(const std::string &id, const std::string &name,
                    const std::string &value);
  void setPseudoState(const std::string &id, const std::string &state,
                      bool enabled);

  bool hitTest(const std::string &id, float x, float y) const;
  bool hitTest(const FlexNode &node, float x, float y) const;

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
};

} // namespace flexui
