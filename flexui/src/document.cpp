#include "flexui/document.h"

#include <fmtlog.h>
#include <nanovg_css.h>

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
  FlexNodeDesc normalized = desc;
  normalized.id = EnsureId(desc);

  if (m_index.find(normalized.id) != m_index.end()) {
    logw("FlexDocument: element with id '{}' already exists, reusing",
                 normalized.id);
    return *m_index.at(normalized.id);
  }

  auto node = std::make_unique<FlexNode>(std::move(normalized));
  FlexNode *node_ptr = node.get();

  if (!parent_id.empty()) {
    auto *parent = findNode(parent_id);
    if (!parent) {
      throw std::runtime_error("Parent with id '" + parent_id + "' not found");
    }
    node_ptr->m_parent = parent;
    parent->children().push_back(std::move(node));
  } else {
    m_roots.push_back(std::move(node));
  }

  m_index[node_ptr->id()] = node_ptr;

  if (m_renderer) {
    attachNodeRecursive(*node_ptr);
  }

  return *node_ptr;
}

FlexNode *FlexDocument::findNode(const std::string &id) {
  auto it = m_index.find(id);
  if (it == m_index.end()) {
    return nullptr;
  }
  return it->second;
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

  auto &classes = node->m_desc.classes;
  auto it = std::find(classes.begin(), classes.end(), class_name);

  if (enabled) {
    if (it == classes.end()) {
      classes.push_back(class_name);
      if (node->element()) {
        nvgcssAddClass(node->element(), class_name.c_str());
      }
    }
  } else if (it != classes.end()) {
    classes.erase(it);
    if (node->element()) {
      nvgcssRemoveClass(node->element(), class_name.c_str());
    }
  }
}

void FlexDocument::setStyle(const std::string &id,
                            const std::string &property,
                            const std::string &value) {
  auto *node = findNode(id);
  if (!node) {
    return;
  }

  node->m_desc.styles[property] = value;
  if (node->element()) {
    nvgcssSetStyle(node->element(), property.c_str(), value.c_str());
  }
}

void FlexDocument::setText(const std::string &id, const std::string &text) {
  auto *node = findNode(id);
  if (!node) {
    return;
  }

  node->m_desc.text = text;
  if (node->element()) {
    nvgcssSetText(node->element(), text.c_str());
  }
}

void FlexDocument::setAttribute(const std::string &id,
                                const std::string &name,
                                const std::string &value) {
  auto *node = findNode(id);
  if (!node || !node->element()) {
    return;
  }

  node->m_desc.attributes[name] = value;
  nvgcssSetAttribute(node->element(), name.c_str(), value.c_str());
}

void FlexDocument::setPseudoState(const std::string &id,
                                  const std::string &state, bool enabled) {
  auto *node = findNode(id);
  if (!node || !node->element()) {
    return;
  }
  nvgcssSetPseudoState(node->element(), state.c_str(), enabled ? 1 : 0);
}

bool FlexDocument::hitTest(const std::string &id, float x, float y) const {
  auto *node = findNode(id);
  if (!node) {
    return false;
  }
  return hitTest(*node, x, y);
}

bool FlexDocument::hitTest(const FlexNode &node, float x, float y) const {
  if (!node.element()) {
    return false;
  }

  const auto &computed = node.element()->computed;
  const float left = computed.x;
  const float top = computed.y;
  const float right = left + computed.width;
  const float bottom = top + computed.height;

  return x >= left && x <= right && y >= top && y <= bottom;
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
  traverse([](FlexNode &node) { node.m_element = nullptr; });
  for (auto &root : m_roots) {
    attachNodeRecursive(*root);
  }
}

void FlexDocument::attachNode(FlexNode &node) {
  if (!m_renderer || node.element()) {
    return;
  }

  node.m_element =
      nvgcssCreateElement(m_renderer, node.id().c_str(), node.tag().c_str());

  for (const auto &cls : node.classes()) {
    nvgcssAddClass(node.m_element, cls.c_str());
  }
  for (const auto &style : node.styles()) {
    nvgcssSetStyle(node.m_element, style.first.c_str(), style.second.c_str());
  }
  for (const auto &attribute : node.attributes()) {
    nvgcssSetAttribute(node.m_element, attribute.first.c_str(),
                       attribute.second.c_str());
  }
  if (!node.text().empty()) {
    nvgcssSetText(node.m_element, node.text().c_str());
  }

  if (node.parent() && node.parent()->element()) {
    nvgcssAppendChild(m_renderer, node.parent()->element(), node.m_element);
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
