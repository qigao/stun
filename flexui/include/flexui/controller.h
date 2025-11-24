#pragma once

#include "flexui/fwd.h"
#include "flexui/document.h"

#include <SDL3/SDL.h>

namespace flexui {

class FlexApp; // Forward declaration

class FlexController {
  friend class FlexApp; // Declare FlexApp as a friend
public:
  virtual ~FlexController() = default;

  void attachDocument(FlexDocument *document) {
    m_document = document;
    onDocumentAttached(document);
  }

  FlexDocument *getDocument() const { return m_document; }

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
