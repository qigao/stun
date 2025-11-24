#pragma once

#include "flexui/controller.h"
#include "flexui/node.h"

#include <map>
#include <string>
#include <unordered_map>


struct NVGcontext;
struct NVGCSSElement;

namespace flexui {

struct FlexSpinnerBinding {
  std::string spinner_id;
  float rotation_speed = 360.0f; // Degrees per second
  bool active = true;
};

class FlexSpinner : public FlexNode {
public:
  explicit FlexSpinner(FlexNodeDesc desc = FlexNodeDesc{}) : FlexNode(desc) {}
  void registerSpinner(FlexSpinnerBinding binding);

  void setActive(const std::string &spinner_id, bool active);
  bool isActive(const std::string &spinner_id) const;

  void onDocumentAttached(FlexDocument *document) override;
  void update(float dt) override;

private:
  struct SpinnerState {
    FlexSpinnerBinding binding;
    float rotation = 0.0f;
  };

  void updateSpinner(SpinnerState &state);
  static void DrawSpinner(NVGcontext *vg, const NVGCSSElement *element,
                          const std::map<std::string, std::string> &attributes);

  std::unordered_map<std::string, SpinnerState> m_spinners;
};

} // namespace flexui
