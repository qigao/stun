#include "flexui/document.h"
#include "flexui/node.h"

#include <fmtlog.h>
#include <nanovg_css.h>
#include <nanovg_css_internal.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace flexui {

namespace {
std::string EnsureId(const FlexNodeDesc &desc) {
  if (!desc.id.empty()) {
    return desc.id;
  }
  static size_t auto_id = 0;
  return "flex-node-" + std::to_string(++auto_id);
}

void TraverseMutable(FlexNode &node,
                     const std::function<void(FlexNode &)> &fn) {
  fn(node);
  for (auto &child : node.children()) {
    TraverseMutable(*child, fn);
  }
}

void TraverseConst(const FlexNode &node,
                   const std::function<void(const FlexNode &)> &fn) {
  fn(node);
  for (const auto &child : node.children()) {
    TraverseConst(static_cast<const FlexNode &>(*child), fn);
  }
}
} // namespace

FlexNode &FlexDocument::appendNode(const FlexNodeDesc &desc,
                                   const std::string &parent_id) {
  // Delegate to addNode with a new FlexNode
  FlexNodeDesc normalized = desc;
  normalized.id = EnsureId(desc);
  return addNode(std::make_unique<FlexNode>(std::move(normalized)), parent_id);
}

FlexNode &FlexDocument::addNode(std::unique_ptr<FlexNode> node, const std::string &parent_id) {
    if (!node) {
        throw std::runtime_error("FlexDocument: attempted to add null node");
    }

    // Check collision
    if (m_index.find(node->id()) != m_index.end()) {
        throw std::runtime_error("FlexDocument: element with id '" + node->id() + "' already exists");
    }

    FlexNode *node_ptr = node.get();
    node_ptr->setDocument(this);

    if (!parent_id.empty()) {
        auto *parent = findNode(parent_id);
        if (!parent) {
            throw std::runtime_error("Parent with id '" + parent_id + "' not found");
        }
        node_ptr->setParent(parent);
        parent->children().push_back(std::move(node));
    } else {
        m_roots.push_back(std::move(node));
    }

    m_index[node_ptr->id()] = node_ptr;

    if (m_renderer) {
        attachNodeRecursive(*node_ptr);
    }

    // Notify the node it has been attached
    node_ptr->onDocumentAttached(this);

    return *node_ptr;
}

FlexNode *FlexDocument::findNode(const std::string &id) {
  auto it = m_index.find(id);
  if (it == m_index.end()) {
    return nullptr;
  }
  return it->second;
}

bool FlexDocument::removeNode(const std::string &id) {
  auto *node = findNode(id);
  if (!node) {
    return false;
  }

  // Remove from NVGCSSRenderer (this recursively deletes element and children)
  if (m_renderer) {
    nvgcssDeleteElement(m_renderer, id.c_str());
  }

  // Remove from index (node and all descendants)
  std::vector<std::string> ids_to_remove;
  TraverseMutable(*node, [&](FlexNode &n) { ids_to_remove.push_back(n.id()); });
  for (const auto &id_to_remove : ids_to_remove) {
    m_index.erase(id_to_remove);
  }

  // Remove from parent's children or from roots
  if (node->parent()) {
    auto &siblings = node->parent()->children();
    siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                                   [&](const auto &child) {
                                     return child->id() == id;
                                   }),
                   siblings.end());
  } else {
    m_roots.erase(std::remove_if(m_roots.begin(), m_roots.end(),
                                  [&](const auto &root) {
                                    return root->id() == id;
                                  }),
                  m_roots.end());
  }

  return true;
}

bool FlexDocument::removeChild(const std::string &parent_id,
                                const std::string &child_id) {
  auto *parent = findNode(parent_id);
  auto *child = findNode(child_id);

  if (!parent || !child) {
    return false;
  }

  // Verify child is actually a child of parent
  if (child->parent() != parent) {
    return false;
  }

  return removeNode(child_id);
}

const FlexNode *FlexDocument::findNode(const std::string &id) const {
  auto it = m_index.find(id);
  if (it == m_index.end()) {
    return nullptr;
  }
  return it->second;
}

void FlexDocument::setRenderer(NVGCSSRenderer *renderer) {
  if (m_renderer == renderer) {
    return;
  }

  m_renderer = renderer;

  if (!m_renderer) {
    return;
  }

  rebuild();
  applyStyleSheets();
  applyVariables();
}

void FlexDocument::addStyleSheet(FlexStyleSheet sheet) {
  m_stylesheets.push_back(std::move(sheet));
  if (m_renderer) {
    applyStyleSheets();
    applyVariables();
  }
}

void FlexDocument::clearStyleSheets() {
  m_stylesheets.clear();
  if (m_renderer) {
    nvgcssClearCSS(m_renderer);
    applyVariables();
  }
}

void FlexDocument::setVariable(const std::string &name,
                               const std::string &value) {
  m_variables[name] = value;
  if (m_renderer) {
    nvgcssSetVariable(m_renderer, name.c_str(), value.c_str());
  }
}

void FlexDocument::setClass(const std::string &id,
                            const std::string &class_name, bool enabled) {
  auto *node = findNode(id);
  if (!node) {
    return;
  }

  auto &classes = node->desc().classes;

  if (enabled) {
    if (classes.count(class_name) == 0) {
      classes.insert(class_name);
      if (node->element()) {
        nvgcssAddClass(node->element(), class_name.c_str());
      }
    }
  } else if (classes.count(class_name) > 0) {
    classes.erase(class_name);
    if (node->element()) {
      nvgcssRemoveClass(node->element(), class_name.c_str());
    }
  }
}

void FlexDocument::setText(const std::string &id, const std::string &text) {
  auto *node = findNode(id);
  if (!node) {
    return;
  }

  node->desc().text = text;
  if (node->element()) {
    nvgcssSetText(node->element(), text.c_str());
  }
}

void FlexDocument::setPseudoState(const std::string &id,
                                  const std::string &state, bool enabled) {
  auto *node = findNode(id);
  if (!node || !node->element()) {
    return;
  }
  nvgcssSetPseudoState(node->element(), state.c_str(), enabled ? 1 : 0);
}

void FlexDocument::setAttribute(const std::string &id,
                                const std::string &attribute,
                                const std::string &value) {
  auto *node = findNode(id);
  if (node) {
    node->setAttribute(attribute, value);
  }
}

bool FlexDocument::hitTest(const std::string &id, float x, float y) const {
  auto *node = findNode(id);
  if (!node) {
    return false;
  }
  return hitTest(*node, x, y);
}

void FlexDocument::handleEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        FlexNode* target = hitTestTopmost(event.motion.x, event.motion.y);

        if (target != m_hovered_node) {
            // Un-hover old node
            if (m_hovered_node) {
                setPseudoState(m_hovered_node->id(), "hover", false);
            }
            // Hover new node
            if (target) {
                setPseudoState(target->id(), "hover", true);
            }
            m_hovered_node = target;
        }
        
        if (m_active_node) {
            m_active_node->handleEvent(event);
        } else if (m_hovered_node) {
            m_hovered_node->handleEvent(event);
        }

    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (m_hovered_node) {
            m_active_node = m_hovered_node;
            setPseudoState(m_active_node->id(), "active", true);
            m_active_node->handleEvent(event);
        }
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (m_active_node) {
            // Click is only triggered if up happens on the same element as down
            FlexNode* target = hitTestTopmost(event.button.x, event.button.y);
            if (target == m_active_node) {
                m_active_node->handleEvent(event);
            }
            setPseudoState(m_active_node->id(), "active", false);
            m_active_node = nullptr;
        }
    }
}

bool FlexDocument::hitTest(const FlexNode &node, float x, float y) const {
  if (!m_renderer || !node.element()) {
    return false;
  }

  // Use the proper API to get computed opacity
  char opacity_str[16];
  if (nvgcssGetComputedStyle(m_renderer, node.element(), "opacity",
                             opacity_str, sizeof(opacity_str))) {
    try {
      if (std::stof(opacity_str) <= 0.0f) {
        return false; // Invisible
      }
    } catch (const std::exception&) {
      // Invalid opacity value - treat as visible
    }
  }

  const auto &computed = node.element()->computed;
  const float left = computed.x;
  const float top = computed.y;
  const float right = left + computed.width;
  const float bottom = top + computed.height;

  return x >= left && x <= right && y >= top && y <= bottom;
}

FlexNode *FlexDocument::hitTestTopmost(float x, float y) {
  std::vector<FlexNode *> hits;

  if (m_spatial_hash_enabled) {
    // Use spatial hash for fast lookup
    auto candidates = m_spatial_hash.query(x, y);
    for (const auto& id : candidates) {
      auto* node = findNode(id);
      if (node && hitTest(*node, x, y)) {
        hits.push_back(node);
      }
    }
  } else {
    // Fallback to linear search
    traverse([&](FlexNode &node) {
      if (hitTest(node, x, y)) {
        hits.push_back(&node);
      }
    });
  }

  if (hits.empty()) {
    return nullptr;
  }

  // Find the topmost element (highest z-index, then latest in document order)
  FlexNode* best_node = nullptr;
  int max_z = -2147483648; // INT_MIN

  // Iterate in reverse document order (latest painted first)
  for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
      FlexNode* node = *it;
      int z = 0;
      if (node->element()) {
          z = node->element()->explicit_style.z_index;
      }

      // Strictly greater because we are iterating backwards.
      // The first one we see with a given Z is the "last" in document order (topmost for that Z).
      // If we find another one with SAME Z later in the loop (earlier in document), 
      // we ignore it because the one we already found covers it.
      // If we find one with HIGHER Z, it covers the previous best.
      
      if (best_node == nullptr || z > max_z) {
          max_z = z;
          best_node = node;
      }
  }

  return best_node;
}

void FlexDocument::enableSpatialHash(bool enabled, float cell_size) {
  m_spatial_hash_enabled = enabled;
  if (enabled) {
    m_spatial_hash = SpatialHash(cell_size);
    updateSpatialIndex();
  } else {
    m_spatial_hash.clear();
  }
}

void FlexDocument::updateSpatialIndex() {
  if (!m_spatial_hash_enabled) {
    return;
  }

  m_spatial_hash.clear();

  traverse([&](FlexNode &node) {
    if (node.element()) {
      const auto& computed = node.element()->computed;
      Bounds bounds{computed.x, computed.y, computed.width, computed.height};
      m_spatial_hash.insert(node.id(), bounds);
    }
  });
}

void FlexDocument::traverse(const std::function<void(FlexNode &)> &callback) {
  for (auto &root : m_roots) {
    TraverseMutable(*root, callback);
  }
}

void FlexDocument::traverse(
    const std::function<void(const FlexNode &)> &callback) const {
  for (const auto &root : m_roots) {
    TraverseConst(*root, callback);
  }
}

void FlexDocument::rebuild() {
  if (!m_renderer) {
    return;
  }

  nvgcssClearElements(m_renderer);
  for (auto &root : m_roots) {
    attachNodeRecursive(*root);
  }
}

// Static custom paint function to bridge C and C++
void customPaint(NVGcontext* vg, const NVGCSSElement* element, const std::map<std::string, std::string>&) {
    if (element && element->user_data) {
        FlexNode* node = static_cast<FlexNode*>(element->user_data);
        node->render(vg);
    }
}

void FlexDocument::attachNode(FlexNode &node) {
  if (!m_renderer || node.element()) {
    return;
  }

  NVGCSSElement *elem =
      nvgcssCreateElement(m_renderer, node.id().c_str(), node.tag().c_str());

  // Store the FlexNode pointer in the user_data
  elem->user_data = &node;
  elem->custom_paint = &customPaint;

  for (const auto &cls : node.classes()) {
    nvgcssAddClass(elem, cls.c_str());
  }

  // Apply attributes (including inline styles)
  for (const auto &attr : node.desc().attributes) {
    elem->attributes[attr.first] = attr.second;
  }

  // Convert inline style attribute to CSS rule
  if (node.desc().attributes.count("style")) {
      std::string style = node.desc().attributes.at("style");
      if (!style.empty()) {
          std::string css = "#" + node.id() + " { " + style + " }";
          nvgcssParseCSS(m_renderer, css.c_str());
      }
  }

  // All styling MUST come from CSS. Do not apply inline styles or attributes.
  if (!node.text().empty()) {
    nvgcssSetText(elem, node.text().c_str());
  }

  if (node.parent() && node.parent()->element()) {
    nvgcssAppendChild(m_renderer, node.parent()->element(), elem);
  }
}

void FlexDocument::attachNodeRecursive(FlexNode &node) {
  attachNode(node);
  for (auto &child : node.children()) {
    attachNodeRecursive(*child);
  }
}

void FlexDocument::applyStyleSheets() {
  if (!m_renderer) {
    return;
  }

  nvgcssClearCSS(m_renderer);
  for (const auto &sheet : m_stylesheets) {
    if (sheet.contents.empty()) {
      continue;
    }
    if (!nvgcssParseCSS(m_renderer, sheet.contents.c_str())) {
      loge("FlexDocument: failed to parse stylesheet '{}'",
                    sheet.name);
    }
  }

  // Re-apply inline styles from all nodes, as nvgcssClearCSS wiped them
  traverse([&](FlexNode &node) {
      if (node.desc().attributes.count("style")) {
          std::string style = node.desc().attributes.at("style");
          if (!style.empty()) {
              std::string css = "#" + node.id() + " { " + style + " }";
              nvgcssParseCSS(m_renderer, css.c_str());
          }
      }
  });
}

void FlexDocument::applyVariables() {
  if (!m_renderer) {
    return;
  }

  for (const auto &entry : m_variables) {
    nvgcssSetVariable(m_renderer, entry.first.c_str(), entry.second.c_str());
  }
}

} // namespace flexui
