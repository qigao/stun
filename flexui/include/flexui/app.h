#pragma once

#include "flexui/controller.h"
#include "flexui/view.h"

#include <memory>
#include <vector>

namespace flexui {

struct FlexAppConfig {
  FlexViewConfig view;
  bool quit_on_close = true;
};

class FlexApp {
public:
  bool init(const FlexAppConfig &config);

  void setDocument(std::shared_ptr<FlexDocument> document);
  std::shared_ptr<FlexDocument> document() const { return m_document; }

  void addController(const std::shared_ptr<Flex> &controller);

  void setOverlayCallback(OverlayDrawCallback callback);

  void run();

  FlexView &view() { return m_view; }

private:
  FlexAppConfig m_config;
  FlexView m_view;
  std::shared_ptr<FlexDocument> m_document;
  std::vector<std::shared_ptr<Flex>> m_controllers;
  OverlayDrawCallback m_overlay_callback;
};

} // namespace flexui
