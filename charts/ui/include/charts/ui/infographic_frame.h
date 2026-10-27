#pragma once

#include <memory>
#include <string>

namespace flexUI {
class Box;
class Element;
class Widget;
}

namespace charts::ui {

struct InfographicFrameOptions {
  std::string title;
  std::string view_id;
  std::string accessible_label = "Infographic";
  float width = 600.0f;
  float height = 500.0f;
};

struct InfographicFrame {
  flexUI::Element* root = nullptr;
  flexUI::Element* title = nullptr;
  flexUI::Element* surface = nullptr;
  flexUI::Element* plot = nullptr;

  explicit operator bool() const noexcept {
    return root != nullptr && surface != nullptr && plot != nullptr;
  }
};

/// Prepare a single detached XML component containing the shell, optional
/// title and one already-built native Plot widget. It is published to Box only
/// if template compilation, all required slots, layout and widget binding
/// succeed. No BytesVM, browser DOM, or fallback imperative UI path.
InfographicFrame create_infographic_frame_with_plot(
    flexUI::Box& box, const InfographicFrameOptions& options,
    std::unique_ptr<flexUI::Widget> prepared_plot);

} // namespace charts::ui
