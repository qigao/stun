#include "flexui/node.h"
#include "flexui/document.h" // For FlexDocument definition
#include <nanovg_css_internal.h>

namespace flexui {

FlexNode::FlexNode(FlexNodeDesc desc)
    : m_desc(std::move(desc))
{
    if (!m_desc.styles.empty()) {
        std::string styleStr;
        if (m_desc.attributes.count("style")) {
             styleStr = m_desc.attributes["style"];
             if (!styleStr.empty() && styleStr.back() != ';') {
                 styleStr += "; ";
             }
        }
        for (const auto& [key, value] : m_desc.styles) {
            styleStr += key + ": " + value + "; ";
        }
        m_desc.attributes["style"] = styleStr;
    }
}

NVGCSSElement *FlexNode::element() const {
  if (m_document) {
    if (m_document->renderer()) {
      return nvgcssGetElement(m_document->renderer(), m_desc.id.c_str());
    }
  }
  return nullptr;
}

void FlexNode::setAttribute(const std::string &name, const std::string &value) {
  m_desc.attributes[name] = value;
  if (m_document) {
    if (m_document->renderer()) {
      if (auto* elem = nvgcssGetElement(m_document->renderer(), m_desc.id.c_str())) {
          elem->attributes[name] = value;
      }
      nvgcssComputeLayout(m_document->renderer());
    }
  }
}

} // namespace flexui
