#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <flexui/app.h>
#include <flexui/flexui.h>
#include <flexui/widget_factory.h>
#include <flexui/widgets/interaction.h>
#include <flexui/widgets/text_input.h>


#include <fmtlog.h>
#include <nanovg.h>

#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

using namespace flexui;

namespace {

std::string loadCssTheme(const std::string &path) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file) {
    loge("FlexUI MVC demo: failed to open CSS '{}'", path);
    exit(1);
  }

  std::ostringstream ss;
  ss << file.rdbuf();

  const std::string contents = ss.str();
  if (contents.empty()) {
    loge("FlexUI MVC demo: CSS '{}' is empty", path);
    exit(1);
  }

  return contents;
}

constexpr char kFeedbackInputId[] = "feedback-input";

void BuildDemoDocument(FlexDocument &document) {
  document.setVariable("--primary-color", "#4a90e2");
  document.setVariable("--accent-color", "#764ba2");

  const std::string css = loadCssTheme("styles/flexui_mvc.css");
  document.addStyleSheet({"mvc-demo", css});

  FlexButtonProps hover_btn;
  hover_btn.id = "hover-btn";
  hover_btn.text = "Hover";
  hover_btn.styles["left"] = "80px";
  hover_btn.styles["top"] = "140px";
  FlexWidgetFactory::createButton(document, hover_btn);

  FlexButtonProps click_btn = hover_btn;
  click_btn.id = "click-btn";
  click_btn.text = "Click";
  click_btn.styles["left"] = "280px";
  click_btn.classes.push_back("button-secondary");
  FlexWidgetFactory::createButton(document, click_btn);

  FlexNodeDesc surface;
  surface.id = "card";
  surface.tag = "rect";
  surface.classes = {"surface"};
  surface.styles["left"] = "80px";
  surface.styles["top"] = "240px";
  surface.text = "CSS + Controllers";
  document.appendNode(surface);

  FlexNodeDesc pulse;
  pulse.id = "pulse-circle";
  pulse.tag = "circle";
  pulse.classes = {"pulse"};
  pulse.styles["left"] = "480px";
  pulse.styles["top"] = "140px";
  document.appendNode(pulse);

  FlexToggleProps toggle_props;
  toggle_props.id = "toggle";
  toggle_props.handle_id = "toggle-handle";
  toggle_props.track_styles["left"] = "480px";
  toggle_props.track_styles["top"] = "320px";
  toggle_props.handle_styles["left"] = "556px";
  toggle_props.handle_styles["top"] = "320px";
  FlexWidgetFactory::createToggle(document, toggle_props);

  FlexCheckboxProps checkbox_props;
  checkbox_props.id = "updates-checkbox";
  checkbox_props.styles["left"] = "720px";
  checkbox_props.styles["top"] = "400px";
  FlexWidgetFactory::createCheckbox(document, checkbox_props);

  FlexNodeDesc checkbox_label;
  checkbox_label.id = "updates-label";
  checkbox_label.tag = "label";
  checkbox_label.classes = {"label"};
  checkbox_label.styles["left"] = "760px";
  checkbox_label.styles["top"] = "396px";
  checkbox_label.text = "Email updates";
  document.appendNode(checkbox_label);

  FlexTextInputProps feedback_input;
  feedback_input.id = kFeedbackInputId;
  feedback_input.styles["left"] = "720px";
  feedback_input.styles["top"] = "320px";
  feedback_input.styles["width"] = "320px";
  feedback_input.placeholder = "Share feedback...";
  FlexWidgetFactory::createTextInput(document, feedback_input);

  FlexSliderProps volume_slider;
  volume_slider.track_id = "volume-track";
  volume_slider.fill_id = "volume-fill";
  volume_slider.thumb_id = "volume-thumb";
  volume_slider.left = 720.f;
  volume_slider.top = 500.f;
  volume_slider.width = 320.f;
  volume_slider.height = 8.f;
  volume_slider.thumb_size = 26.f;
  volume_slider.value = 0.35f;
  FlexWidgetFactory::createSlider(document, volume_slider);

  FlexNodeDesc volume_label;
  volume_label.id = "volume-label";
  volume_label.tag = "label";
  volume_label.classes = {"label"};
  volume_label.styles["left"] = "720px";
  volume_label.styles["top"] = "472px";
  volume_label.text = "Volume";
  document.appendNode(volume_label);

  FlexRadioProps radio_music;
  radio_music.id = "radio-music";
  radio_music.styles["left"] = "720px";
  radio_music.styles["top"] = "560px";
  FlexWidgetFactory::createRadio(document, radio_music);

  FlexNodeDesc radio_music_label;
  radio_music_label.id = "radio-music-label";
  radio_music_label.tag = "label";
  radio_music_label.classes = {"label"};
  radio_music_label.styles["left"] = "760px";
  radio_music_label.styles["top"] = "556px";
  radio_music_label.text = "Music";
  document.appendNode(radio_music_label);

  FlexRadioProps radio_podcast = radio_music;
  radio_podcast.id = "radio-podcast";
  radio_podcast.styles["top"] = "600px";
  FlexWidgetFactory::createRadio(document, radio_podcast);

  FlexNodeDesc radio_podcast_label;
  radio_podcast_label.id = "radio-podcast-label";
  radio_podcast_label.tag = "label";
  radio_podcast_label.classes = {"label"};
  radio_podcast_label.styles["left"] = "760px";
  radio_podcast_label.styles["top"] = "596px";
  radio_podcast_label.text = "Podcasts";
  document.appendNode(radio_podcast_label);

  FlexDropdownProps mode_dropdown;
  mode_dropdown.id = "mode-dropdown";
  mode_dropdown.display_id = "mode-dropdown-label";
  mode_dropdown.menu_id = "mode-dropdown-menu";
  mode_dropdown.placeholder = "Select mode";
  mode_dropdown.container_styles["left"] = "1040px";
  mode_dropdown.container_styles["top"] = "320px";
  mode_dropdown.container_styles["width"] = "220px";

  FlexDropdownOptionProps opt_music;
  opt_music.id = "dropdown-option-music";
  opt_music.text = "Focus";
  opt_music.value = "focus";
  mode_dropdown.options.push_back(opt_music);

  FlexDropdownOptionProps opt_relax;
  opt_relax.id = "dropdown-option-relax";
  opt_relax.text = "Relax";
  opt_relax.value = "relax";
  mode_dropdown.options.push_back(opt_relax);

  FlexDropdownOptionProps opt_energy;
  opt_energy.id = "dropdown-option-energy";
  opt_energy.text = "Energy";
  opt_energy.value = "energy";
  mode_dropdown.options.push_back(opt_energy);

  FlexWidgetFactory::createDropdown(document, mode_dropdown);
}

class Pulse : public Flex {
public:
  void update(float dt) override {
    if (!document()) {
      return;
    }
    m_time += dt;
    const float pulse = (std::sin(m_time * 2.0f) + 1.0f) * 0.5f;
    const float scale = 1.0f + pulse * 0.25f;
    const float opacity = 0.75f + pulse * 0.25f;

    const std::string transform = "scale(" + std::to_string(scale) + ")";
    document()->setStyle("pulse-circle", "transform", transform);
    document()->setStyle("pulse-circle", "opacity", std::to_string(opacity));
  }

private:
  float m_time = 0.f;
};

void DrawOverlay(NVGcontext *vg, int width, int height) {
  nvgFontFace(vg, "sans-serif-Bold");
  nvgFontSize(vg, 34.f);
  nvgFillColor(vg, nvgRGBA(30, 30, 30, 255));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(vg, 40.f, 32.f, "FlexUI MVC Demo", nullptr);

  nvgFontFace(vg, "sans-serif");
  nvgFontSize(vg, 16.f);
  nvgFillColor(vg, nvgRGBA(90, 90, 90, 255));
  nvgText(vg, 40.f, 72.f, "Model (document) + View (NVG/NVGCSS) + Controllers (input/anim)",
          nullptr);
}

} // namespace

int main(int /*argc*/, char ** /*argv*/) {
  FlexApp app;
  FlexAppConfig config;
  config.view.window_title = "FlexUI MVC Demo";
  config.view.width = 1400;
  config.view.height = 900;

  if (!app.init(config)) {
    loge("Failed to initialize FlexApp");
    return -1;
  }

  // Load fonts used by overlays / CSS text.
  if (auto *vg = app.view().vg()) {
    nvgCreateFont(vg, "sans-serif", "resources/Roboto-Regular.ttf");
    nvgCreateFont(vg, "sans-serif-Bold", "resources/Roboto-Bold.ttf");
  }

  auto document = std::make_shared<FlexDocument>();
  BuildDemoDocument(*document);
  app.setDocument(document);

  auto pointer_controller = std::make_shared<FlexPointer>();
  FlexPointerBinding hover_binding;
  hover_binding.element_id = "hover-btn";
  pointer_controller->addBinding(hover_binding);

  std::weak_ptr<FlexDocument> weak_doc = document;
  auto card_highlight = std::make_shared<bool>(false);

  FlexPointerBinding click_binding;
  click_binding.element_id = "click-btn";
  click_binding.callback = [weak_doc, card_highlight](const FlexPointerEvent &event) mutable {
    if (event.type != FlexPointerEventType::Clicked) {
      return;
    }
    *card_highlight = !*card_highlight;
    if (auto doc = weak_doc.lock()) {
      doc->setText("card", *card_highlight ? "Button Clicked!" : "CSS + Controllers");
      doc->setClass("card", "surface-accent", *card_highlight);
    }
  };
  pointer_controller->addBinding(click_binding);

  auto pulse_controller = std::make_shared<Pulse>();
  auto text_input_controller = std::make_shared<FlexTextInput>();
  FlexTextInputBinding feedback_binding;
  feedback_binding.element_id = kFeedbackInputId;
  feedback_binding.on_change = [](const std::string &value) { logi("Feedback input: {}", value); };
  text_input_controller->registerInput(std::move(feedback_binding));

  auto toggle_controller = std::make_shared<FlexToggle>();
  FlexToggleBinding toggle_binding;
  toggle_binding.track_id = "toggle";
  toggle_binding.handle_id = "toggle-handle";
  toggle_binding.handle_on_value = "556px";
  toggle_binding.handle_off_value = "512px";
  toggle_binding.on_change = [](bool on) { logi("Toggle: {}", on); };
  toggle_controller->registerToggle(std::move(toggle_binding));

  auto checkbox_controller = std::make_shared<FlexCheckbox>();
  FlexCheckboxBinding checkbox_binding;
  checkbox_binding.element_id = "updates-checkbox";
  checkbox_binding.on_change = [](bool checked) { logi("Email updates: {}", checked); };
  checkbox_controller->registerCheckbox(std::move(checkbox_binding));

  auto slider_controller = std::make_shared<FlexSlider>();
  FlexSliderBinding slider_binding;
  slider_binding.track_id = "volume-track";
  slider_binding.fill_id = "volume-fill";
  slider_binding.thumb_id = "volume-thumb";
  slider_binding.track_left = 720.f;
  slider_binding.track_width = 320.f;
  slider_binding.thumb_size = 26.f;
  slider_binding.value = 0.35f;
  slider_binding.on_change = [](float value) { logi("Volume slider: {:.2f}", value); };
  slider_controller->registerSlider(std::move(slider_binding));

  auto radio_controller = std::make_shared<FlexRadio>();
  FlexRadioBinding radio_music_binding;
  radio_music_binding.element_id = "radio-music";
  radio_music_binding.group_id = "audio-mode";
  radio_music_binding.initial_checked = true;
  radio_music_binding.on_selected = []() { logi("Audio mode: Music"); };
  radio_controller->registerRadio(std::move(radio_music_binding));

  FlexRadioBinding radio_podcast_binding;
  radio_podcast_binding.element_id = "radio-podcast";
  radio_podcast_binding.group_id = "audio-mode";
  radio_podcast_binding.on_selected = []() { logi("Audio mode: Podcasts"); };
  radio_controller->registerRadio(std::move(radio_podcast_binding));

  auto dropdown_controller = std::make_shared<FlexDropdown>();
  FlexDropdownBinding dropdown_binding;
  dropdown_binding.container_id = "mode-dropdown";
  dropdown_binding.menu_id = "mode-dropdown-menu";
  dropdown_binding.display_id = "mode-dropdown-label";
  dropdown_binding.placeholder = "Select mode";
  dropdown_binding.options = {{"dropdown-option-music", "Focus", "focus"},
                              {"dropdown-option-relax", "Relax", "relax"},
                              {"dropdown-option-energy", "Energy", "energy"}};
  dropdown_binding.on_select = [](const std::string &value) { logi("Mode dropdown: {}", value); };
  dropdown_controller->registerDropdown(std::move(dropdown_binding));

  app.addController(pointer_controller);
  app.addController(text_input_controller);
  app.addController(slider_controller);
  app.addController(toggle_controller);
  app.addController(checkbox_controller);
  app.addController(radio_controller);
  app.addController(dropdown_controller);
  app.addController(pulse_controller);

  app.setOverlayCallback(DrawOverlay);
  app.run();

  return 0;
}
