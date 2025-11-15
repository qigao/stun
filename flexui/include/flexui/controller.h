#pragma once

#include "flexui/document.h"

#include <SDL3/SDL.h>

namespace flexui {

class Flex {
public:
  virtual ~Flex() = default;

  void attachDocument(FlexDocument *document) {
    m_document = document;
    onDocumentAttached(document);
  }

  FlexDocument *document() const { return m_document; }

  virtual void onDocumentAttached(FlexDocument *document) { (void)document; }
  virtual void onBeforeRun() {}
  virtual void update(float dt) { (void)dt; }
  virtual void handleEvent(const SDL_Event &event) { (void)event; }

protected:
  void requestLayout() {}

private:
  FlexDocument *m_document = nullptr;
};

} // namespace flexui
