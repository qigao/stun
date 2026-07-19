#pragma once

#include <string>

namespace flexUI {
class Box;
class Element;
}

namespace flex::modules::infographic {

class UnifiedInfographic;

struct InfographicViewOptions {
  float width = 600.0f;
  float height = 500.0f;
  std::string view_id;
  std::string accessible_label = "Infographic";
};

struct FlexUiInfographicResult {
  flexUI::Element* root = nullptr;
  flexUI::Element* plot = nullptr;
  std::string error;

  explicit operator bool() const noexcept { return root != nullptr; }
};

FlexUiInfographicResult create_flexui_infographic(
    flexUI::Box& box,
    const UnifiedInfographic& infographic,
    const InfographicViewOptions& options = {});

}  // namespace flex::modules::infographic
