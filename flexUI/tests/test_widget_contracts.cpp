#include <tinytest.h>
#undef group
#include "test_support.h"

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/renderer.h>
#include <flexUI/text_layout.h>
#include <flexUI/text_util.h>
#include <flexUI/textedit.h>
#include <flexUI/widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flex/runtime/renderer.h>

#include <memory>
#include <string>
#include <vector>

using namespace flexUI;

namespace {

Event ctrl_key(KeyCode key) {
  return Event::key_down(key, static_cast<int>(KeyMod::Control));
}

Event ctrl_shift_key(KeyCode key) {
  return Event::key_down(
      key, static_cast<int>(KeyMod::Control) | static_cast<int>(KeyMod::Shift));
}

void render_widget(Widget& widget, const Element& elem, Renderer& renderer) {
  RenderCommandList commands(
      renderer.capabilities(),
      flex::make_translation(elem.absolute_x(), elem.absolute_y()));
  commands.save();
  commands.translate(elem.absolute_x(), elem.absolute_y());
  widget.emit_render_commands(elem, commands);
  commands.restore();
  commands.replay(renderer);
}

struct DrawRectCall {
  float x;
  float y;
  float w;
  float h;
};

struct TextCall {
  std::string text;
  float x;
  float y;
};

class RecordingRenderer final : public flex::Renderer {
public:
  bool register_font(const std::string& family,
                     const std::string& path) override {
    registered_font = family + ":" + path;
    return true;
  }
  void unregister_font(const std::string& family) override {
    unregistered_font = family;
  }
  void begin_frame(float width, float height, float) override {
    viewport_ = {0.0f, 0.0f, width, height};
    rects.clear();
    texts.clear();
  }
  void end_frame() override {}
  void set_retained_mode(bool) override {}
  void save() override {}
  void restore() override {}
  void reset() override {}
  void set_transform(const flex::Transform&) override {}
  void translate(float, float) override {}
  void rotate(float) override {}
  void scale(float, float) override {}
  void clip_rect(float, float, float, float) override {}
  void reset_clip() override {}
  void set_global_alpha(float) override {}
  void set_shadow(const flex::Shadow&) override {}
  void clear_shadow() override {}
  void set_blur(const flex::BlurFilter&) override {}
  void clear_blur() override {}
  void fill_path(const std::string&, const flex::Paint&) override {}
  void stroke_path(const std::string&, const flex::Paint&, float) override {}
  void draw_line(float, float, float, float, const flex::Paint&, float) override {}
  void draw_rect(float x, float y, float w, float h, float, const flex::Paint&,
                 const flex::Paint&, float) override {
    rects.push_back({x, y, w, h});
  }
  void draw_circle(float, float, float, const flex::Paint&, const flex::Paint&, float) override {}
  void draw_ellipse(float, float, float, float, const flex::Paint&, const flex::Paint&, float) override {}
  void draw_text(const std::string& text, float x, float y, const std::string&, float, bool,
                 const flex::Color&) override {
    texts.push_back({text, x, y});
  }
  void draw_image(const std::string&, float, float, float, float) override {}
  void draw_svg(const std::string&, float, float, float, float) override {}
  void draw_svg_data(const std::string&, float, float, float, float) override {}
  void clear(const flex::Color&) override {}
  flex::Bounds viewport() const override { return viewport_; }
  bool supports_retained_mode() const override { return false; }
  void remove_cached(flex::PaintHandle) override {}
  flex::PaintHandle push_rect(float, float, float, float, float, const flex::Paint&,
                              const flex::Paint&, float, const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_circle(float, float, float, const flex::Paint&, const flex::Paint&, float,
                                const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_ellipse(float, float, float, float, const flex::Paint&, const flex::Paint&,
                                 float, const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_polygon(int, float, const flex::Paint&, const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_star(int, float, float, const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_path(const std::string&, const flex::Paint&, const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  void update_transform(flex::PaintHandle, const flex::Transform&) override {}

  std::vector<DrawRectCall> rects;
  std::vector<TextCall> texts;
  std::string registered_font;
  std::string unregistered_font;

private:
  flex::Bounds viewport_{0.0f, 0.0f, 320.0f, 200.0f};
};

template <typename WidgetT>
struct WidgetHarness {
  Box box{nullptr};
  Element* root = nullptr;
  Element* elem = nullptr;
  WidgetT* widget = nullptr;

  template <typename... Args>
  WidgetHarness(const std::string& tag, const std::string& css, Args&&... args) {
    root = box.create("div", "root");
    elem = box.create_widget<WidgetT>(tag, "subject", std::forward<Args>(args)...);
    widget = static_cast<WidgetT*>(elem->widget);
    root->append(elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
    box.load_css(css);
    box.update();
    box.set_focus(elem);
  }
};

const char* kInputCss = R"(
  #root {
    width: 100%;
    height: 100%;
  }

  #subject {
    width: 240px;
    height: 40px;
    padding: 6px 8px;
    font-size: 20px;
  }
)";

const char* kTextAreaCss = R"(
  #root {
    width: 100%;
    height: 100%;
  }

  #subject {
    width: 240px;
    height: 120px;
    padding: 6px 8px;
    font-size: 20px;
  }
)";

float textarea_expected_width(const Element& elem, const std::string& text) {
  auto* style = elem.computed_style;
  if (!style) {
    return 0.0f;
  }

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--textarea-text"), style->text_color,
      Symbol("--textarea-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.5f);

  ComputedStyle measure_style = *style;
  measure_style.font_family = metrics.font_family;
  measure_style.font_size = metrics.font_size;
  measure_style.text_transform = TextTransform::None;
  measure_style.word_spacing = 0.0f;

  float width = 0.0f;
  bool has_previous_glyph = false;
  size_t byte_pos = 0;
  while (byte_pos < text.size()) {
    const size_t start = byte_pos;
    const uint32_t cp = utf8_next_scalar(text, byte_pos).value;
    if (has_previous_glyph) {
      width += std::max(style->letter_spacing, 0.0f);
    }
    if (is_emoji(cp)) {
      width += metrics.font_size;
    } else {
      width += approximate_text_width(
          &measure_style, text.substr(start, byte_pos - start));
    }
    if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\f') {
      width += std::max(style->word_spacing, 0.0f);
    }
    has_previous_glyph = true;
  }

  return width;
}

float input_expected_width(const Element& elem, const std::string& text) {
  auto* style = elem.computed_style;
  if (!style) {
    return 0.0f;
  }

  const auto metrics = resolve_editable_text_metrics(
      style, style->font_size, Symbol("--input-text"), style->text_color,
      Symbol("--input-placeholder"), Color{0.63f, 0.63f, 0.63f, 1.0f}, 1.0f);

  ComputedStyle measure_style = *style;
  measure_style.font_family = metrics.font_family;
  measure_style.font_size = metrics.font_size;
  measure_style.text_transform = TextTransform::None;
  measure_style.word_spacing = 0.0f;

  float width = 0.0f;
  bool has_previous_glyph = false;
  size_t byte_pos = 0;
  while (byte_pos < text.size()) {
    const size_t start = byte_pos;
    const uint32_t cp = utf8_next_scalar(text, byte_pos).value;
    if (has_previous_glyph) {
      width += std::max(style->letter_spacing, 0.0f);
    }
    if (is_emoji(cp)) {
      width += metrics.font_size;
    } else {
      width += approximate_text_width(
          &measure_style, text.substr(start, byte_pos - start));
    }
    if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\f') {
      width += std::max(style->word_spacing, 0.0f);
    }
    has_previous_glyph = true;
  }

  return width;
}

template <typename WidgetT>
void require_clipboard_roundtrip_contract(WidgetT& widget, Element& elem,
                                          const std::string& original) {
  check(widget.handle_event(Event::text_input(original), elem));
  check(widget.handle_event(ctrl_key(KeyCode::A), elem));

  check(widget.handle_event(ctrl_key(KeyCode::C), elem));
  check(widget.text() == original);

  check(widget.handle_event(ctrl_key(KeyCode::X), elem));
  check(widget.text().empty());

  check(widget.handle_event(ctrl_key(KeyCode::V), elem));
  check(widget.text() == original);
}

template <typename WidgetT>
void require_composition_contract(WidgetT& widget, Element& elem) {
  check(widget.handle_event(Event::text_input("ab"), elem));
  const std::string committed = widget.text();

  auto start = Event::composition_start();
  check(widget.handle_event(start, elem));
  check(widget.text() == committed);

  auto update = Event::composition_update("zh");
  check(widget.handle_event(update, elem));
  check(widget.text() == committed);

  auto end = Event::composition_end();
  check(widget.handle_event(end, elem));
  check(widget.text() == committed);

  check(widget.handle_event(Event::text_input("c"), elem));
  check(widget.text() == committed + "c");
}

template <typename WidgetT>
void require_caret_advances_with_text(WidgetT& widget, Element& elem,
                                      float expected_width) {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float w0 = 0.0f;
  float h0 = 0.0f;
  widget.get_caret_rect(elem, x0, y0, w0, h0);

  check(x0 >= 0.0f);
  check(y0 >= 0.0f);
  check(approx_eq(w0, expected_width, 0.001f));
  check(h0 > 0.0f);

  check(widget.handle_event(Event::text_input("ab"), elem));

  float x1 = 0.0f;
  float y1 = 0.0f;
  float w1 = 0.0f;
  float h1 = 0.0f;
  widget.get_caret_rect(elem, x1, y1, w1, h1);

  check(x1 > x0);
  check(approx_eq(y1, y0, 0.001f));
  check(approx_eq(w1, expected_width, 0.001f));
  check(approx_eq(h1, h0, 0.001f));
}

template <typename WidgetT>
void require_focus_out_composition_cleanup_and_blink_contract(
    WidgetHarness<WidgetT>& harness, float blink_period_ms) {
  RecordingRenderer backend;
  Renderer renderer(&backend);

  check(harness.widget->handle_event(Event::text_input("ab"), *harness.elem));
  const std::string committed = harness.widget->text();

  auto start = Event::composition_start();
  auto update = Event::composition_update("zh");
  auto end = Event::composition_end();

  check(harness.widget->handle_event(start, *harness.elem));
  check(harness.widget->handle_event(update, *harness.elem));
  check(harness.widget->text() == committed);

  backend.begin_frame(320.0f, 200.0f, 1.0f);
  render_widget(*harness.widget, *harness.elem, renderer);
  check(std::any_of(backend.texts.begin(), backend.texts.end(),
                    [](const TextCall& call) { return call.text == "zh"; }));

  check(harness.widget->handle_event(end, *harness.elem));
  check(harness.widget->text() == committed);

  float caret_x = 0.0f;
  float caret_y = 0.0f;
  float caret_w = 0.0f;
  float caret_h = 0.0f;
  harness.widget->get_caret_rect(*harness.elem, caret_x, caret_y, caret_w, caret_h);

  backend.begin_frame(320.0f, 200.0f, 1.0f);
  render_widget(*harness.widget, *harness.elem, renderer);
  check(std::none_of(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "zh"; }));
  check(std::any_of(backend.rects.begin(), backend.rects.end(),
                    [&](const DrawRectCall& call) {
                      return approx_eq(call.x, caret_x, 0.001f) &&
                             approx_eq(call.y, caret_y, 0.001f) &&
                             approx_eq(call.w, caret_w, 0.001f) &&
                             approx_eq(call.h, caret_h, 0.001f);
                    }));

  harness.box.update_time(blink_period_ms + 20.0f);
  backend.begin_frame(320.0f, 200.0f, 1.0f);
  render_widget(*harness.widget, *harness.elem, renderer);
  check(std::none_of(backend.rects.begin(), backend.rects.end(),
                     [&](const DrawRectCall& call) {
                       return approx_eq(call.x, caret_x, 0.001f) &&
                              approx_eq(call.y, caret_y, 0.001f) &&
                              approx_eq(call.w, caret_w, 0.001f) &&
                              approx_eq(call.h, caret_h, 0.001f);
                     }));

  harness.box.set_focus(nullptr);
  harness.box.update_time(blink_period_ms + 20.0f);
  backend.begin_frame(320.0f, 200.0f, 1.0f);
  render_widget(*harness.widget, *harness.elem, renderer);
  check(harness.widget->text() == committed);
  check(std::none_of(backend.texts.begin(), backend.texts.end(),
                     [](const TextCall& call) { return call.text == "zh"; }));
  check(std::none_of(backend.rects.begin(), backend.rects.end(),
                     [&](const DrawRectCall& call) {
                       return approx_eq(call.x, caret_x, 0.001f) &&
                              approx_eq(call.y, caret_y, 0.001f) &&
                              approx_eq(call.w, caret_w, 0.001f) &&
                              approx_eq(call.h, caret_h, 0.001f);
                     }));
}

template <typename WidgetT>
void require_change_callback_contract(WidgetHarness<WidgetT>& harness) {
  std::vector<std::string> changes;
  harness.widget->set_change_callback(
      [&](const std::string& text) { changes.push_back(text); });

  check(harness.widget->handle_event(Event::text_input("hi"), *harness.elem));
  check(changes.size() == 1);
  check(changes.back() == "hi");

  check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
  check(harness.widget->handle_event(ctrl_key(KeyCode::C), *harness.elem));
  check(changes.size() == 1);

  check(harness.widget->handle_event(ctrl_key(KeyCode::X), *harness.elem));
  check(changes.size() == 2);
  check(changes.back().empty());

  check(harness.widget->handle_event(ctrl_key(KeyCode::V), *harness.elem));
  check(changes.size() == 3);
  check(changes.back() == "hi");
}

template <typename WidgetT>
void require_select_all_replacement_contract(WidgetHarness<WidgetT>& harness) {
  check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
  check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
  check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
  check(harness.widget->text() == "X");
}

template <typename WidgetT>
void require_shortcut_entry_clipboard_contract(WidgetHarness<WidgetT>& harness) {
  std::vector<std::string> changes;
  harness.widget->set_change_callback(
      [&](const std::string& text) { changes.push_back(text); });

  check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
  check(changes.size() == 1);
  check(changes.back() == "hello");

  check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
  check(harness.widget->text() == "hello");
  check(changes.size() == 1);

  check(harness.widget->handle_event(ctrl_key(KeyCode::C), *harness.elem));
  check(harness.widget->text() == "hello");
  check(changes.size() == 1);

  check(harness.widget->handle_event(ctrl_key(KeyCode::X), *harness.elem));
  check(harness.widget->text().empty());
  check(changes.size() == 2);
  check(changes.back().empty());

  check(harness.widget->handle_event(ctrl_key(KeyCode::V), *harness.elem));
  check(harness.widget->text() == "hello");
  check(changes.size() == 3);
  check(changes.back() == "hello");
}

template <typename WidgetT, typename CursorGetter>
void require_selected_delete_contract(
    WidgetHarness<WidgetT>& harness,
    CursorGetter&& cursor_getter,
    KeyCode deletion_key) {
  check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
  check(harness.widget->handle_event(
      Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
  check(harness.widget->handle_event(
      Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));

  check(harness.widget->handle_event(Event::key_down(deletion_key), *harness.elem));
  check(harness.widget->text() == "hel");
  check(std::forward<CursorGetter>(cursor_getter)(*harness.widget) == 3);
}

int cursor_position_of(const InputWidget& widget) {
  return static_cast<int>(widget.cursor_pos());
}

int cursor_position_of(const TextAreaWidget& widget) {
  return widget.cursor_position();
}

template <typename WidgetT>
void require_selection_delete_contract(WidgetHarness<WidgetT>& harness, KeyCode key) {
  std::vector<std::string> changes;
  harness.widget->set_change_callback(
      [&](const std::string& text) { changes.push_back(text); });

  check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
  check(changes.size() == 1);
  check(changes.back() == "hello");

  check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
  check(harness.widget->handle_event(Event::key_down(key), *harness.elem));

  check(harness.widget->text().empty());
  check(cursor_position_of(*harness.widget) == 0);
  check(changes.size() == 2);
  check(changes.back().empty());
}

void require_input_undo_redo_shortcut_contract() {
  WidgetHarness<InputWidget> harness("input", kInputCss);

  check(harness.widget->handle_event(Event::text_input("a"), *harness.elem));
  check(harness.widget->handle_event(Event::text_input("b"), *harness.elem));
  check(harness.widget->text() == "ab");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "a");

  check(harness.widget->handle_event(ctrl_shift_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "ab");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "a");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Y), *harness.elem));
  check(harness.widget->text() == "ab");
}

void require_textarea_undo_redo_shortcut_contract() {
  WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

  check(harness.widget->handle_event(Event::text_input("a"), *harness.elem));
  check(harness.widget->handle_event(Event::text_input("b"), *harness.elem));
  check(harness.widget->text() == "ab");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "a");

  check(harness.widget->handle_event(ctrl_shift_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "ab");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "a");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Y), *harness.elem));
  check(harness.widget->text() == "ab");
}

void require_textarea_undo_redo_enter_contract() {
  WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

  check(harness.widget->handle_event(Event::text_input("a"), *harness.elem));
  check(harness.widget->handle_event(Event::text_input("b"), *harness.elem));
  check(harness.widget->handle_event(Event::key_down(KeyCode::Enter), *harness.elem));
  check(harness.widget->text() == "ab\n");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "ab");

  check(harness.widget->handle_event(ctrl_shift_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "ab\n");
}

void require_textarea_undo_redo_mouse_delete_contract() {
  WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

  check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));

  const float base_x = harness.elem->absolute_x();
  const float base_y = harness.elem->absolute_y();
  const float padding_left = 8.0f;
  const float padding_top = 6.0f;
  const float char_width = 12.0f;
  const float y = base_y + padding_top + 10.0f;

  check(harness.widget->handle_event(
      Event::mouse_down(base_x + padding_left + char_width * 1.1f, y), *harness.elem));
  check(harness.widget->handle_event(
      Event::mouse_move(base_x + padding_left + char_width * 4.1f, y), *harness.elem));
  check(harness.widget->handle_event(
      Event::mouse_up(base_x + padding_left + char_width * 4.1f, y), *harness.elem));

  check(harness.widget->handle_event(Event::key_down(KeyCode::Backspace), *harness.elem));
  check(harness.widget->text() == "ho");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text() == "hello");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Y), *harness.elem));
  check(harness.widget->text() == "ho");
}

void require_textarea_undo_redo_paste_contract() {
  WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

  check(harness.widget->handle_event(Event::text_input("hi"), *harness.elem));
  check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
  check(harness.widget->handle_event(ctrl_key(KeyCode::C), *harness.elem));
  check(harness.widget->handle_event(ctrl_key(KeyCode::X), *harness.elem));
  check(harness.widget->text().empty());

  check(harness.widget->handle_event(ctrl_key(KeyCode::V), *harness.elem));
  check(harness.widget->text() == "hi");

  check(harness.widget->handle_event(ctrl_key(KeyCode::Z), *harness.elem));
  check(harness.widget->text().empty());

  check(harness.widget->handle_event(ctrl_key(KeyCode::Y), *harness.elem));
  check(harness.widget->text() == "hi");
}

void require_textarea_navigation_clears_selection_before_text_input() {
  WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

  check(harness.widget->handle_event(Event::text_input("ab"), *harness.elem));

  check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
  check(harness.widget->handle_event(Event::key_down(KeyCode::End), *harness.elem));
  check(harness.widget->handle_event(Event::text_input("x"), *harness.elem));
  check(harness.widget->text() == "abx");
}

} // namespace

spec("TextEdit horizontal navigation preserves UTF-8 scalar boundaries") {
  it("moves across two-byte and four-byte scalars in both directions") {
    std::string text = "aé🙂b";
    TextEdit edit;
    edit.init(&text, 10.0f, 20.0f);

    for (const int expected : {1, 3, 7, 8, 8}) {
      check(edit.key(static_cast<int>(KeyCode::Right)));
      check(edit.cursor() == expected);
      check_false(edit.has_selection());
    }
    for (const int expected : {7, 3, 1, 0, 0}) {
      check(edit.key(static_cast<int>(KeyCode::Left)));
      check(edit.cursor() == expected);
      check_false(edit.has_selection());
    }
    check(text == "aé🙂b");
  }

  it("keeps the shift anchor when reversing and collapses selections once") {
    std::string text = "aé🙂b";
    TextEdit edit;
    edit.init(&text, 10.0f, 20.0f);
    edit.set_cursor(1);

    check(edit.key(static_cast<int>(KeyCode::Right), true));
    check(edit.selected_text() == "é");
    check(edit.key(static_cast<int>(KeyCode::Right), true));
    check(edit.selected_text() == "é🙂");
    check(edit.key(static_cast<int>(KeyCode::Left), true));
    check(edit.selected_text() == "é");
    check(edit.key(static_cast<int>(KeyCode::Left), true));
    check_false(edit.has_selection());
    check(edit.cursor() == 1);
    check(edit.key(static_cast<int>(KeyCode::Left), true));
    check(edit.selected_text() == "a");
    check(edit.key(static_cast<int>(KeyCode::Right)));
    check(edit.cursor() == 1);
    check_false(edit.has_selection());

    edit.set_cursor(7);
    check(edit.key(static_cast<int>(KeyCode::Left), true));
    check(edit.selected_text() == "🙂");
    check(edit.key(static_cast<int>(KeyCode::Left), true));
    check(edit.selected_text() == "é🙂");
    check(edit.key(static_cast<int>(KeyCode::Right), true));
    check(edit.selected_text() == "🙂");
    check(edit.key(static_cast<int>(KeyCode::Left)));
    check(edit.cursor() == 3);
    check_false(edit.has_selection());
    check(text == "aé🙂b");
  }

  it("clamps stale cursor and selection offsets after external text shortening") {
    std::string text = "abc";
    TextEdit edit;
    edit.init(&text, 10.0f, 20.0f);
    edit.set_cursor(3);
    text = "a";

    check(edit.key(static_cast<int>(KeyCode::Left), true));
    check(edit.cursor() == 0);
    check(edit.selected_text() == "a");
    text.clear();
    check(edit.key(static_cast<int>(KeyCode::Right), true));
    check(edit.cursor() == 0);
    check_false(edit.has_selection());
  }
}

spec("Box exposes backend-neutral font registration") {
  it("forwards font lifecycle to the active renderer") {
    RecordingRenderer backend;
    Box box(&backend);

    check_true(box.register_font("Inter", "assets/Inter.ttf"));
    check(backend.registered_font == "Inter:assets/Inter.ttf");

    box.unregister_font("Inter");
    check(backend.unregistered_font == "Inter");
  }

  it("fails registration explicitly without a renderer") {
    Box box(nullptr);
    check_false(box.register_font("Inter", "assets/Inter.ttf"));
  }
}

spec("InputWidget and TextAreaWidget share the clipboard round-trip contract") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    check(harness.widget->wants_text_input());
    require_clipboard_roundtrip_contract(*harness.widget, *harness.elem,
                                         u8"héllo 世界");
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    check(harness.widget->wants_text_input());
    require_clipboard_roundtrip_contract(*harness.widget, *harness.elem, "hello");
  }
}

spec("InputWidget and TextAreaWidget share the Ctrl+A/C/X/V shortcut entry contract") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_shortcut_entry_clipboard_contract(harness);
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_shortcut_entry_clipboard_contract(harness);
  }
}

spec("InputWidget and TextAreaWidget keep composition separate from committed text") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_composition_contract(*harness.widget, *harness.elem);
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_composition_contract(*harness.widget, *harness.elem);
  }
}

spec("InputWidget and TextAreaWidget expose a stable first-line caret contract") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_caret_advances_with_text(*harness.widget, *harness.elem, 1.0f);
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_caret_advances_with_text(*harness.widget, *harness.elem, 2.0f);
  }
}

spec("TextAreaWidget caret moves to the next line after Enter") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

    check(harness.widget->handle_event(Event::text_input("ab"), *harness.elem));

    float x_before = 0.0f;
    float y_before = 0.0f;
    float w_before = 0.0f;
    float h_before = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_before, y_before, w_before, h_before);

    check(harness.widget->handle_event(Event::key_down(KeyCode::Enter), *harness.elem));

    float x_after = 0.0f;
    float y_after = 0.0f;
    float w_after = 0.0f;
    float h_after = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_after, y_after, w_after, h_after);

    check(x_after < x_before);
    check(y_after > y_before);
    check(approx_eq(w_after, w_before, 0.001f));
    check(approx_eq(h_after, h_before, 0.001f));
  }
}

spec("TextAreaWidget clears selection on plain navigation after select-all") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
    check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));

    check(harness.widget->handle_event(Event::key_down(KeyCode::Left), *harness.elem));
    check(harness.widget->cursor_position() == 4);

    check(harness.widget->handle_event(Event::text_input("x"), *harness.elem));
    check(harness.widget->text() == "hellxo");
  }
}

spec("TextAreaWidget extends selection with shift-navigation and replaces it on text input") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));

    check(harness.widget->cursor_position() == 3);

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "helX");
  }
}

spec("TextAreaWidget shrinks an anchored shift-selection when navigation reverses") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Right, static_cast<int>(KeyMod::Shift)), *harness.elem));

    check(harness.widget->cursor_position() == 4);

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "hellX");
  }
}

spec("TextAreaWidget mouse drag selection is replaced by text input") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));

    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float padding_left = 8.0f;
    const float padding_top = 6.0f;
    const float char_width = 12.0f;
    const float y = base_y + padding_top + 10.0f;

    check(harness.widget->handle_event(
        Event::mouse_down(base_x + padding_left + char_width * 1.1f, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_move(base_x + padding_left + char_width * 4.1f, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_up(base_x + padding_left + char_width * 4.1f, y), *harness.elem));

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "hXo");
  }
}

spec("TextAreaWidget extends a mouse-drag selection with shift-navigation") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));

    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float padding_left = 8.0f;
    const float padding_top = 6.0f;
    const float text_end_x =
        padding_left + textarea_expected_width(*harness.elem, "hello");
    const float selection_start_x =
        padding_left + textarea_expected_width(*harness.elem, "hel") + 1.0f;
    const float y = base_y + padding_top + 10.0f;

    check(harness.widget->handle_event(
        Event::mouse_down(base_x + text_end_x + 2.0f, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_move(base_x + selection_start_x, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_up(base_x + selection_start_x, y), *harness.elem));

    check(harness.widget->cursor_position() == 3);

    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->cursor_position() == 2);

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "heX");
  }
}

spec("InputWidget extends selection with shift-navigation and replaces it on text input") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", kInputCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));

    check(harness.widget->cursor_pos() == 3);
    check(harness.widget->selected_text() == "lo");

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "helX");
  }
}

spec("InputWidget shrinks an anchored shift-selection when navigation reverses") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", kInputCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Right, static_cast<int>(KeyMod::Shift)), *harness.elem));

    check(harness.widget->cursor_pos() == 4);
    check(harness.widget->selected_text() == "o");

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "hellX");
  }
}

spec("InputWidget keeps mouse-drag selection anchored across shift-navigation") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", kInputCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));

    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float padding_left = 8.0f;
    const float text_end_x =
        padding_left + input_expected_width(*harness.elem, "hello");
    const float selection_start_x =
        padding_left + input_expected_width(*harness.elem, "hel") + 1.0f;
    const float y = base_y + harness.elem->height() * 0.5f;

    check(harness.widget->handle_event(
        Event::mouse_down(base_x + text_end_x + 2.0f, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_move(base_x + selection_start_x, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_up(base_x + selection_start_x, y), *harness.elem));

    check(harness.widget->cursor_pos() == 3);
    check(harness.widget->selected_text() == "lo");

    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Left, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->cursor_pos() == 2);
    check(harness.widget->selected_text() == "llo");

    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Right, static_cast<int>(KeyMod::Shift)), *harness.elem));
    check(harness.widget->cursor_pos() == 3);
    check(harness.widget->selected_text() == "lo");

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "helX");
  }
}

spec("InputWidget mouse drag selection is replaced by text input") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", kInputCss);

    check(harness.widget->handle_event(Event::text_input("hello"), *harness.elem));

    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float padding_left = 8.0f;
    const float char_width = 12.0f;
    const float y = base_y + harness.elem->height() * 0.5f;

    check(harness.widget->handle_event(
        Event::mouse_down(base_x + padding_left + char_width * 1.1f, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_move(base_x + padding_left + char_width * 4.1f, y), *harness.elem));
    check(harness.widget->handle_event(
        Event::mouse_up(base_x + padding_left + char_width * 4.1f, y), *harness.elem));

    check(harness.widget->selected_text() == "ell");

    check(harness.widget->handle_event(Event::text_input("X"), *harness.elem));
    check(harness.widget->text() == "hXo");
  }
}

spec("InputWidget and TextAreaWidget share focus-out, composition cleanup, and caret blink semantics") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_focus_out_composition_cleanup_and_blink_contract(harness, 500.0f);
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_focus_out_composition_cleanup_and_blink_contract(harness, 530.0f);
  }
}

spec("InputWidget and TextAreaWidget share change-callback semantics for edit operations") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_change_callback_contract(harness);
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_change_callback_contract(harness);
  }
}

spec("InputWidget and TextAreaWidget delete selected text with consistent callback and caret semantics") {
  it("InputWidget Backspace") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_selection_delete_contract(harness, KeyCode::Backspace);
  }

  it("InputWidget Delete") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_selection_delete_contract(harness, KeyCode::Delete);
  }

  it("TextAreaWidget Backspace") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_selection_delete_contract(harness, KeyCode::Backspace);
  }

  it("TextAreaWidget Delete") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_selection_delete_contract(harness, KeyCode::Delete);
  }
}

spec("InputWidget and TextAreaWidget share Ctrl+A replacement semantics") {
  it("InputWidget") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_select_all_replacement_contract(harness);
  }

  it("TextAreaWidget") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_select_all_replacement_contract(harness);
  }
}

spec("InputWidget supports shared undo redo shortcut entry semantics") {
  it("runs") {
    require_input_undo_redo_shortcut_contract();
  }
}

spec("TextAreaWidget supports undo redo shortcut entry semantics") {
  it("runs") {
    require_textarea_undo_redo_shortcut_contract();
  }
}

spec("TextAreaWidget undo redo covers Enter edits") {
  it("runs") {
    require_textarea_undo_redo_enter_contract();
  }
}

spec("TextAreaWidget undo redo covers mouse drag delete edits") {
  it("runs") {
    require_textarea_undo_redo_mouse_delete_contract();
  }
}

spec("TextAreaWidget undo redo covers paste edits") {
  it("runs") {
    require_textarea_undo_redo_paste_contract();
  }
}

spec("InputWidget and TextAreaWidget share selected-delete semantics") {
  it("InputWidget Backspace") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_selected_delete_contract(
        harness, [](const InputWidget& widget) { return static_cast<int>(widget.cursor_pos()); },
        KeyCode::Backspace);
  }

  it("InputWidget Delete") {
    WidgetHarness<InputWidget> harness("input", kInputCss);
    require_selected_delete_contract(
        harness, [](const InputWidget& widget) { return static_cast<int>(widget.cursor_pos()); },
        KeyCode::Delete);
  }

  it("TextAreaWidget Backspace") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_selected_delete_contract(
        harness, [](const TextAreaWidget& widget) { return widget.cursor_position(); },
        KeyCode::Backspace);
  }

  it("TextAreaWidget Delete") {
    WidgetHarness<TextAreaWidget> harness("div", kTextAreaCss);
    require_selected_delete_contract(
        harness, [](const TextAreaWidget& widget) { return widget.cursor_position(); },
        KeyCode::Delete);
  }
}

spec("TextAreaWidget collapses selection on non-shift navigation before text input") {
  it("runs") {
    require_textarea_navigation_clears_selection_before_text_input();
  }
}

spec("InputWidget collapses selection on non-shift navigation before text input") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", kInputCss);

    check(harness.widget->handle_event(Event::text_input("ab"), *harness.elem));
    check(harness.widget->handle_event(ctrl_key(KeyCode::A), *harness.elem));
    check(harness.widget->handle_event(Event::key_down(KeyCode::End), *harness.elem));
    check(harness.widget->handle_event(Event::text_input("x"), *harness.elem));

    check(harness.widget->text() == "abx");
  }
}

spec("InputWidget rtl caret and mouse hit testing use the right edge as logical start") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", R"(
      #root {
        width: 100%;
        height: 100%;
      }
      #subject {
        width: 240px;
        height: 40px;
        padding: 6px 8px;
        font-size: 20px;
        direction: rtl;
      }
    )");

    harness.widget->set_text("abcd");

    float y0 = 0.0f;
    float w0 = 0.0f;
    float h0 = 0.0f;
    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float y = base_y + 20.0f;
    check(harness.widget->handle_event(Event::mouse_down(base_x + 231.0f, y), *harness.elem));

    float x_right = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_right, y0, w0, h0);

    check(harness.widget->handle_event(Event::mouse_down(base_x + 185.0f, y), *harness.elem));

    float x_left = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_left, y0, w0, h0);
    check(x_left < x_right);
  }
}

spec("InputWidget letter spacing keeps emoji caret, hit testing, selection, and composition geometry aligned") {
  it("runs") {
    WidgetHarness<InputWidget> harness("input", R"(
      #root {
        width: 100%;
        height: 100%;
      }
      #subject {
        width: 240px;
        height: 40px;
        padding: 6px 8px;
        font-size: 20px;
        letter-spacing: 6px;
      }
    )");

    RecordingRenderer backend;
    Renderer renderer(&backend);

    harness.widget->set_text("a🙂b");
    harness.widget->set_cursor_pos(0);

    float x0 = 0.0f;
    float y0 = 0.0f;
    float w0 = 0.0f;
    float h0 = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x0, y0, w0, h0);

    check(harness.widget->handle_event(Event::key_down(KeyCode::Right), *harness.elem));
    float x_after_a = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_after_a, y0, w0, h0);

    check(harness.widget->handle_event(Event::key_down(KeyCode::Right), *harness.elem));
    float x_after_emoji = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_after_emoji, y0, w0, h0);

    check(approx_eq(x_after_a - x0, input_expected_width(*harness.elem, "a"), 0.001f));
    check(approx_eq(x_after_emoji - x0, input_expected_width(*harness.elem, "a🙂"), 0.001f));

    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float hit_y = base_y + 20.0f;
    const float hit_x =
        base_x + x_after_a + (x_after_emoji - x_after_a) * 0.75f;
    check(harness.widget->handle_event(Event::mouse_down(hit_x, hit_y), *harness.elem));
    check(harness.widget->handle_event(Event::mouse_up(hit_x, hit_y), *harness.elem));

    float x_hit = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_hit, y0, w0, h0);
    check(approx_eq(x_hit, x_after_emoji, 0.001f));

    harness.widget->set_cursor_pos(1);
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Right, static_cast<int>(KeyMod::Shift)),
        *harness.elem));

    backend.begin_frame(320.0f, 200.0f, 1.0f);
    render_widget(*harness.widget, *harness.elem, renderer);
    check(std::any_of(
        backend.rects.begin(), backend.rects.end(),
        [&](const DrawRectCall& call) {
          return approx_eq(call.x, x_after_a, 0.001f) &&
                 approx_eq(call.w, x_after_emoji - x_after_a, 0.001f);
        }));

    harness.widget->set_cursor_pos(1);
    check(harness.widget->handle_event(Event::composition_start(), *harness.elem));
    check(harness.widget->handle_event(Event::composition_update("🙂b"), *harness.elem));

    backend.begin_frame(320.0f, 200.0f, 1.0f);
    render_widget(*harness.widget, *harness.elem, renderer);
    check(std::any_of(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; }));
    check(std::any_of(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "b"; }));
    check(std::any_of(
        backend.rects.begin(), backend.rects.end(),
        [&](const DrawRectCall& call) {
          return approx_eq(call.x, x_after_a, 0.001f) &&
                 approx_eq(call.h, 1.0f, 0.001f) &&
                 approx_eq(call.w, input_expected_width(*harness.elem, "🙂b"),
                           0.001f);
        }));
  }
}

spec("TextAreaWidget rtl caret and mouse hit testing use the right edge as logical start") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", R"(
      #root {
        width: 100%;
        height: 100%;
      }
      #subject {
        width: 240px;
        height: 120px;
        padding: 6px 8px;
        font-size: 20px;
        direction: rtl;
      }
    )");

    harness.widget->set_text("abcd");

    float y0 = 0.0f;
    float w0 = 0.0f;
    float h0 = 0.0f;
    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float y = base_y + 16.0f;
    check(harness.widget->handle_event(Event::mouse_down(base_x + 231.0f, y), *harness.elem));

    float x_right = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_right, y0, w0, h0);

    check(harness.widget->handle_event(Event::mouse_down(base_x + 185.0f, y), *harness.elem));

    float x_left = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_left, y0, w0, h0);
    check(x_left < x_right);
  }
}

spec("TextAreaWidget letter spacing keeps emoji caret, hit testing, selection, and composition geometry aligned") {
  it("runs") {
    WidgetHarness<TextAreaWidget> harness("div", R"(
      #root {
        width: 100%;
        height: 100%;
      }
      #subject {
        width: 240px;
        height: 120px;
        padding: 6px 8px;
        font-size: 20px;
        letter-spacing: 6px;
      }
    )");

    RecordingRenderer backend;
    Renderer renderer(&backend);

    harness.widget->set_text("a🙂b\nz");
    harness.widget->set_cursor_position(0);

    float x0 = 0.0f;
    float y0 = 0.0f;
    float w0 = 0.0f;
    float h0 = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x0, y0, w0, h0);

    check(harness.widget->handle_event(Event::key_down(KeyCode::Right), *harness.elem));
    float x_after_a = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_after_a, y0, w0, h0);

    check(harness.widget->handle_event(Event::key_down(KeyCode::Right), *harness.elem));
    float x_after_emoji = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_after_emoji, y0, w0, h0);

    check(approx_eq(x_after_a - x0, textarea_expected_width(*harness.elem, "a"), 0.001f));
    check(approx_eq(x_after_emoji - x0, textarea_expected_width(*harness.elem, "a🙂"), 0.001f));

    const float base_x = harness.elem->absolute_x();
    const float base_y = harness.elem->absolute_y();
    const float hit_y = base_y + 16.0f;
    const float hit_x =
        base_x + x_after_a + (x_after_emoji - x_after_a) * 0.75f;
    check(harness.widget->handle_event(Event::mouse_down(hit_x, hit_y), *harness.elem));
    check(harness.widget->handle_event(Event::mouse_up(hit_x, hit_y), *harness.elem));

    float x_hit = 0.0f;
    harness.widget->get_caret_rect(*harness.elem, x_hit, y0, w0, h0);
    check(approx_eq(x_hit, x_after_emoji, 0.001f));

    harness.widget->set_cursor_position(1);
    check(harness.widget->handle_event(
        Event::key_down(KeyCode::Right, static_cast<int>(KeyMod::Shift)),
        *harness.elem));

    backend.begin_frame(320.0f, 200.0f, 1.0f);
    render_widget(*harness.widget, *harness.elem, renderer);
    check(std::any_of(
        backend.rects.begin(), backend.rects.end(),
        [&](const DrawRectCall& call) {
          return approx_eq(call.x, x_after_a, 0.001f) &&
                 approx_eq(call.w, x_after_emoji - x_after_a, 0.001f);
        }));

    harness.widget->set_cursor_position(1);
    check(harness.widget->handle_event(Event::composition_start(), *harness.elem));
    check(harness.widget->handle_event(Event::composition_update("🙂b"), *harness.elem));

    backend.begin_frame(320.0f, 200.0f, 1.0f);
    render_widget(*harness.widget, *harness.elem, renderer);
    check(std::any_of(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "🙂"; }));
    check(std::any_of(
        backend.texts.begin(), backend.texts.end(),
        [](const TextCall& call) { return call.text == "b"; }));
    check(std::any_of(
        backend.rects.begin(), backend.rects.end(),
        [&](const DrawRectCall& call) {
          return approx_eq(call.x, x_after_a, 0.001f) &&
                 approx_eq(call.h, 2.0f, 0.001f) &&
                 approx_eq(call.w, textarea_expected_width(*harness.elem, "🙂b"),
                           0.001f);
        }));
  }
}
