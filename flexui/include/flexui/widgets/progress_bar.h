#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexProgressBarBinding {
  std::string track_id;
  std::string fill_id;
  float initial_value = 0.0f; // 0.0 to 1.0
  std::function<void(float)> on_change;
};

class FlexProgressBar : public FlexController {
public:
  void registerProgressBar(FlexProgressBarBinding binding);

  void setValue(const std::string &track_id, float value);
  float getValue(const std::string &track_id) const;

  void onDocumentAttached(FlexDocument *document) override;
  void update(float dt) override;

private:
  struct ProgressBarState {
    FlexProgressBarBinding binding;
    float value = 0.0f;
    float target_value = 0.0f;
    bool animating = false;
  };

  void updateProgressBar(ProgressBarState &state);

  std::unordered_map<std::string, ProgressBarState> m_progress_bars;
};

} // namespace flexui
