#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct NVGCSSElement;

namespace flexui {

struct FlexNodeDesc {
  std::string id;
  std::string tag = "div";
  std::vector<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::unordered_map<std::string, std::string> attributes;
  std::string text;
};

class FlexNode {
public:
  explicit FlexNode(FlexNodeDesc desc) : m_desc(std::move(desc)) {}

  const std::string &id() const { return m_desc.id; }
  const std::string &tag() const { return m_desc.tag; }
  const std::vector<std::string> &classes() const { return m_desc.classes; }
  const std::unordered_map<std::string, std::string> &styles() const { return m_desc.styles; }
  const std::unordered_map<std::string, std::string> &attributes() const {
    return m_desc.attributes;
  }
  std::unordered_map<std::string, std::string> &attributes() { return m_desc.attributes; }
  const std::string &text() const { return m_desc.text; }
  FlexNode *parent() const { return m_parent; }
  const std::vector<std::unique_ptr<FlexNode>> &children() const { return m_children; }
  std::vector<std::unique_ptr<FlexNode>> &children() { return m_children; }
  NVGCSSElement *element() const { return m_element; }

  bool hasClass(const std::string &name) const {
    for (const auto &cls : m_desc.classes) {
      if (cls == name) {
        return true;
      }
    }
    return false;
  }

private:
  friend class FlexDocument;

  FlexNodeDesc m_desc;
  NVGCSSElement *m_element = nullptr;
  FlexNode *m_parent = nullptr;
  std::vector<std::unique_ptr<FlexNode>> m_children;
};

} // namespace flexui
