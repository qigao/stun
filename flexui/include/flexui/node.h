#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nanovg_css.h>
#include <nanovg_css_internal.h>
#include <SDL3/SDL.h>
#include "flexui/fwd.h" // Use forward declarations

// Forward declare NVGcontext
struct NVGcontext;

namespace flexui {

// No longer need forward declaration, as it's now included
// class FlexDocument;

struct FlexNodeDesc {
  std::string id;
  std::string parent_id;
  std::string tag = "div";
  std::unordered_set<std::string> classes; // Changed to unordered_set
  std::string text;
  std::unordered_map<std::string, std::string> attributes;
  std::unordered_map<std::string, std::string> styles;
};

class FlexNode {
public:
  explicit FlexNode(FlexNodeDesc desc); // Constructor definition will be in .cpp
  virtual ~FlexNode() = default;

  const std::string &id() const { return m_desc.id; }
  const std::string &tag() const { return m_desc.tag; }
  const std::unordered_set<std::string> &classes() const { return m_desc.classes; } // Changed to unordered_set
  const std::string &text() const { return m_desc.text; }
  FlexNode* parent() const { return m_parent; }
  const std::vector<std::unique_ptr<FlexNode>> &children() const { return m_children; }
  std::vector<std::unique_ptr<FlexNode>> &children() { return m_children; }

  FlexDocument* document() const { return m_document; } // Added document() getter

  // Tree manipulation API (for FlexDocument use)
  void setDocument(FlexDocument* doc) { m_document = doc; }
  void setParent(FlexNode* parent) { m_parent = parent; }
  void addChild(std::unique_ptr<FlexNode> child) {
    if (child) {
      child->m_parent = this;
      m_children.push_back(std::move(child));
    }
  }

  // Desc manipulation API (for FlexDocument use)
  FlexNodeDesc& desc() { return m_desc; }
  const FlexNodeDesc& desc() const { return m_desc; }

public: // Expose NVGCSSElement for derived classes and internal use
  NVGCSSElement *element() const;
  void setAttribute(const std::string &name, const std::string &value); // Added setAttribute declaration

public:
  // Event handling
  virtual void handleEvent(const SDL_Event &event) {}
  virtual void update(float dt) {} // Added update method
  virtual void render(NVGcontext* vg) {}
  virtual void onDocumentAttached(FlexDocument *document) {}

  bool hasClass(const std::string &name) const {
    return m_desc.classes.count(name) > 0; // Optimized for unordered_set
  }

private:
  // No more friend class - use public API instead

  FlexNodeDesc m_desc;
  FlexDocument* m_document = nullptr;
  FlexNode* m_parent = nullptr;
  std::vector<std::unique_ptr<FlexNode>> m_children;
};

} // namespace flexui
