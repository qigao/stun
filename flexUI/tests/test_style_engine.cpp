#include <tinytest.h>
#undef group
#include "test_support.h"
#include <flexUI/box.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/style_engine.h>
#include <flexUI/transition.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/datepicker_widget.h>
#include <flexUI/widgets/draggable_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <flexUI/widgets/gradient_editor_widget.h>
#include <flexUI/widgets/group_button_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/listview_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/panel_widget.h>
#include <flexUI/widgets/popover_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/widgets/splitter_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/timepicker_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/toolbar_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/tree_widget.h>

#include <memory>
#include <variant>

using namespace flexUI;

namespace {

class CountingWidget final : public Widget {
public:
  void emit_render_commands(const Element&, RenderCommandList&) override {}
  void update(float, Element&) override { ++updates; }
  const char* type_name() const override { return "CountingWidget"; }

  int updates = 0;
};

class OwnedWidget final : public Widget {
public:
  explicit OwnedWidget(int* destructions) : destructions_(destructions) {}
  ~OwnedWidget() override {
    if (destructions_) {
      ++(*destructions_);
    }
  }

  void emit_render_commands(const Element&, RenderCommandList&) override {}
  const char* type_name() const override { return "OwnedWidget"; }

private:
  int* destructions_ = nullptr;
};

void require_color(const Color& color, float r, float g, float b,
                   float a = 1.0f, float tolerance = 0.001f) {
  check(approx_eq(color.r, r, tolerance));
  check(approx_eq(color.g, g, tolerance));
  check(approx_eq(color.b, b, tolerance));
  check(approx_eq(color.a, a, tolerance));
}

const DrawRectCommand* first_rect_at(const RenderCommandList& commands, float x, float y) {
  for (const auto& command : commands.commands()) {
    const auto* rect = std::get_if<DrawRectCommand>(&command);
    if (rect && approx_eq(rect->x, x, 0.001f) &&
        approx_eq(rect->y, y, 0.001f)) {
      return rect;
    }
  }
  return nullptr;
}

} // namespace

spec("Box keeps element id index synchronized") {
  it("updates lookup entries when an element id changes") {
    Box box(nullptr);
    auto* elem = box.create("div", "before");

    check(box.get_by_id("before") == elem);

    elem->set_element_id("after");

    check(box.get_by_id("before") == nullptr);
    check(box.get_by_id("after") == elem);
  }

  it("restores a previous duplicate id owner when the current owner changes") {
    Box box(nullptr);
    auto* first = box.create("div", "shared");
    auto* second = box.create("div", "shared");

    check(box.get_by_id("shared") == second);

    second->set_element_id("unique");

    check(box.get_by_id("shared") == first);
    check(box.get_by_id("unique") == second);
  }
}

spec("Box owns widgets created through create_with_widget") {
  it("destroys unique_ptr widgets exactly once") {
    int destructions = 0;
    {
      Box box(nullptr);
      auto widget = std::make_unique<OwnedWidget>(&destructions);
      auto* elem = box.create_with_widget("div", std::move(widget), "owned");
      check(elem->widget != nullptr);
      check(box.get_by_id("owned") == elem);
      check(destructions == 0);
    }
    check(destructions == 1);
  }
}

spec("StyleEngine applies selector specificity and source order") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "submit");
    button->add_class("btn");
    button->add_class("primary");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      * { color: #111111; }
      button { color: #222222; width: 10px; }
      .btn { color: #333333; width: 20px; }
      .btn { color: #444444; }
      button.primary { color: #555555; }
      #submit { color: #666666; width: 30px; }
    )");
    box.update();

    require_color(button->style_.text_color, 0x66 / 255.0f, 0x66 / 255.0f,
                  0x66 / 255.0f);
    check(approx_eq(button->style_.width, 30.0f, 0.0));
    check(approx_eq(button->layout_width(), 30.0f, 0.0));
  }
}

spec("StyleEngine applies important declarations through the CSS cascade") {
  it("orders important declarations by priority specificity and source order") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "submit");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #submit {
        color: #111111;
        width: 10px !important;
        height: 20px !important;
      }
      .btn {
        color: #224466 !important;
        width: 30px;
        height: 40px !important;
      }
      .btn { height: 50px !important; }
    )");
    box.update();

    require_color(button->style_.text_color, 0x22 / 255.0f,
                  0x44 / 255.0f, 0x66 / 255.0f);
    check(approx_eq(button->style_.width, 10.0f, 0.0f));
    check(approx_eq(button->style_.height, 20.0f, 0.0f));
  }
}

spec("Box manages named stylesheet lifecycle atomically") {
  it("loads replaces and removes a stylesheet by handle") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    auto loaded = box.load_stylesheet("#root { width: 40px; }");
    check_true(loaded.applied);
    check(loaded.stylesheet_id != 0);
    box.update();
    check(approx_eq(root->style_.width, 40.0f, 0.0f));

    auto replaced = box.replace_stylesheet(
        loaded.stylesheet_id, "#root { width: 70px; }");
    check_true(replaced.applied);
    box.update();
    check(approx_eq(root->style_.width, 70.0f, 0.0f));

    check_true(box.remove_stylesheet(loaded.stylesheet_id));
    box.update();
    check(root->style_.width_size.kind == CssSizeKind::Auto);
    check_false(box.remove_stylesheet(loaded.stylesheet_id));
  }

  it("keeps the previous stylesheet when strict replacement has diagnostics") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    auto loaded = box.load_stylesheet("#root { width: 40px; }");
    auto rejected = box.replace_stylesheet(
        loaded.stylesheet_id,
        "#root { widht: 90px; width: 12wat; }",
        CssLoadOptions{"theme.css", true});

    check_false(rejected.applied);
    check_false(rejected.diagnostics.empty());
    check(rejected.diagnostics.front().source == "theme.css");
    box.update();
    check(approx_eq(root->style_.width, 40.0f, 0.0f));
  }
}

spec("StyleEngine preserves typed CSS size semantics") {
  it("distinguishes auto zero percentage and expression values") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* zero = box.create("div", "zero");
    auto* percent = box.create("div", "percent");
    auto* expression = box.create("div", "expression");
    auto* variable_expression = box.create("div", "variable-expression");
    root->append(zero);
    root->append(percent);
    root->append(expression);
    root->append(variable_expression);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    auto loaded = box.load_stylesheet(R"(
      #root { width: auto; }
      #zero { width: 0; }
      #percent { width: 50%; }
      #expression { width: calc(20px + 5px); }
      #variable-expression {
        --gutter: 16px;
        width: calc(100% - var(--gutter));
      }
    )", CssLoadOptions{"typed-size.css", true});
    check_true(loaded.applied);
    box.update();

    check(root->style_.width_size.kind == CssSizeKind::Auto);
    check(zero->style_.width_size.kind == CssSizeKind::Length);
    check(percent->style_.width_size.kind == CssSizeKind::Percentage);
    check(expression->style_.width_size.kind == CssSizeKind::Expression);
    check(variable_expression->style_.width_size.kind == CssSizeKind::Expression);
    check(approx_eq(expression->style_.width, 25.0f, 0.0f));
    check(approx_eq(variable_expression->layout_width(), 304.0f, 0.0f));
  }
}

spec("Box delegates CSS view lifecycle to ViewPipeline") {
  it("keeps style layout usable without a backend renderer") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 120px;
        height: 48px;
      }
    )");
    box.update();

    check(box.lifecycle_state() == ViewLifecycleState::LaidOut);
    check(approx_eq(root->layout_width(), 120.0f, 0.0f));
    check(approx_eq(root->layout_height(), 48.0f, 0.0f));
  }
}

spec("Box skips time updates for widgets inside hidden subtrees") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* visible = box.create_widget<CountingWidget>("div", "visible");
    auto* hidden_panel = box.create("div", "hidden-panel");
    hidden_panel->add_class("hidden");
    auto* hidden = box.create_widget<CountingWidget>("div", "hidden");
    hidden_panel->append(hidden);
    root->append(visible);
    root->append(hidden_panel);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: flex;
        width: 320px;
        height: 200px;
      }
      #visible,
      #hidden,
      #hidden-panel {
        width: 20px;
        height: 20px;
      }
      .hidden {
        display: none;
      }
    )");
    box.update();
    box.update_time(16.0f);

    check(static_cast<CountingWidget*>(visible->widget)->updates == 1);
    check(static_cast<CountingWidget*>(hidden->widget)->updates == 0);
  }
}

spec("StyleEngine recomputes pseudo-class styles from baseline") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "submit");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .btn {
        background-color: #111111;
        width: 40px;
      }
      .btn:hover {
        background-color: #222222;
        width: 80px;
      }
    )");
    box.update();

    require_color(button->style_.background_color, 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    check(approx_eq(button->style_.width, 40.0f, 0.0));
    check_false(button->dirty_style());

    button->set_hover(true);
    check(button->dirty_style());
    box.update();

    require_color(button->style_.background_color, 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);
    check(approx_eq(button->style_.width, 80.0f, 0.0));
    check(approx_eq(button->layout_width(), 80.0f, 0.0));
    check_false(button->dirty_style());

    button->set_hover(false);
    check(button->dirty_style());
    box.update();

    require_color(button->style_.background_color, 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    check(approx_eq(button->style_.width, 40.0f, 0.0));
    check(approx_eq(button->layout_width(), 40.0f, 0.0));
    check_false(button->dirty_style());
  }
}

spec("StyleEngine keeps selector-independent pseudo changes out of layout") {
  it("marks only widget paint when no selector depends on the state") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create_widget<ButtonWidget>("button", "submit");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .btn {
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
    button->clear_dirty();

    button->set_hover(true);

    check_false(button->dirty_style());
    check_false(button->dirty_layout());
    check(button->dirty_paint());
  }

  it("skips paint for static widgets when no selector depends on the state") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* label = box.create_widget<LabelWidget>("label", "caption", "Caption");
    label->add_class("caption");
    root->append(label);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .caption {
        width: 100px;
        height: 20px;
      }
    )");
    box.update();
    label->clear_dirty();

    label->set_hover(true);

    check_false(label->dirty_style());
    check_false(label->dirty_layout());
    check_false(label->dirty_paint());
  }
}

spec("EventDispatcher ignores pointer-events none overlays from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "button");
    auto* overlay = box.create("div", "overlay");
    root->append(button);
    root->append(overlay);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    bool clicked = false;
    button->on_click([&]() { clicked = true; });

    box.load_css(R"(
      #button {
        width: 100px;
        height: 40px;
      }
      #overlay {
        pointer-events: none;
        position: absolute;
        left: 0;
        top: 0;
        width: 100px;
        height: 40px;
      }
    )");
    box.update();

    Event down = Event::mouse_down(10.0f, 10.0f);
    box.dispatch_event(down);
    Event up = Event::mouse_up(10.0f, 10.0f);
    box.dispatch_event(up);

    check(clicked);
  }
}

spec("EventDispatcher hit tests CSS z-index in paint order") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* front = box.create("button", "front");
    auto* back = box.create("button", "back");
    root->append(front);
    root->append(back);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    bool front_clicked = false;
    bool back_clicked = false;
    front->on_click([&]() { front_clicked = true; });
    back->on_click([&]() { back_clicked = true; });

    box.load_css(R"(
      #root {
        position: relative;
        width: 160px;
        height: 120px;
      }
      #front,
      #back {
        position: absolute;
        left: 0;
        top: 0;
        width: 100px;
        height: 40px;
      }
      #front {
        z-index: 10;
      }
      #back {
        z-index: 1;
      }
    )");
    box.update();

    Event down = Event::mouse_down(10.0f, 10.0f);
    box.dispatch_event(down);
    Event up = Event::mouse_up(10.0f, 10.0f);
    box.dispatch_event(up);

    check(front_clicked);
    check_false(back_clicked);
  }
}

spec("EventDispatcher respects CSS clip-path inset during hit testing") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* back = box.create("button", "back");
    auto* front = box.create("button", "front");
    root->append(back);
    root->append(front);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    int front_clicks = 0;
    int back_clicks = 0;
    front->on_click([&]() { ++front_clicks; });
    back->on_click([&]() { ++back_clicks; });

    box.load_css(R"(
      #root {
        position: relative;
        width: 160px;
        height: 120px;
      }
      #front,
      #back {
        position: absolute;
        left: 0;
        top: 0;
        width: 100px;
        height: 40px;
      }
      #front {
        z-index: 10;
        clip-path: inset(0 50% 0 0);
      }
      #back {
        z-index: 1;
      }
    )");
    box.update();

    Event front_down = Event::mouse_down(25.0f, 10.0f);
    box.dispatch_event(front_down);
    Event front_up = Event::mouse_up(25.0f, 10.0f);
    box.dispatch_event(front_up);

    check(front_clicks == 1);
    check(back_clicks == 0);

    Event back_down = Event::mouse_down(75.0f, 10.0f);
    box.dispatch_event(back_down);
    Event back_up = Event::mouse_up(75.0f, 10.0f);
    box.dispatch_event(back_up);

    check(front_clicks == 1);
    check(back_clicks == 1);
  }
}

spec("EventDispatcher hit tests CSS transformed elements at rendered positions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "button");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    bool clicked = false;
    button->on_click([&]() { clicked = true; });

    box.load_css(R"(
      #root {
        width: 160px;
        height: 120px;
      }
      #button {
        width: 20px;
        height: 20px;
        transform: translateX(40px);
      }
    )");
    box.update();

    Event down = Event::mouse_down(50.0f, 10.0f);
    box.dispatch_event(down);
    Event up = Event::mouse_up(50.0f, 10.0f);
    box.dispatch_event(up);

    check(clicked);
  }
}

spec("EventDispatcher maps scaled and rotated CSS transform hits to local boxes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* scaled = box.create("button", "scaled");
    auto* rotated = box.create("button", "rotated");
    root->append(scaled);
    root->append(rotated);
    box.set_root(root);
    box.set_viewport(240.0f, 160.0f);

    int scaled_clicks = 0;
    int rotated_clicks = 0;
    scaled->on_click([&]() { ++scaled_clicks; });
    rotated->on_click([&]() { ++rotated_clicks; });

    box.load_css(R"(
      #root {
        position: relative;
        width: 160px;
        height: 120px;
      }
      #scaled {
        position: absolute;
        left: 0;
        top: 0;
        width: 20px;
        height: 20px;
        transform-origin: left top;
        transform: scale(2);
      }
      #rotated {
        position: absolute;
        left: 0;
        top: 0;
        width: 40px;
        height: 20px;
        transform-origin: left top;
        transform: translateX(80px) rotate(90deg);
      }
    )");
    box.update();

    Event scaled_down = Event::mouse_down(35.0f, 10.0f);
    box.dispatch_event(scaled_down);
    Event scaled_up = Event::mouse_up(35.0f, 10.0f);
    box.dispatch_event(scaled_up);
    check(scaled_clicks == 1);
    check(rotated_clicks == 0);

    Event rotated_down = Event::mouse_down(70.0f, 30.0f);
    box.dispatch_event(rotated_down);
    Event rotated_up = Event::mouse_up(70.0f, 30.0f);
    box.dispatch_event(rotated_up);
    check(scaled_clicks == 1);
    check(rotated_clicks == 1);
  }
}

spec("SliderWidget maps pointer values through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* slider_elem =
        box.create_widget<SliderWidget>("div", "slider", 0.0f, 100.0f,
                                        0.0f, 0.0f);
    auto* slider = static_cast<SliderWidget*>(slider_elem->widget);
    root->append(slider_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 200px;
        height: 80px;
      }
      #slider {
        width: 120px;
        height: 20px;
        transform: translateX(40px);
      }
    )");
    box.update();

    Event down = Event::mouse_down(100.0f, 10.0f);
    box.dispatch_event(down);
    Event up = Event::mouse_up(100.0f, 10.0f);
    box.dispatch_event(up);

    check(approx_eq(slider->value(), 50.0f, 0.001f));
  }
}

spec("SliderWidget exposes stable semantic parts to CSS") {
  it("matches widget-owned child elements through normal selectors") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* slider_elem = box.create_widget<SliderWidget>(
        "slider", "slider", 0.0f, 100.0f, 50.0f, 1.0f);
    auto* slider = static_cast<SliderWidget*>(slider_elem->widget);
    root->append(slider_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);

    box.load_css(R"(
      #root { width: 240px; height: 80px; }
      #slider {
        width: 120px;
        height: 28px;
        accent-color: #ff3300;
      }
      #slider > track { height: 6px; background-color: #112233; }
      #slider > thumb {
        width: 18px;
        height: 16px;
        border-color: #778899;
        pointer-events: auto;
      }
    )");
    box.update();

    check(slider_elem->child_count() == 4);
    check(box.query_selector("#slider > track") == slider->track_element());
    check(box.query_selector("[role=slider] > fill") == slider->fill_element());
    check(box.query_selector("#missing, #slider > fill") ==
          slider->fill_element());
    check(box.query_selector_all("#slider > *").size() == 4);
    check(box.query_selector_all("#slider > track, #slider > thumb").size() ==
          2);
    check(slider->track_element()->is_widget_owned());
    check(slider->track_element()->parent_elem() == slider_elem);
    check(*slider->track_element()->attribute("part") == "track");
    check(slider->fill_element()->computed_style->get_variable(
              Symbol("--track-fill")) == "255, 51, 0, 255");
    require_color(slider->track_element()->computed_style->background_color,
                  0x11 / 255.0f, 0x22 / 255.0f, 0x33 / 255.0f);
    require_color(slider->fill_element()->computed_style->background_color,
                  1.0f, 0x33 / 255.0f, 0.0f);
    require_color(slider->thumb_element()->computed_style->border_color,
                  0x77 / 255.0f, 0x88 / 255.0f, 0x99 / 255.0f);
    check(approx_eq(slider->track_element()->height(), 6.0f, 0.001f));
    check(approx_eq(slider->fill_element()->width(), 60.0f, 0.001f));
    check(approx_eq(slider->thumb_element()->width(), 18.0f, 0.001f));
    check(approx_eq(slider->thumb_element()->height(), 16.0f, 0.001f));

    auto* other = box.create("div", "other");
    check(other->append(slider->track_element()) == nullptr);
    check(slider->track_element()->parent_elem() == slider_elem);
    check_false(slider_elem->remove(slider->track_element()));
    check(slider->track_element()->parent_elem() == slider_elem);
    slider_elem->remove_child(slider->fill_element());
    slider_elem->clear_children();
    check(slider_elem->child_count() == 4);
    auto* injected = box.create("span", "injected");
    check(slider->track_element()->append(injected) == nullptr);
    check(injected->parent_elem() == nullptr);

    Event hover_thumb = Event::mouse_move(60.0f, 14.0f);
    box.dispatch_event(hover_thumb);
    check(box.hovered_element() == slider_elem);

    slider_elem->clear_dirty(flex::DirtyFlags::Visual);
    check_false(slider_elem->dirty_paint());
    slider->set_value(75.0f);
    check(slider_elem->dirty_paint());
    box.update();
    check(approx_eq(slider->fill_element()->width(), 90.0f, 0.001f));

    slider_elem->clear_dirty(flex::DirtyFlags::Visual);
    slider->set_max(150.0f);
    check(slider_elem->dirty_paint());
    box.update();
    check(approx_eq(slider->fill_element()->width(), 60.0f, 0.001f));

    slider_elem->clear_dirty(flex::DirtyFlags::Visual);
    slider->set_disabled(true);
    check(slider_elem->dirty_paint());
    check(slider_elem->has_attribute("disabled"));
  }
}

spec("ProgressBarWidget exposes semantic track and fill parts") {
  it("styles normal child elements and updates their geometry from state") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* progress_elem = box.create_widget<ProgressBarWidget>(
        "progress", "progress", 25.0f, false);
    auto* progress = static_cast<ProgressBarWidget*>(progress_elem->widget);
    root->append(progress_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 100.0f);
    box.load_css(R"(
      #root { width: 240px; height: 80px; }
      #progress { width: 200px; height: 24px; accent-color: #22c55e; }
      #progress > track { height: 10px; background-color: #1f2937; }
      #progress > fill { height: 6px; }
    )");
    box.update();

    check(progress_elem->child_count() == 2);
    check(box.query_selector("#progress > track") == progress->track_element());
    check(box.query_selector("#progress > fill") == progress->fill_element());
    require_color(progress->track_element()->computed_style->background_color,
                  0x1f / 255.0f, 0x29 / 255.0f, 0x37 / 255.0f);
    require_color(progress->fill_element()->computed_style->background_color,
                  0x22 / 255.0f, 0xc5 / 255.0f, 0x5e / 255.0f);
    check(approx_eq(progress->track_element()->height(), 10.0f, 0.001f));
    check(approx_eq(progress->fill_element()->height(), 6.0f, 0.001f));
    check(approx_eq(progress->fill_element()->width(), 50.0f, 0.001f));

    progress_elem->clear_dirty(flex::DirtyFlags::Visual);
    check_false(progress_elem->dirty_paint());
    progress->set_value(60.0f);
    check(progress_elem->dirty_paint());
    box.update();
    check(approx_eq(progress->fill_element()->width(), 120.0f, 0.001f));
  }
}

spec("Core controls expose stable semantic parts to CSS") {
  it("styles button checkbox switch input and textarea through child selectors") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button_elem =
        box.create_widget<ButtonWidget>("button", "parts-button", "Save");
    auto* checkbox_elem = box.create_widget<CheckboxWidget>(
        "checkbox", "parts-checkbox", "Terms", true);
    auto* switch_elem = box.create_widget<SwitchWidget>(
        "switch", "parts-switch", "Power", true);
    auto* input_elem =
        box.create_widget<InputWidget>("input", "parts-input", "Search");
    auto* textarea_elem = box.create_widget<TextAreaWidget>(
        "textarea", "parts-textarea", "Notes", "Write here");
    root->append(button_elem);
    root->append(checkbox_elem);
    root->append(switch_elem);
    root->append(input_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(480.0f, 360.0f);

    box.load_css(R"(
      #root { display: flex; flex-direction: column; width: 360px; }
      #parts-button { width: 140px; height: 36px; }
      #parts-button > label { color: #123456; }
      #parts-checkbox { width: 140px; height: 24px; }
      #parts-checkbox > box { width: 18px; height: 16px; background-color: #112233; }
      #parts-checkbox > indicator { color: #fefefe; }
      #parts-switch { width: 140px; height: 28px; }
      #parts-switch > track { width: 46px; height: 24px; background-color: #223344; }
      #parts-switch > thumb { width: 18px; height: 18px; background-color: #ddeeff; }
      #parts-input { width: 180px; height: 36px; }
      #parts-input > text { color: #345678; }
      #parts-input > caret { background-color: #abcdef; }
      #parts-textarea { width: 180px; height: 72px; }
      #parts-textarea > text { color: #456789; }
      #parts-textarea > selection-layer { background-color: #334455; }
    )");
    box.update();

    auto* button = static_cast<ButtonWidget*>(button_elem->widget);
    auto* checkbox = static_cast<CheckboxWidget*>(checkbox_elem->widget);
    auto* toggle = static_cast<SwitchWidget*>(switch_elem->widget);
    auto* input = static_cast<InputWidget*>(input_elem->widget);
    auto* textarea = static_cast<TextAreaWidget*>(textarea_elem->widget);

    check(button_elem->child_count() == 3);
    check(checkbox_elem->child_count() == 3);
    check(switch_elem->child_count() == 3);
    check(input_elem->child_count() == 5);
    check(textarea_elem->child_count() == 5);
    check(box.query_selector("#parts-button > label") == button->label_element());
    check(box.query_selector("#parts-checkbox > box") == checkbox->box_element());
    check(box.query_selector("#parts-switch > thumb") == toggle->thumb_element());
    check(box.query_selector("#parts-input > caret") == input->caret_element());
    check(box.query_selector("#parts-textarea > selection-layer") ==
          textarea->selection_layer_element());
    check(button->label_element()->is_widget_owned());
    check(input->viewport_element()->is_widget_owned());
    require_color(button->label_element()->computed_style->text_color,
                  0x12 / 255.0f, 0x34 / 255.0f, 0x56 / 255.0f);
    require_color(checkbox->box_element()->computed_style->background_color,
                  0x11 / 255.0f, 0x22 / 255.0f, 0x33 / 255.0f);
    require_color(toggle->thumb_element()->computed_style->background_color,
                  0xdd / 255.0f, 0xee / 255.0f, 1.0f);
    require_color(input->caret_element()->computed_style->background_color,
                  0xab / 255.0f, 0xcd / 255.0f, 0xef / 255.0f);
    require_color(textarea->selection_layer_element()->computed_style->background_color,
                  0x33 / 255.0f, 0x44 / 255.0f, 0x55 / 255.0f);
    check(approx_eq(checkbox->box_element()->width(), 18.0f, 0.001f));
    check(approx_eq(toggle->track_element()->width(), 46.0f, 0.001f));
    check(approx_eq(toggle->thumb_element()->width(), 18.0f, 0.001f));
    check(checkbox->label_element()->width() > 0.0f);
    check(toggle->label_element()->width() > 0.0f);
    check(approx_eq(input->viewport_element()->width(), 180.0f, 0.001f));
    check(approx_eq(textarea->text_element()->height(), 72.0f, 0.001f));
  }
}

spec("Composite controls bridge host CSS into semantic parts") {
  it("keeps standard control styling useful while preserving direct part CSS") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* checkbox_elem = box.create_widget<CheckboxWidget>(
        "checkbox", "host-checkbox", "Status", true);
    auto* switch_elem = box.create_widget<SwitchWidget>(
        "switch", "host-switch", "Power", true);
    root->append(checkbox_elem);
    root->append(switch_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);
    box.load_css(R"(
      #root { display: flex; width: 300px; height: 100px; }
      #host-checkbox {
        height: 20px;
        background-color: #2563eb;
        border-color: #1d4ed8;
        border-width: 1px;
        border-radius: 5px;
      }
      #host-switch {
        height: 24px;
        --switch-width: 44px;
        --switch-height: 24px;
        background-color: #22c55e;
        border-color: #15803d;
        border-width: 1px;
      }
      #host-switch > thumb { background-color: #f8fafc; }
    )");
    box.update();

    auto* checkbox = static_cast<CheckboxWidget*>(checkbox_elem->widget);
    auto* toggle = static_cast<SwitchWidget*>(switch_elem->widget);
    check(checkbox_elem->computed_style->get_variable(
              Symbol("--host-control-background")) == "37, 99, 235, 255");
    check(checkbox_elem->computed_style->get_variable(
              Symbol("--host-control-border-color")) == "29, 78, 216, 255");
    check(switch_elem->computed_style->get_variable(
              Symbol("--host-control-background")) == "34, 197, 94, 255");
    check(approx_eq(checkbox->box_element()->computed_style->background_color.r,
                    0x25 / 255.0f, 0.001f));
    check(approx_eq(checkbox->box_element()->computed_style->border_color.r,
                    0x1d / 255.0f, 0.001f));
    check(approx_eq(toggle->track_element()->computed_style->background_color.r,
                    0x22 / 255.0f, 0.001f));
    check(approx_eq(toggle->track_element()->computed_style->border_color.r,
                    0x15 / 255.0f, 0.001f));
    require_color(checkbox->box_element()->computed_style->background_color,
                  0x25 / 255.0f, 0x63 / 255.0f, 0xeb / 255.0f);
    require_color(checkbox->box_element()->computed_style->border_color,
                  0x1d / 255.0f, 0x4e / 255.0f, 0xd8 / 255.0f);
    check(approx_eq(checkbox->box_element()->computed_style->border_width[0],
                    1.0f, 0.001f));
    require_color(toggle->track_element()->computed_style->background_color,
                  0x22 / 255.0f, 0xc5 / 255.0f, 0x5e / 255.0f);
    require_color(toggle->track_element()->computed_style->border_color,
                  0x15 / 255.0f, 0x80 / 255.0f, 0x3d / 255.0f);
    require_color(toggle->thumb_element()->computed_style->background_color,
                  0xf8 / 255.0f, 0xfa / 255.0f, 0xfc / 255.0f);
    check(checkbox->label_element()->width() > 0.0f);
    check(toggle->label_element()->width() > 0.0f);
  }
}

spec("Editable text widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* input_elem = box.create_widget<InputWidget>("input", "input");
    auto* textarea_elem =
        box.create_widget<TextAreaWidget>("textarea", "textarea");
    auto* input = static_cast<InputWidget*>(input_elem->widget);
    auto* textarea = static_cast<TextAreaWidget*>(textarea_elem->widget);
    input->set_text("abcd");
    textarea->set_text("abcd");
    root->append(input_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 240px;
        height: 120px;
      }
      #input,
      #textarea {
        width: 160px;
        height: 40px;
        font-size: 10px;
        padding: 8px;
        transform: translateX(40px);
      }
    )");
    box.update();

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    input->handle_event(Event::focus_in(), *input_elem);
    input->set_cursor_pos(1);
    input->get_caret_rect(*input_elem, x, y, w, h);
    input->set_cursor_pos(4);
    input->handle_event(Event::mouse_down(x + 40.1f, y + h * 0.5f),
                        *input_elem);
    input->handle_event(Event::mouse_up(x + 40.1f, y + h * 0.5f),
                        *input_elem);
    check(input->cursor_pos() == 1);

    textarea->set_cursor_position(1);
    textarea->get_caret_rect(*textarea_elem, x, y, w, h);
    textarea->set_cursor_position(4);
    textarea->handle_event(Event::mouse_down(x + 40.1f, y + h * 0.5f),
                           *textarea_elem);
    textarea->handle_event(Event::mouse_up(x + 40.1f, y + h * 0.5f),
                           *textarea_elem);
    check(textarea->cursor_position() == 1);
  }
}

spec("Segmented choice widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* tabs_elem = box.create_widget<TabsWidget>("div", "tabs");
    auto* tabs = static_cast<TabsWidget*>(tabs_elem->widget);
    tabs->add_tab("A", "a");
    tabs->add_tab("B", "b");
    tabs->add_tab("C", "c");
    std::vector<ToggleGroupWidget::Option> options{{"a", "A"},
                                                   {"b", "B"},
                                                   {"c", "C"}};
    auto* toggle_elem =
        box.create_widget<ToggleGroupWidget>("div", "toggle", options);
    auto* toggle = static_cast<ToggleGroupWidget*>(toggle_elem->widget);
    root->append(tabs_elem);
    root->append(toggle_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 180px;
        height: 100px;
      }
      #tabs,
      #toggle {
        width: 90px;
        height: 30px;
        font-size: 10px;
        transform: translateX(40px);
      }
    )");
    box.update();

    tabs->handle_event(Event::mouse_down(85.0f, 15.0f), *tabs_elem);
    check(tabs->active_index() == 1);

    toggle->handle_event(Event::mouse_down(85.0f, 45.0f), *toggle_elem);
    check(toggle->selected_index() == 1);
  }
}

spec("TabsWidget projects page selection through CSS state") {
  it("switches visible content on pointer input and keeps it after recompute") {
    Box box(nullptr);
    auto* root = box.create("div", "tabs-root");
    auto* tabs_elem = box.create_widget<TabsWidget>("tabs", "stateful-tabs");
    auto* tabs = static_cast<TabsWidget*>(tabs_elem->widget);
    auto* content = box.create("div", "tabs-content");
    auto* first = box.create("section", "first-page");
    auto* second = box.create("section", "second-page");
    first->add_class("tab-page");
    second->add_class("tab-page");

    tabs->add_tab("A", "first", first);
    tabs->add_tab("B", "second", second);
    content->append(first);
    content->append(second);
    root->append(tabs_elem);
    root->append(content);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #tabs-root { width: 300px; height: 180px; }
      #stateful-tabs { width: 300px; height: 48px; }
      .tab-page { display: flex; width: 300px; height: 100px; }
      .tab-page[data-state=inactive] { display: none; }
    )");
    box.update();

    check(*first->attribute("data-state") == "active");
    check(*first->attribute("aria-hidden") == "false");
    check_true(first->is_visible());
    check(*second->attribute("data-state") == "inactive");
    check(*second->attribute("aria-hidden") == "true");
    check_false(second->is_visible());

    check_true(tabs->handle_event(Event::mouse_down(50.0f, 24.0f),
                                  *tabs_elem));
    check(tabs->active_index() == 1);
    check_false(first->is_visible());

    box.update();
    box.update();

    check(*first->attribute("data-state") == "inactive");
    check(first->computed_style->display == Display::None);
    check_false(first->is_visible());
    check(*second->attribute("data-state") == "active");
    check(second->computed_style->display == Display::Flex);
    check_true(second->is_visible());
  }

  it("releases projected state when pages are no longer associated") {
    Box box(nullptr);
    auto* tabs_elem = box.create_widget<TabsWidget>("tabs", "tabs");
    auto* tabs = static_cast<TabsWidget*>(tabs_elem->widget);
    auto* first = box.create("section", "first");
    auto* second = box.create("section", "second");

    tabs->add_tab("A", "first", first);
    tabs->add_tab("B", "second", second);
    check_false(second->is_visible());

    tabs->remove_tab("first");
    check_null(first->attribute("data-state"));
    check_null(first->attribute("aria-hidden"));
    check_true(first->is_visible());
    check(*second->attribute("data-state") == "active");

    tabs->clear_tabs();
    check_null(second->attribute("data-state"));
    check_null(second->attribute("aria-hidden"));
    check_true(second->is_visible());
  }
}

spec("Composite navigation widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* stepper_elem =
        box.create_widget<StepperWidget>("div", "stepper", 1, 0, 3, 1);
    auto* pagination_elem =
        box.create_widget<PaginationWidget>("div", "pagination", 3, 1);
    auto* breadcrumb_elem =
        box.create_widget<BreadcrumbWidget>("div", "breadcrumb");
    auto* stepper = static_cast<StepperWidget*>(stepper_elem->widget);
    auto* pagination =
        static_cast<PaginationWidget*>(pagination_elem->widget);
    auto* breadcrumb =
        static_cast<BreadcrumbWidget*>(breadcrumb_elem->widget);
    breadcrumb->set_items({{"home", "Home"}, {"docs", "Docs"}});
    root->append(stepper_elem);
    root->append(pagination_elem);
    root->append(breadcrumb_elem);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 220px;
        height: 120px;
      }
      #stepper,
      #pagination,
      #breadcrumb {
        width: 140px;
        height: 30px;
        font-size: 10px;
        transform: translateX(40px);
      }
    )");
    box.update();

    RenderCommandList commands(flex::RendererCapabilities{});
    pagination->emit_render_commands(*pagination_elem, commands);
    breadcrumb->emit_render_commands(*breadcrumb_elem, commands);

    const float stepper_y = stepper_elem->absolute_y() + 15.0f;
    stepper->handle_event(Event::mouse_move(165.0f, stepper_y),
                          *stepper_elem);
    stepper->handle_event(Event::mouse_down(165.0f, stepper_y),
                          *stepper_elem);
    check(stepper->value() == 2);

    const float pagination_y = pagination_elem->absolute_y() + 15.0f;
    pagination->handle_event(Event::mouse_move(118.0f, pagination_y),
                             *pagination_elem);
    pagination->handle_event(Event::mouse_down(118.0f, pagination_y),
                             *pagination_elem);
    check(pagination->current_page() == 2);

    int clicked_index = -1;
    std::string clicked_id;
    breadcrumb->set_click_callback(
        [&](int index, const std::string& id) {
          clicked_index = index;
          clicked_id = id;
        });
    const float breadcrumb_y = breadcrumb_elem->absolute_y() + 15.0f;
    breadcrumb->handle_event(Event::mouse_move(45.0f, breadcrumb_y),
                             *breadcrumb_elem);
    breadcrumb->handle_event(Event::mouse_down(45.0f, breadcrumb_y),
                             *breadcrumb_elem);
    check(clicked_index == 0);
    check(clicked_id == "home");
  }
}

spec("List widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* list_elem = box.create_widget<ListViewWidget>("div", "list");
    auto* list = static_cast<ListViewWidget*>(list_elem->widget);
    list->add_item("first", "First");
    list->add_item("second", "Second");
    list->add_item("third", "Third");
    root->append(list_elem);
    box.set_root(root);
    box.set_viewport(240.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 160px;
        height: 120px;
      }
      #list {
        width: 100px;
        height: 40px;
        --listview-item-height: 20px;
        transform: translate(40px, 40px);
      }
    )");
    box.update();

    RenderCommandList commands(flex::RendererCapabilities{});
    list->emit_render_commands(*list_elem, commands);

    const float y = list_elem->absolute_y() + 70.0f;
    list->handle_event(Event::mouse_down(45.0f, y), *list_elem);

    check_false(list->items()[0].selected);
    check(list->items()[1].selected);
  }
}

spec("Simple composite widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button_elem =
        box.create_widget<GroupButtonWidget>("button", "group_button");
    auto* accordion_elem =
        box.create_widget<AccordionWidget>("div", "accordion");
    auto* toast_elem =
        box.create_widget<ToastWidget>("div", "toast", "Saved");
    auto* button = static_cast<GroupButtonWidget*>(button_elem->widget);
    auto* accordion = static_cast<AccordionWidget*>(accordion_elem->widget);
    auto* toast = static_cast<ToastWidget*>(toast_elem->widget);
    accordion->add_section("First", "first", 20.0f);
    accordion->add_section("Second", "second", 20.0f);
    toast->show();
    root->append(button_elem);
    root->append(accordion_elem);
    root->append(toast_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 220.0f);

    box.load_css(R"(
      #root {
        width: 180px;
        height: 180px;
      }
      #group_button,
      #accordion,
      #toast {
        width: 100px;
        height: 40px;
        transform: translate(40px, 40px);
      }
    )");
    box.update();

    bool clicked = false;
    button->set_click_callback([&]() { clicked = true; });
    const float button_y = button_elem->absolute_y() + 45.0f;
    button->handle_event(Event::mouse_down(45.0f, button_y), *button_elem);
    button->handle_event(Event::mouse_up(45.0f, button_y), *button_elem);
    check(clicked);

    const float accordion_y = accordion_elem->absolute_y() + 50.0f;
    accordion->handle_event(Event::mouse_down(45.0f, accordion_y),
                            *accordion_elem);
    check(accordion->is_expanded("first"));
    check_false(accordion->is_expanded("second"));

    const float toast_y = toast_elem->absolute_y() + 45.0f;
    toast->handle_event(Event::mouse_down(145.0f, toast_y), *toast_elem);
    check_false(toast->is_visible());
  }
}

spec("Data entry widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* picker_elem =
        box.create_widget<ColorPickerWidget>("div", "picker");
    auto* gradient_elem =
        box.create_widget<GradientEditorWidget>("div", "gradient");
    auto* picker = static_cast<ColorPickerWidget*>(picker_elem->widget);
    auto* gradient =
        static_cast<GradientEditorWidget*>(gradient_elem->widget);
    root->append(picker_elem);
    root->append(gradient_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 220.0f);

    box.load_css(R"(
      #root {
        width: 180px;
        height: 180px;
      }
      #picker {
        width: 140px;
        height: 100px;
        transform: translate(40px, 40px);
      }
      #gradient {
        width: 100px;
        height: 60px;
        transform: translate(40px, 40px);
      }
    )");
    box.update();

    picker->handle_event(Event::mouse_down(92.0f, 66.0f), *picker_elem);
    picker->handle_event(Event::mouse_up(92.0f, 66.0f), *picker_elem);
    check(approx_eq(picker->saturation(), 0.5f, 0.001f));
    check(approx_eq(picker->value(), 0.5f, 0.001f));

    gradient->handle_event(Event::mouse_down(90.0f, gradient_elem->absolute_y() + 52.0f),
                           *gradient_elem);
    check(gradient->stop_count() == 3);
    check(approx_eq(gradient->get_stop(1).offset, 0.5f, 0.001f));
  }
}

spec("Structured widgets map pointer positions through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* calendar_elem =
        box.create_widget<CalendarWidget>("div", "calendar");
    auto* sidebar_elem =
        box.create_widget<SidebarWidget>("div", "sidebar");
    auto* panel_elem =
        box.create_widget<PanelWidget>("div", "panel", "Tools");
    auto* table_elem = box.create_widget<TableWidget>("div", "table");
    auto* calendar = static_cast<CalendarWidget*>(calendar_elem->widget);
    auto* sidebar = static_cast<SidebarWidget*>(sidebar_elem->widget);
    auto* panel = static_cast<PanelWidget*>(panel_elem->widget);
    auto* table = static_cast<TableWidget*>(table_elem->widget);
    calendar->set_view_date({2024, 1, 1});
    sidebar->add_section("Main");
    sidebar->add_item("home", "", "Home");
    table->add_column("Name", 100.0f);
    table->add_row({"One"});
    table->add_row({"Two"});
    root->append(calendar_elem);
    root->append(sidebar_elem);
    root->append(panel_elem);
    root->append(table_elem);
    box.set_root(root);
    box.set_viewport(420.0f, 520.0f);

    box.load_css(R"(
      #root {
        width: 220px;
        height: 460px;
      }
      #calendar {
        width: 140px;
        height: 140px;
        transform: translate(40px, 40px);
      }
      #sidebar {
        width: 120px;
        height: 120px;
        transform: translate(40px, 40px);
      }
      #panel {
        position: absolute;
        left: 0;
        top: 260px;
        width: 120px;
        height: 60px;
        transform: translate(40px, 40px);
      }
      #table {
        width: 140px;
        height: 120px;
        transform: translate(40px, 40px);
      }
    )");
    box.update();

    RenderCommandList commands(flex::RendererCapabilities{});
    sidebar->emit_render_commands(*sidebar_elem, commands);

    calendar->handle_event(Event::mouse_down(90.0f, 125.0f), *calendar_elem);
    const Date selected = calendar->selected_date();
    check(selected.year == 2024);
    check(selected.month == 1);
    check(selected.day == 2);

    const float sidebar_y = sidebar_elem->absolute_y() + 80.0f;
    sidebar->handle_event(Event::mouse_move(50.0f, sidebar_y), *sidebar_elem);
    sidebar->handle_event(Event::mouse_down(50.0f, sidebar_y), *sidebar_elem);
    check(sidebar->selected() == "home");

    panel->handle_event(Event::mouse_down(50.0f, panel_elem->absolute_y() + 50.0f),
                        *panel_elem);
    panel->handle_event(Event::mouse_move(90.0f, panel_elem->absolute_y() + 80.0f),
                        *panel_elem);
    panel->handle_event(Event::mouse_up(90.0f, panel_elem->absolute_y() + 80.0f),
                        *panel_elem);
    check(approx_eq(panel_elem->computed_style->left, 40.0f, 0.001f));

    table->handle_event(Event::mouse_down(45.0f, table_elem->absolute_y() + 144.0f),
                        *table_elem);
    check(table->selected_row() == 1);
  }
}

spec("Drag resize widgets map movement through parent CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* drag_elem = box.create_widget<DraggableWidget>("div", "drag");
    auto* panel_elem =
        box.create_widget<PanelWidget>("div", "panel", "Tools");
    auto* previous = box.create("div", "previous");
    auto* splitter_elem =
        box.create_widget<SplitterWidget>("div", "splitter");
    auto* next = box.create("div", "next");
    auto* drag = static_cast<DraggableWidget*>(drag_elem->widget);
    auto* panel = static_cast<PanelWidget*>(panel_elem->widget);
    auto* splitter = static_cast<SplitterWidget*>(splitter_elem->widget);
    root->append(drag_elem);
    root->append(panel_elem);
    root->append(previous);
    root->append(splitter_elem);
    root->append(next);
    box.set_root(root);
    box.set_viewport(520.0f, 320.0f);

    box.load_css(R"(
      #root {
        position: relative;
        display: flex;
        width: 240px;
        height: 160px;
        transform-origin: left top;
        transform: translate(40px, 20px) scale(2);
      }
      #drag {
        position: absolute;
        left: 10px;
        top: 10px;
        width: 20px;
        height: 20px;
      }
      #panel {
        position: absolute;
        left: 40px;
        top: 10px;
        width: 60px;
        height: 40px;
      }
      #previous {
        width: 120px;
        height: 40px;
      }
      #splitter {
        width: 8px;
        height: 40px;
      }
      #next {
        width: 80px;
        height: 40px;
      }
    )");
    box.update();

    const auto drag_down =
        detail::css_render_world_transform(drag_elem) * flex::Vec2(5.0f, 5.0f);
    drag->handle_event(Event::mouse_down(drag_down.x, drag_down.y),
                       *drag_elem);
    const auto drag_move =
        detail::css_render_content_world_transform(root) *
        flex::Vec2(25.0f, 25.0f);
    drag->handle_event(Event::mouse_move(drag_move.x, drag_move.y),
                       *drag_elem);
    drag->handle_event(Event::mouse_up(drag_move.x, drag_move.y), *drag_elem);
    check(approx_eq(drag_elem->computed_style->left, 20.0f, 0.001f));
    check(approx_eq(drag_elem->computed_style->top, 20.0f, 0.001f));

    const auto panel_down =
        detail::css_render_world_transform(panel_elem) *
        flex::Vec2(10.0f, 10.0f);
    panel->handle_event(Event::mouse_down(panel_down.x, panel_down.y),
                        *panel_elem);
    const auto panel_move =
        detail::css_render_content_world_transform(root) *
        flex::Vec2(70.0f, 30.0f);
    panel->handle_event(Event::mouse_move(panel_move.x, panel_move.y),
                        *panel_elem);
    panel->handle_event(Event::mouse_up(panel_move.x, panel_move.y),
                        *panel_elem);
    check(approx_eq(panel_elem->computed_style->left, 60.0f, 0.001f));
    check(approx_eq(panel_elem->computed_style->top, 20.0f, 0.001f));

    const auto split_down =
        detail::css_render_world_transform(splitter_elem) *
        flex::Vec2(4.0f, 10.0f);
    splitter->handle_event(Event::mouse_down(split_down.x, split_down.y),
                           *splitter_elem);
    const auto split_move =
        detail::css_render_content_world_transform(root) *
        flex::Vec2(splitter_elem->x() + 24.0f, splitter_elem->y() + 10.0f);
    splitter->handle_event(Event::mouse_move(split_move.x, split_move.y),
                           *splitter_elem);
    splitter->handle_event(Event::mouse_up(split_move.x, split_move.y),
                           *splitter_elem);
    check(approx_eq(previous->computed_style->width, 140.0f, 0.001f));
  }
}

spec("Drag widgets map movement through scrolled parent content coordinates") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* drag_elem = box.create_widget<DraggableWidget>("div", "drag");
    auto* filler = box.create("div", "filler");
    auto* drag = static_cast<DraggableWidget*>(drag_elem->widget);
    root->append(drag_elem);
    root->append(filler);
    box.set_root(root);
    box.set_viewport(240.0f, 180.0f);

    box.load_css(R"(
      #root {
        position: relative;
        overflow-y: scroll;
        width: 100px;
        height: 80px;
        transform: translate(40px, 20px);
      }
      #drag {
        position: absolute;
        left: 10px;
        top: 60px;
        width: 20px;
        height: 20px;
      }
      #filler {
        width: 100px;
        height: 200px;
      }
    )");
    box.update();
    check(root->set_scroll_offset(0.0f, 40.0f));

    const auto down =
        detail::css_render_world_transform(drag_elem) * flex::Vec2(5.0f, 5.0f);
    drag->handle_event(Event::mouse_down(down.x, down.y), *drag_elem);
    const auto move =
        detail::css_render_content_world_transform(root) *
        flex::Vec2(25.0f, 85.0f);
    drag->handle_event(Event::mouse_move(move.x, move.y), *drag_elem);
    drag->handle_event(Event::mouse_up(move.x, move.y), *drag_elem);

    check(approx_eq(drag_elem->computed_style->left, 20.0f, 0.001f));
    check(approx_eq(drag_elem->computed_style->top, 80.0f, 0.001f));
  }
}

spec("Overlay widgets anchor and hit-test through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* date_elem =
        box.create_widget<DatePickerWidget>("div", "date", Date{2024, 1, 1});
    auto* time_elem =
        box.create_widget<TimePickerWidget>("div", "time", Time{12, 0, 0});
    auto* search_elem =
        box.create_widget<SearchBoxWidget>("div", "search");
    auto* popover_elem =
        box.create_widget<PopoverWidget>("button", "popover", "Details");
    auto* toolbar_elem =
        box.create_widget<ToolbarWidget>("div", "toolbar");
    auto* dropdown_elem =
        box.create_widget<DropdownWidget>("div", "dropdown");
    auto* select_elem =
        box.create_widget<SelectWidget>("div", "select",
                                        std::vector<std::string>{"One", "Two"});
    auto* date = static_cast<DatePickerWidget*>(date_elem->widget);
    auto* time = static_cast<TimePickerWidget*>(time_elem->widget);
    auto* search = static_cast<SearchBoxWidget*>(search_elem->widget);
    auto* popover = static_cast<PopoverWidget*>(popover_elem->widget);
    auto* toolbar = static_cast<ToolbarWidget*>(toolbar_elem->widget);
    auto* dropdown = static_cast<DropdownWidget*>(dropdown_elem->widget);
    auto* select = static_cast<SelectWidget*>(select_elem->widget);
    search->add_suggestion("alpha", "Alpha");
    toolbar->add_dropdown("menu", "M", {{"new", "New"}});
    dropdown->add_option("One", "one");
    dropdown->add_option("Two", "two");
    root->append(date_elem);
    root->append(time_elem);
    root->append(search_elem);
    root->append(popover_elem);
    root->append(toolbar_elem);
    root->append(dropdown_elem);
    root->append(select_elem);
    box.set_root(root);
    box.set_viewport(360.0f, 520.0f);

    box.load_css(R"(
      #root {
        width: 180px;
        height: 440px;
      }
      #date,
      #time,
      #search,
      #popover,
      #toolbar,
      #dropdown,
      #select {
        width: 120px;
        height: 32px;
        font-size: 12px;
        --item-height: 32px;
        transform: translate(40px, 40px);
      }
    )");
    box.update();

    RenderCommandList commands(flex::RendererCapabilities{});

    date->handle_event(Event::mouse_down(date_elem->absolute_x() + 50.0f,
                                         date_elem->absolute_y() + 50.0f),
                       *date_elem);
    check(date->is_open());
    date->emit_overlay_commands(*date_elem, commands);
    const float date_overlay_x = date_elem->absolute_x() + 40.0f;
    const float date_overlay_y = date_elem->absolute_y() + 40.0f +
                                 date_elem->height() + 4.0f;
    date->handle_event(Event::mouse_down(date_overlay_x + 100.0f,
                                         date_overlay_y + 97.0f),
                       *date_elem);
    const Date picked = date->date();
    check(picked.year == 2024);
    check(picked.month == 1);
    check(picked.day > 1);

    time->handle_event(Event::mouse_down(time_elem->absolute_x() + 50.0f,
                                         time_elem->absolute_y() + 50.0f),
                       *time_elem);
    check(time->has_overlay());
    time->emit_overlay_commands(*time_elem, commands);
    const float time_overlay_x = time_elem->absolute_x() + 40.0f;
    const float time_overlay_y = time_elem->absolute_y() + 40.0f +
                                 time_elem->height() + 4.0f;
    time->handle_event(Event::mouse_down(time_overlay_x + 20.0f,
                                         time_overlay_y + 45.0f),
                       *time_elem);
    check(time->time().hour == 13);

    search->handle_event(Event::mouse_down(search_elem->absolute_x() + 50.0f,
                                           search_elem->absolute_y() + 50.0f),
                         *search_elem);
    check(search->has_overlay());
    search->emit_overlay_commands(*search_elem, commands);
    const float search_overlay_x = search_elem->absolute_x() + 40.0f;
    const float search_overlay_y = search_elem->absolute_y() + 40.0f +
                                   search_elem->height() + 2.0f;
    search->handle_event(Event::mouse_down(search_overlay_x + 20.0f,
                                           search_overlay_y + 12.0f),
                         *search_elem);
    check(search->text() == "Alpha");

    popover->handle_event(Event::mouse_down(popover_elem->absolute_x() + 50.0f,
                                            popover_elem->absolute_y() + 50.0f),
                          *popover_elem);
    check(popover->is_visible());

    toolbar->emit_render_commands(*toolbar_elem, commands);
    toolbar->handle_event(Event::mouse_down(toolbar_elem->absolute_x() + 50.0f,
                                            toolbar_elem->absolute_y() + 50.0f),
                          *toolbar_elem);
    check(toolbar->has_overlay());
    std::string toolbar_id;
    std::string item_id;
    toolbar->on_dropdown_select([&](const std::string& id,
                                    const std::string& item) {
      toolbar_id = id;
      item_id = item;
    });
    toolbar->emit_overlay_commands(*toolbar_elem, commands);
    const float toolbar_overlay_x = toolbar_elem->absolute_x() + 40.0f + 4.0f;
    const float toolbar_overlay_y = toolbar_elem->absolute_y() + 40.0f + 4.0f +
                                    toolbar_elem->height() - 8.0f + 2.0f;
    toolbar->handle_event(Event::mouse_down(toolbar_overlay_x + 20.0f,
                                            toolbar_overlay_y + 12.0f),
                          *toolbar_elem);
    check(toolbar_id == "menu");
    check(item_id == "new");

    dropdown->handle_event(Event::mouse_down(dropdown_elem->absolute_x() + 50.0f,
                                             dropdown_elem->absolute_y() + 50.0f),
                           *dropdown_elem);
    check(dropdown->is_open());
    dropdown->handle_event(Event::mouse_down(dropdown_elem->absolute_x() + 50.0f,
                                             dropdown_elem->absolute_y() + 40.0f +
                                                 dropdown_elem->height() + 4.0f +
                                                 52.0f),
                           *dropdown_elem);
    check(dropdown->selected_index() == 1);

    select->handle_event(Event::mouse_down(select_elem->absolute_x() + 50.0f,
                                           select_elem->absolute_y() + 50.0f),
                         *select_elem);
    check(select->is_expanded());
    select->handle_event(Event::mouse_down(select_elem->absolute_x() + 50.0f,
                                           select_elem->absolute_y() + 40.0f +
                                               32.0f + 48.0f),
                         *select_elem);
    check(select->selected_index() == 1);
  }
}

spec("Overlay widgets anchor to scaled CSS render bounds") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* search_elem = box.create_widget<SearchBoxWidget>("div", "search");
    auto* dropdown_elem = box.create_widget<DropdownWidget>("div", "dropdown");
    auto* select_elem = box.create_widget<SelectWidget>(
        "div", "select", std::vector<std::string>{"One", "Two"});
    auto* search = static_cast<SearchBoxWidget*>(search_elem->widget);
    auto* dropdown = static_cast<DropdownWidget*>(dropdown_elem->widget);
    auto* select = static_cast<SelectWidget*>(select_elem->widget);
    search->add_suggestion("alpha", "Alpha");
    dropdown->add_option("One", "one");
    dropdown->add_option("Two", "two");
    root->append(search_elem);
    root->append(dropdown_elem);
    root->append(select_elem);
    box.set_root(root);
    box.set_viewport(420.0f, 260.0f);

    box.load_css(R"(
      #root {
        width: 220px;
        height: 180px;
      }
      #search,
      #dropdown,
      #select {
        width: 120px;
        height: 32px;
        font-size: 12px;
        --item-height: 32px;
        transform-origin: left top;
        transform: translate(40px, 20px) scale(1.5, 1.25);
      }
    )");
    box.update();

    RenderCommandList commands(flex::RendererCapabilities{});

    const auto search_hit =
        detail::css_render_world_transform(search_elem) * flex::Vec2(50.0f, 16.0f);
    search->handle_event(Event::mouse_down(search_hit.x, search_hit.y), *search_elem);
    check(search->has_overlay());
    search->emit_overlay_commands(*search_elem, commands);
    const auto search_bounds = detail::css_render_world_bounds(search_elem);
    const auto* search_rect = first_rect_at(
        commands, search_bounds.x, search_bounds.y + search_bounds.height + 2.0f);
    check(search_rect != nullptr);
    if (search_rect) {
      check(approx_eq(search_rect->width, search_bounds.width, 0.001f));
    }
    search->handle_event(
        Event::mouse_down(search_bounds.x + 20.0f,
                          search_bounds.y + search_bounds.height + 14.0f),
        *search_elem);
    check(search->text() == "Alpha");

    const auto dropdown_hit =
        detail::css_render_world_transform(dropdown_elem) *
        flex::Vec2(50.0f, 16.0f);
    dropdown->handle_event(Event::mouse_down(dropdown_hit.x, dropdown_hit.y),
                           *dropdown_elem);
    check(dropdown->is_open());
    const auto dropdown_bounds = detail::css_render_world_bounds(dropdown_elem);
    dropdown->handle_event(
        Event::mouse_down(dropdown_bounds.x + 50.0f,
                          dropdown_bounds.y + dropdown_bounds.height + 56.0f),
        *dropdown_elem);
    check(dropdown->selected_index() == 1);

    const auto select_hit =
        detail::css_render_world_transform(select_elem) * flex::Vec2(50.0f, 16.0f);
    select->handle_event(Event::mouse_down(select_hit.x, select_hit.y), *select_elem);
    check(select->is_expanded());
    select->update(200.0f, *select_elem);
    const auto select_bounds = detail::css_render_world_bounds(select_elem);
    select->handle_event(
        Event::mouse_down(select_bounds.x + 50.0f,
                          select_bounds.y + select_bounds.height + 48.0f),
        *select_elem);
    check(select->selected_index() == 1);
  }
}

spec("EventDispatcher scrolls overflow containers from CSS mouse wheel") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* scroll = box.create("div", "scroll");
    auto* child = box.create("div", "child");
    root->append(scroll);
    scroll->append(child);
    box.set_root(root);
    box.set_viewport(240.0f, 160.0f);
  
    box.load_css(R"(
      #scroll {
        overflow-y: scroll;
        width: 120px;
        height: 60px;
      }
      #child {
        width: 120px;
        height: 160px;
      }
    )");
    box.update();
  
    check(scroll->can_scroll_y());
    check(approx_eq(scroll->max_scroll_y(), 100.0f, 0.1f));
  
    Event wheel = Event::mouse_wheel(10.0f, 10.0f, 0.0f, -1.0f);
    box.dispatch_event(wheel);
  
    check(wheel.handled);
    check(approx_eq(scroll->scroll_y(), 40.0f, 0.1f));
  }
}

spec("EventDispatcher prefers horizontal overflow scroll and falls back to scrollable ancestors") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* outer = box.create("div", "outer");
    auto* inner = box.create("div", "inner");
    auto* wide = box.create("div", "wide");
    auto* tail = box.create("div", "tail");
    root->append(outer);
    outer->append(inner);
    inner->append(wide);
    outer->append(tail);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #outer {
        overflow-y: scroll;
        width: 120px;
        height: 60px;
      }
      #inner {
        overflow-x: scroll;
        width: 100px;
        height: 30px;
      }
      #wide {
        width: 220px;
        height: 30px;
      }
      #tail {
        width: 120px;
        height: 120px;
      }
    )");
    box.update();
  
    check(inner->can_scroll_x());
    check(outer->can_scroll_y());
  
    Event first_wheel = Event::mouse_wheel(10.0f, 10.0f, 0.0f, -1.0f);
    box.dispatch_event(first_wheel);
  
    check(first_wheel.handled);
    check(approx_eq(inner->scroll_x(), 40.0f, 0.1f));
    check(approx_eq(outer->scroll_y(), 0.0f, 0.1f));
  
    check(inner->set_scroll_offset(inner->max_scroll_x(), 0.0f));
  
    Event second_wheel = Event::mouse_wheel(10.0f, 10.0f, 0.0f, -1.0f);
    box.dispatch_event(second_wheel);
  
    check(second_wheel.handled);
    check(approx_eq(outer->scroll_y(), 40.0f, 0.1f));
  }
}

spec("EventDispatcher scrolls focused overflow ancestors from keyboard paging keys") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* scroll = box.create("div", "scroll");
    auto* child = box.create("div", "child");
    root->append(scroll);
    scroll->append(child);
    box.set_root(root);
    box.set_viewport(240.0f, 160.0f);
  
    box.load_css(R"(
      #scroll {
        overflow-y: scroll;
        width: 120px;
        height: 60px;
      }
      #child {
        width: 120px;
        height: 180px;
      }
    )");
    box.update();
  
    check(scroll->can_scroll_y());
    box.set_focus(child);
  
    Event page_down = Event::key_down(KeyCode::PageDown);
    box.dispatch_event(page_down);
  
    check(page_down.handled);
    check(approx_eq(scroll->scroll_y(), 60.0f, 0.1f));
  
    Event end = Event::key_down(KeyCode::End);
    box.dispatch_event(end);
  
    check(end.handled);
    check(approx_eq(scroll->scroll_y(), scroll->max_scroll_y(), 0.1f));
  }
}

spec("EventDispatcher hit tests children inside scrolled overflow containers") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* scroll = box.create("div", "scroll");
    auto* button = box.create("button", "button");
    root->append(scroll);
    scroll->append(button);
    box.set_root(root);
    box.set_viewport(240.0f, 200.0f);
  
    bool clicked = false;
    button->on_click([&]() { clicked = true; });
  
    box.load_css(R"(
      #scroll {
        overflow-y: scroll;
        width: 120px;
        height: 60px;
      }
      #button {
        width: 120px;
        height: 30px;
        margin-top: 80px;
      }
    )");
    box.update();
    check(scroll->can_scroll_y());
    check(scroll->set_scroll_offset(0.0f, 50.0f));
  
    Event down = Event::mouse_down(10.0f, 40.0f);
    box.dispatch_event(down);
    Event up = Event::mouse_up(10.0f, 40.0f);
    box.dispatch_event(up);
  
    check(clicked);
  }
}

spec("StyleEngine parses scroll padding and scroll margin properties") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* scroll = box.create("div", "scroll");
    auto* target = box.create("button", "target");
    root->append(scroll);
    scroll->append(target);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #scroll {
        scroll-padding: 8px 12px;
        scroll-padding-inline: 14px 18px;
        scroll-padding-block-end: 20px;
      }
      #target {
        scroll-margin: 4px 6px 8px 10px;
        scroll-margin-inline-end: 16px;
        scroll-margin-block: 12px 24px;
      }
    )");
    box.update();

    check(scroll->computed_style->get_variable(Symbol("--scroll-padding-top")) == "8px");
    check(scroll->computed_style->get_variable(Symbol("--scroll-padding-right")) == "18px");
    check(scroll->computed_style->get_variable(Symbol("--scroll-padding-bottom")) == "20px");
    check(scroll->computed_style->get_variable(Symbol("--scroll-padding-left")) == "14px");

    check(target->computed_style->get_variable(Symbol("--scroll-margin-top")) == "12px");
    check(target->computed_style->get_variable(Symbol("--scroll-margin-right")) == "16px");
    check(target->computed_style->get_variable(Symbol("--scroll-margin-bottom")) == "24px");
    check(target->computed_style->get_variable(Symbol("--scroll-margin-left")) == "10px");
  }
}

spec("EventDispatcher uses scroll padding and scroll margin when focusing targets") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* scroll = box.create("div", "scroll");
    auto* spacer = box.create("div", "spacer");
    auto* target = box.create("button", "target");
    root->append(scroll);
    scroll->append(spacer);
    scroll->append(target);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #scroll {
        overflow-y: scroll;
        width: 120px;
        height: 60px;
        scroll-padding-top: 10px;
        scroll-padding-bottom: 6px;
      }
      #spacer {
        width: 120px;
        height: 100px;
      }
      #target {
        width: 120px;
        height: 20px;
        scroll-margin-top: 8px;
        scroll-margin-bottom: 4px;
      }
    )");
    box.update();

    check(scroll->can_scroll_y());
    check(approx_eq(scroll->max_scroll_y(), 60.0f, 0.1f));
    check(approx_eq(scroll->scroll_y(), 0.0f, 0.1f));

    box.set_focus(target);

    check(approx_eq(scroll->scroll_y(), 60.0f, 0.1f));
    check(approx_eq(scroll->max_scroll_y(), 60.0f, 0.1f));

    box.set_focus(nullptr);
    check(scroll->set_scroll_offset(0.0f, 0.0f));
    box.set_focus(target);
    check(approx_eq(scroll->scroll_y(), 60.0f, 0.1f));
  }
}

spec("StyleEngine supports selector lists and inherited variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* label = box.create("label", "name");
    auto* badge = box.create("span", "badge");
    badge->add_class("pill");
    root->append(label);
    root->append(badge);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      div {
        --brand: #3366ff;
        color: #101010;
        font-size: 18px;
      }
      label, .pill {
        background-color: var(--brand);
      }
    )");
    box.update();
  
    require_color(label->style_.text_color, 0x10 / 255.0f, 0x10 / 255.0f,
                  0x10 / 255.0f);
    check(approx_eq(label->style_.font_size, 18.0f, 0.0));
    require_color(label->style_.background_color, 0x33 / 255.0f,
                  0x66 / 255.0f, 1.0f);
    require_color(badge->style_.background_color, 0x33 / 255.0f,
                  0x66 / 255.0f, 1.0f);
  }
}

spec("StyleEngine matches simple selectors and selector lists") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* plain = box.create("section", "plain");
    auto* button = box.create("button", "submit");
    auto* label = box.create("label", "name");
    auto* badge = box.create("span", "badge");
    button->add_class("primary");
    badge->add_class("pill");
    root->append(plain);
    root->append(button);
    root->append(label);
    root->append(badge);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      * { width: 5px; }
      button { color: #112233; }
      .primary { background-color: #223344; }
      #submit { width: 25px; }
      label, .pill { font-size: 18px; }
    )");
    box.update();
  
    check(approx_eq(plain->style_.width, 5.0f, 0.0));
    require_color(button->style_.text_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
    require_color(button->style_.background_color, 0x22 / 255.0f,
                  0x33 / 255.0f, 0x44 / 255.0f);
    check(approx_eq(button->style_.width, 25.0f, 0.0));
    check(approx_eq(label->style_.font_size, 18.0f, 0.0));
    check(approx_eq(badge->style_.font_size, 18.0f, 0.0));
  }
}

spec("StyleEngine does not inherit projected layout properties") {
  it("keeps custom properties inherited without leaking flex state") {
    Box box(nullptr);
    auto* parent = box.create("div", "parent");
    auto* child = box.create("div", "child");
    parent->append(child);
    box.set_root(parent);
    box.set_viewport(640.0f, 320.0f);

    box.load_css(R"(
      #parent {
        display: flex;
        flex: 1 1 360px;
        align-self: center;
        min-width: 240px;
        grid-column: 2;
        --surface-token: #123456;
      }
    )");
    box.update();

    check(parent->computed_style->get_variable(Symbol("flex-grow"), "") == "1");
    check(child->computed_style->get_variable(Symbol("flex-grow"), "").empty());
    check(child->computed_style->get_variable(Symbol("flex-shrink"), "").empty());
    check(child->computed_style->get_variable(Symbol("flex-basis"), "").empty());
    check(child->computed_style->get_variable(Symbol("align-self"), "").empty());
    check(child->computed_style->get_variable(Symbol("min-width"), "").empty());
    check(child->computed_style->get_variable(Symbol("grid-column"), "").empty());
    check(child->computed_style->get_variable(Symbol("--surface-token"), "") ==
          "#123456");
  }
}

spec("StyleEngine honors state layer rules loaded after utilities") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* pane = box.create("div", "pane");
    pane->add_class("flex");
    pane->add_class("hidden");
    root->append(pane);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(".hidden { display: none; }");
    box.load_css(".flex { display: flex; }");
    box.load_css(".hidden { display: none; }");
    box.update();

    check(pane->computed_style->display == Display::None);
    check_false(pane->is_visible());
  }
}

spec("LayoutManager keeps demo shell columns and hidden panes stable") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* sidebar = box.create("div", "sidebar");
    auto* main = box.create("div", "main");
    auto* container = box.create("div", "container");
    root->append(sidebar);
    root->append(main);
    main->append(container);

    root->add_class("flex");
    root->add_class("flex-row");
    root->add_class("w-root");
    root->add_class("h-root");
    sidebar->add_class("flex");
    sidebar->add_class("flex-col");
    sidebar->add_class("w-sidebar");
    sidebar->add_class("h-root");
    main->add_class("flex");
    main->add_class("flex-col");
    main->add_class("w-main");
    main->add_class("h-root");
    main->add_class("p-main");
    container->add_class("flex");
    container->add_class("relative");
    container->add_class("w-container");
    container->add_class("h-container");

    Element* panes[4] = {};
    for (int i = 0; i < 4; ++i) {
      panes[i] = box.create("div");
      panes[i]->add_class("flex");
      panes[i]->add_class("w-container");
      panes[i]->add_class("h-container");
      if (i > 0) {
        panes[i]->add_class("hidden");
      }
      container->append(panes[i]);
    }

    box.set_root(root);
    box.set_viewport(1200.0f, 900.0f);
    box.load_css(R"(
      * { box-sizing: border-box; }
      .hidden { display: none; }
      .flex { display: flex; }
      .flex-row { flex-direction: row; }
      .flex-col { flex-direction: column; }
      .relative { position: relative; }
      .w-root { width: 1200px; }
      .h-root { height: 900px; }
      .w-sidebar { width: 260px; }
      .w-main { width: 940px; }
      .p-main { padding: 40px; }
      .w-container { width: 860px; }
      .h-container { height: 800px; }
    )");
    box.load_css(".hidden { display: none; }");
    box.update();

    check(approx_eq(sidebar->computed_style->width, 260.0f, 0.001f));
    check(sidebar->computed_style->box_sizing == BoxSizing::BorderBox);
    check(approx_eq(main->computed_style->width, 940.0f, 0.001f));
    check(main->computed_style->box_sizing == BoxSizing::BorderBox);
    check(approx_eq(sidebar->x(), 0.0f, 0.001f));
    check(approx_eq(sidebar->layout_width(), 260.0f, 0.001f));
    check(approx_eq(main->x(), 260.0f, 0.001f));
    check(approx_eq(main->layout_width(), 940.0f, 0.001f));
    check(approx_eq(container->x(), 40.0f, 0.001f));
    check(approx_eq(container->layout_width(), 860.0f, 0.001f));
    check(panes[0]->is_visible());
    check_false(panes[1]->is_visible());
    check_false(panes[2]->is_visible());
    check_false(panes[3]->is_visible());
  }
}

spec("LayoutManager preserves explicit cross size under flex stretch") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* explicit_child = box.create("div", "explicit");
    auto* auto_child = box.create("div", "auto");
    root->append(explicit_child);
    root->append(auto_child);
    box.set_root(root);
    box.set_viewport(300.0f, 200.0f);

    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
        display: flex;
        flex-direction: column;
        align-items: stretch;
      }
      #explicit {
        width: 120px;
        height: 20px;
      }
      #auto {
        height: 20px;
      }
    )");
    box.update();

    check(approx_eq(explicit_child->layout_width(), 120.0f, 0.001f));
    check(approx_eq(auto_child->layout_width(), 300.0f, 0.001f));
  }
}

spec("StyleEngine matches Tailwind escaped utility class selectors") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "save");
    auto* stateful = box.create("div", "stateful");
    auto* marker = box.create("span", "marker");
    auto* sibling = box.create("div", "sibling");
    auto* cell = box.create("td", "cell");
    auto* checkbox = box.create("input", "row-check");
    auto* copy = box.create("div", "copy");
    auto* paragraph = box.create("p", "copy-paragraph");
    auto* alert = box.create("div", "alert");
    auto* icon = box.create("svg", "alert-icon");
    auto* body = box.create("div", "alert-body");

    button->add_class("focus-visible:ring-2");
    stateful->add_class("data-[state=open]:bg-accent");
    stateful->set_attribute("data-state", "open");
    marker->add_class("[&+div]:text-xs");
    cell->add_class("[&:has([role=checkbox])]:pr-0");
    checkbox->set_attribute("role", "checkbox");
    copy->add_class("[&_p]:text-xs");
    alert->add_class("[&>svg+div]:translate-y-[-3px]");

    root->append(button);
    root->append(stateful);
    root->append(marker);
    root->append(sibling);
    root->append(cell);
    root->append(copy);
    root->append(alert);
    cell->append(checkbox);
    copy->append(paragraph);
    alert->append(icon);
    alert->append(body);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .focus-visible\:ring-2:focus-visible {
        ring-width: 2px;
      }
      .data-\[state\=open\]\:bg-accent[data-state="open"] {
        background-color: #f1f5f9;
      }
      .\[\&\+div\]\:text-xs + div {
        font-size: 12px;
      }
      td {
        padding-right: 9px;
      }
      .\[\&\:has\(\[role\=checkbox\]\)\]\:pr-0:has([role="checkbox"]) {
        padding-right: 0px;
      }
      .\[\&_p\]\:text-xs p {
        font-size: 12px;
      }
      .\[\&\>svg\+div\]\:translate-y-\[-3px\] > svg + div {
        transform: translateY(-3px);
      }
    )");

    box.update();
    check(approx_eq(button->style_.ring_width, 0.0f, 0.0));
    check(approx_eq(cell->style_.padding[1], 0.0f, 0.0));

    button->set_focus_visible(true);
    box.update();

    check(approx_eq(button->style_.ring_width, 2.0f, 0.0));
    require_color(stateful->style_.background_color, 0xf1 / 255.0f,
                  0xf5 / 255.0f, 0xf9 / 255.0f);
    check(approx_eq(sibling->style_.font_size, 12.0f, 0.0));
    check(approx_eq(paragraph->style_.font_size, 12.0f, 0.0));
    check(approx_eq(body->style_.transform_y, -3.0f, 0.0));
  }
}

spec("StyleEngine matches attribute selectors and disabled pseudo") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "submit");
    button->add_class("btn");
    button->set_attribute("data-state", "open");
    button->set_attribute("aria-disabled", "true");
    button->set_attribute("disabled");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn[data-state=open] {
        background-color: #112233;
      }
      .btn[aria-disabled=true] {
        color: #445566;
      }
      .btn[disabled] {
        width: 90px;
      }
      .btn:disabled {
        height: 24px;
      }
    )");
    box.update();
  
    require_color(button->style_.background_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
    require_color(button->style_.text_color, 0x44 / 255.0f, 0x55 / 255.0f,
                  0x66 / 255.0f);
    check(approx_eq(button->style_.width, 90.0f, 0.0));
    check(approx_eq(button->style_.height, 24.0f, 0.0));
  }
}

spec("StyleEngine matches form state pseudo classes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* required_field = box.create("input", "required-field");
    auto* optional_field = box.create("input", "optional-field");
    required_field->add_class("field");
    optional_field->add_class("field");
    required_field->set_attribute("required");
    required_field->set_attribute("aria-invalid", "true");
    required_field->set_attribute("readonly");
    root->append(required_field);
    root->append(optional_field);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .field:required { width: 11px; }
      .field:optional { width: 22px; }
      .field:invalid { height: 13px; }
      .field:valid { height: 24px; }
      .field:read-only { padding-top: 5px; }
      .field:read-write { padding-right: 7px; }
    )");
    box.update();

    check(approx_eq(required_field->style_.width, 11.0f, 0.001f));
    check(approx_eq(required_field->style_.height, 13.0f, 0.001f));
    check(approx_eq(required_field->style_.padding[0], 5.0f, 0.001f));
    check(approx_eq(optional_field->style_.width, 22.0f, 0.001f));
    check(approx_eq(optional_field->style_.height, 24.0f, 0.001f));
    check(approx_eq(optional_field->style_.padding[1], 7.0f, 0.001f));
  }
}

spec("StyleEngine matches checked open selected and modal pseudos") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* checkbox =
        box.create_widget<CheckboxWidget>("button", "checkbox", "Terms", false);
    auto* checkbox_widget = static_cast<CheckboxWidget*>(checkbox->widget);
    auto* switch_elem =
        box.create_widget<SwitchWidget>("button", "switch", "Airplane", false);
    auto* switch_widget = static_cast<SwitchWidget*>(switch_elem->widget);
    std::vector<ToggleGroupWidget::Option> toggle_options{{"left", "Left"},
                                                          {"center", "Center"}};
    auto* toggle =
        box.create_widget<ToggleGroupWidget>("div", "toggle", toggle_options);
    auto* toggle_widget = static_cast<ToggleGroupWidget*>(toggle->widget);
    auto* dropdown_elem = box.create_widget<DropdownWidget>("button", "dropdown");
    auto* dropdown = static_cast<DropdownWidget*>(dropdown_elem->widget);
    auto* modal_elem = box.create_widget<ModalWidget>("div", "modal", "Settings");
    auto* modal = static_cast<ModalWidget*>(modal_elem->widget);
  
    root->append(checkbox);
    root->append(switch_elem);
    root->append(toggle);
    root->append(dropdown_elem);
    root->append(modal_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    dropdown->add_option("Open", "open");
    dropdown->add_option("Closed", "closed");
    checkbox_widget->set_checked(true);
    switch_widget->set_checked(true);
    toggle_widget->set_selected_index(1);
    dropdown->open();
    modal->open();
  
    box.load_css(R"(
      #checkbox:checked {
        width: 41px;
      }
      #switch:checked {
        height: 22px;
      }
      #toggle:selected {
        font-size: 19px;
      }
      #dropdown:open {
        background-color: #112233;
      }
      #modal:modal {
        color: #445566;
      }
    )");
    box.update();
  
    check(approx_eq(checkbox->style_.width, 41.0f, 0.0));
    check(approx_eq(switch_elem->style_.height, 22.0f, 0.0));
    check(approx_eq(toggle->style_.font_size, 19.0f, 0.0));
    require_color(dropdown_elem->style_.background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
    require_color(modal_elem->style_.text_color, 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f);
  }
}

spec("StyleEngine matches indeterminate pseudo state") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* progress_elem =
        box.create_widget<ProgressBarWidget>("div", "progress", 62.0f, false);
    auto* progress = static_cast<ProgressBarWidget*>(progress_elem->widget);

    root->append(progress_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 80.0f);

    box.load_css(R"(
      #progress:indeterminate {
        color: #020617;
      }
      #progress[data-state='indeterminate'] {
        border-color: #020617;
      }
    )");

    box.update();
    require_color(progress_elem->style_.text_color, 0.0f, 0.0f, 0.0f, 1.0f);

    progress->set_indeterminate(true);
    box.update();

    require_color(progress_elem->style_.text_color, 0x02 / 255.0f,
                  0x06 / 255.0f, 0x17 / 255.0f);
    require_color(progress_elem->style_.border_color, 0x02 / 255.0f,
                  0x06 / 255.0f, 0x17 / 255.0f);
  }
}

spec("StyleEngine matches table and tree host bridge attributes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* table_elem = box.create_widget<TableWidget>("div", "table");
    auto* table = static_cast<TableWidget*>(table_elem->widget);
    auto* tree_elem = box.create_widget<TreeWidget>("div", "tree");
    auto* tree = static_cast<TreeWidget*>(tree_elem->widget);
  
    table->add_column("Name", 120.0f);
    table->add_column("Role", 100.0f);
    table->add_row({"Ada", "Admin"});
    table->add_row({"Linus", "Owner"});
    table->set_selected_row(1);
  
    auto tree_root = tree->add_node("settings", "Settings");
    tree->add_node("display", "Display", tree_root.get());
    tree->expand("settings");
    tree->set_selected("display");
  
    root->append(table_elem);
    root->append(tree_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #table[data-state=selected][aria-rowcount="2"][aria-colcount="2"] {
        width: 210px;
      }
      #tree[data-state=selected][aria-activedescendant=display][data-expanded-count="1"] {
        color: #336699;
      }
    )");
    box.update();
  
    check(approx_eq(table_elem->style_.width, 210.0f, 0.0));
    require_color(tree_elem->style_.text_color, 0x33 / 255.0f,
                  0x66 / 255.0f, 0x99 / 255.0f);
  }
}

spec("StyleEngine supports :is :where and :not selector functions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* open_button = box.create("button", "trigger");
    auto* closed_button = box.create("button", "plain");
    open_button->add_class("btn");
    closed_button->add_class("btn");
    open_button->set_attribute("data-state", "open");
    closed_button->set_attribute("data-state", "closed");
    root->append(open_button);
    root->append(closed_button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        width: 80px;
        color: #111111;
      }
      :where(.btn) {
        width: 40px;
      }
      :is(#trigger, .btn) {
        color: #223344;
      }
      .btn:not([data-state=open]) {
        height: 12px;
      }
      .btn[data-state=open] {
        height: 30px;
      }
    )");
    box.update();
  
    check(approx_eq(open_button->style_.width, 80.0f, 0.0));
    check(approx_eq(closed_button->style_.width, 80.0f, 0.0));
    require_color(open_button->style_.text_color, 0x22 / 255.0f, 0x33 / 255.0f,
                  0x44 / 255.0f);
    require_color(closed_button->style_.text_color, 0x22 / 255.0f, 0x33 / 255.0f,
                  0x44 / 255.0f);
    check(approx_eq(open_button->style_.height, 30.0f, 0.0));
    check(approx_eq(closed_button->style_.height, 12.0f, 0.0));
  }
}

spec("StyleEngine supports :has selector functions including child combinators") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* direct_card = box.create("section", "direct");
    auto* nested_card = box.create("section", "nested");
    auto* empty_card = box.create("section", "empty");
    auto* direct_icon = box.create("span", "direct-icon");
    auto* nested_body = box.create("div", "nested-body");
    auto* nested_icon = box.create("span", "nested-icon");
  
    direct_card->add_class("card");
    nested_card->add_class("card");
    empty_card->add_class("card");
    direct_icon->add_class("icon");
    nested_icon->add_class("icon");
    direct_icon->set_attribute("data-slot", "icon");
    nested_icon->set_attribute("data-slot", "icon");
  
    root->append(direct_card);
    root->append(nested_card);
    root->append(empty_card);
    direct_card->append(direct_icon);
    nested_card->append(nested_body);
    nested_body->append(nested_icon);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .card {
        width: 40px;
        height: 10px;
        color: #111111;
      }
      .card:has(.icon) {
        width: 80px;
      }
      .card:has(> .icon) {
        height: 20px;
      }
      .card:has([data-slot=icon]) {
        color: #224466;
      }
    )");
    box.update();
  
    check(approx_eq(direct_card->style_.width, 80.0f, 0.0));
    check(approx_eq(nested_card->style_.width, 80.0f, 0.0));
    check(approx_eq(empty_card->style_.width, 40.0f, 0.0));
  
    check(approx_eq(direct_card->style_.height, 20.0f, 0.0));
    check(approx_eq(nested_card->style_.height, 10.0f, 0.0));
    check(approx_eq(empty_card->style_.height, 10.0f, 0.0));
  
    require_color(direct_card->style_.text_color, 0x22 / 255.0f,
                  0x44 / 255.0f, 0x66 / 255.0f);
    require_color(nested_card->style_.text_color, 0x22 / 255.0f,
                  0x44 / 255.0f, 0x66 / 255.0f);
    require_color(empty_card->style_.text_color, 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
  }
}

spec("StyleEngine supports placeholder pseudo element and placeholder-shown") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* input_elem = box.create_widget<InputWidget>("input", "search", "Search");
    auto* textarea_elem =
        box.create_widget<TextAreaWidget>("textarea", "notes", "", "Write here");
    input_elem->add_class("field");
    textarea_elem->add_class("field");
    root->append(input_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .field {
        width: 40px;
        height: 20px;
      }
      .field:placeholder-shown {
        width: 90px;
      }
      .field::placeholder {
        color: #778899;
      }
    )");
    box.update();
  
    check(approx_eq(input_elem->style_.width, 90.0f, 0.0));
    check(approx_eq(textarea_elem->style_.width, 90.0f, 0.0));
    check(input_elem->computed_style->get_variable(Symbol("--input-placeholder")) ==
            "119, 136, 153, 255");
    check(input_elem->computed_style->get_variable(Symbol("--textarea-placeholder")) ==
            "119, 136, 153, 255");
  
    static_cast<InputWidget*>(input_elem->widget)->set_text("abc");
    static_cast<TextAreaWidget*>(textarea_elem->widget)->set_text("body");
    box.update();
  
    check(approx_eq(input_elem->style_.width, 40.0f, 0.0));
    check(approx_eq(textarea_elem->style_.width, 40.0f, 0.0));
  }
}

spec("StyleEngine supports selection pseudo element bridge variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* input_elem = box.create_widget<InputWidget>("input", "search", "Search");
    auto* textarea_elem =
        box.create_widget<TextAreaWidget>("textarea", "notes", "", "Write here");
    input_elem->add_class("field");
    textarea_elem->add_class("field");
    root->append(input_elem);
    root->append(textarea_elem);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .field::selection {
        color: #f8fafc;
        background-color: #334455;
      }
    )");
    box.update();

    check(input_elem->computed_style->get_variable(Symbol("--selection-color")) ==
            "248, 250, 252, 255");
    check(input_elem->computed_style->get_variable(Symbol("--selection-bg")) ==
            "51, 68, 85, 255");
    check(input_elem->computed_style->get_variable(Symbol("--input-selection-color")) ==
            "248, 250, 252, 255");
    check(input_elem->computed_style->get_variable(Symbol("--input-selection-bg")) ==
            "51, 68, 85, 255");
    check(textarea_elem->computed_style->get_variable(Symbol("--textarea-selection-color")) ==
            "248, 250, 252, 255");
    check(textarea_elem->computed_style->get_variable(Symbol("--textarea-selection-bg")) ==
            "51, 68, 85, 255");
  }
}

spec("StyleEngine remaps before and after pseudo elements into host variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* badge = box.create("div", "badge");
    badge->add_class("badge");
    root->append(badge);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .badge::before {
        content: "NEW";
        color: #ffffff;
        background-color: #112233;
        border: 2px solid #445566;
        border-radius: 6px;
        opacity: 0.5;
        inset: 2px 8px 4px 4px;
        width: 30px;
        height: 14px;
        left: 4px;
        top: 2px;
        font-size: 10px;
      }
      .badge::after {
        content: "!";
        color: #223344;
        right: 3px;
        bottom: 1px;
      }
    )");
    box.update();
  
    check(badge->computed_style->get_variable(Symbol("--before-content")) == "NEW");
    check(badge->computed_style->get_variable(Symbol("--before-width")) == "30px");
    check(badge->computed_style->get_variable(Symbol("--before-left")) == "4px");
    check(badge->computed_style->get_variable(Symbol("--before-border-width")) == "2px");
    check(badge->computed_style->get_variable(Symbol("--before-border-radius")) == "6px");
    check(badge->computed_style->get_variable(Symbol("--before-opacity")) == "0.5");
    check(badge->computed_style->get_variable(Symbol("--before-inset")) == "2px 8px 4px 4px");
    check(badge->computed_style->get_variable(Symbol("--before-font-size")) == "10px");
    check(badge->computed_style->get_variable(Symbol("--after-content")) == "!");
    check(badge->computed_style->get_variable(Symbol("--after-right")) == "3px");
  }
}

spec("StyleEngine applies viewport media queries and recomputes on resize") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("section", "panel");
    panel->add_class("panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(480.0f, 200.0f);
  
    box.load_css(R"(
      .panel {
        width: 40px;
        height: 10px;
        color: #112233;
      }
  
      @media screen and (max-width: 32rem) {
        .panel {
          height: 18px;
        }
      }
  
      @media (min-width: 40rem), (min-height: 30rem) {
        .panel {
          width: 120px;
          color: #445566;
        }
      }
    )");
    box.update();
  
    check(approx_eq(panel->style_.width, 40.0f, 0.0));
    check(approx_eq(panel->style_.height, 18.0f, 0.0));
    require_color(panel->style_.text_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
  
    box.set_viewport(800.0f, 200.0f);
    box.update();
  
    check(approx_eq(panel->style_.width, 120.0f, 0.0));
    check(approx_eq(panel->style_.height, 10.0f, 0.0));
    require_color(panel->style_.text_color, 0x44 / 255.0f, 0x55 / 255.0f,
                  0x66 / 255.0f);
  
    box.set_viewport(480.0f, 200.0f);
    box.update();
  
    check(approx_eq(panel->style_.width, 40.0f, 0.0));
    check(approx_eq(panel->style_.height, 18.0f, 0.0));
    require_color(panel->style_.text_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
  }
}

spec("StyleEngine applies orientation media queries and recomputes on rotate") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* panel = box.create("section", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(320.0f, 640.0f);

    box.load_css(R"(
      #panel {
        width: 40px;
        height: 20px;
      }

      @media (orientation: portrait) {
        #panel {
          width: 80px;
        }
      }

      @media (orientation: landscape) {
        #panel {
          height: 60px;
        }
      }
    )");
    box.update();

    check(approx_eq(panel->style_.width, 80.0f, 0.0));
    check(approx_eq(panel->style_.height, 20.0f, 0.0));

    box.set_viewport(640.0f, 320.0f);
    box.update();

    check(approx_eq(panel->style_.width, 40.0f, 0.0));
    check(approx_eq(panel->style_.height, 60.0f, 0.0));
  }
}

spec("StyleEngine applies aspect-ratio media queries and recomputes on resize") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* panel = box.create("section", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(300.0f, 300.0f);

    box.load_css(R"(
      #panel {
        width: 40px;
        height: 20px;
      }

      @media (min-aspect-ratio: 4/3) {
        #panel {
          width: 90px;
        }
      }

      @media (max-aspect-ratio: 1/1) {
        #panel {
          height: 70px;
        }
      }
    )");
    box.update();

    check(approx_eq(panel->style_.width, 40.0f, 0.0));
    check(approx_eq(panel->style_.height, 70.0f, 0.0));

    box.set_viewport(800.0f, 400.0f);
    box.update();

    check(approx_eq(panel->style_.width, 90.0f, 0.0));
    check(approx_eq(panel->style_.height, 20.0f, 0.0));
  }
}

spec("StyleEngine applies modern range media queries and recomputes on resize") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("section", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(640.0f, 360.0f);

    box.load_css(R"(
      #panel {
        width: 24px;
        height: 18px;
        opacity: 1.0;
      }

      @media (width >= 40rem) {
        #panel {
          width: 72px;
        }
      }

      @media (height < 25rem) {
        #panel {
          height: 44px;
        }
      }

      @media (1/1 < aspect-ratio < 2/1) {
        #panel {
          opacity: 0.5;
        }
      }
    )");
    box.update();

    check(approx_eq(panel->style_.width, 72.0f, 0.0));
    check(approx_eq(panel->style_.height, 44.0f, 0.0));
    check(approx_eq(panel->style_.opacity, 0.5f, 0.001f));

    box.set_viewport(500.0f, 500.0f);
    box.update();

    check(approx_eq(panel->style_.width, 24.0f, 0.0));
    check(approx_eq(panel->style_.height, 18.0f, 0.0));
    check(approx_eq(panel->style_.opacity, 1.0f, 0.001f));
  }
}

spec("StyleEngine applies environment media queries using stable defaults") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("section", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(640.0f, 360.0f);

    box.load_css(R"(
      #panel {
        width: 40px;
        height: 20px;
        color: #111111;
      }

      @media (prefers-reduced-motion: reduce) {
        #panel {
          width: 80px;
        }
      }

      @media (prefers-reduced-motion: no-preference) {
        #panel {
          height: 60px;
        }
      }

      @media (prefers-color-scheme: dark) {
        #panel {
          color: #222222;
        }
      }

      @media (prefers-color-scheme: light) {
        #panel {
          color: #334455;
        }
      }
    )");
    box.update();

    check(approx_eq(panel->style_.width, 40.0f, 0.0));
    check(approx_eq(panel->style_.height, 60.0f, 0.0));
    require_color(panel->style_.text_color, 0x33 / 255.0f, 0x44 / 255.0f,
                  0x55 / 255.0f);
  }
}

spec("StyleEngine recomputes hover pointer and accessibility media queries") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("section", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(640.0f, 360.0f);

    box.load_css(R"(
      #panel {
        width: 40px;
        height: 20px;
        color: #111111;
        background-color: #010203;
        outline-width: 1px;
      }

      @media (hover: hover) {
        #panel {
          width: 60px;
        }
      }

      @media (pointer: fine) {
        #panel {
          height: 30px;
        }
      }

      @media (any-hover: hover) {
        #panel {
          outline-width: 2px;
        }
      }

      @media (any-pointer: fine) {
        #panel {
          color: #223344;
        }
      }

      @media (prefers-contrast: no-preference) {
        #panel {
          background-color: #334455;
        }
      }

      @media (forced-colors: active) {
        #panel {
          background-color: #778899;
        }
      }

      @media (hover: none) and (pointer: coarse) and (any-hover: none) and
             (any-pointer: coarse) and (prefers-contrast: more) and
             (forced-colors: active) {
        #panel {
          width: 72px;
          height: 48px;
          outline-width: 4px;
          color: #aabbcc;
          background-color: #8899aa;
        }
      }
    )");
    box.update();

    check(approx_eq(panel->style_.width, 60.0f, 0.0));
    check(approx_eq(panel->style_.height, 30.0f, 0.0));
    check(approx_eq(panel->style_.outline_width, 2.0f, 0.0));
    require_color(panel->style_.text_color, 0x22 / 255.0f, 0x33 / 255.0f,
                  0x44 / 255.0f);
    require_color(panel->style_.background_color, 0x33 / 255.0f,
                  0x44 / 255.0f, 0x55 / 255.0f);

    MediaEnvironment env;
    env.hover_available = false;
    env.any_hover_available = false;
    env.pointer_precision = PointerPrecision::Coarse;
    env.any_pointer_precision = PointerPrecision::Coarse;
    env.contrast_preference = ContrastPreference::More;
    env.forced_colors_active = true;
    box.set_media_environment(env);
    box.update();

    check(approx_eq(panel->style_.width, 72.0f, 0.0));
    check(approx_eq(panel->style_.height, 48.0f, 0.0));
    check(approx_eq(panel->style_.outline_width, 4.0f, 0.0));
    require_color(panel->style_.text_color, 0xaa / 255.0f, 0xbb / 255.0f,
                  0xcc / 255.0f);
    require_color(panel->style_.background_color, 0x88 / 255.0f,
                  0x99 / 255.0f, 0xaa / 255.0f);
  }
}

spec("StyleEngine applies inline-size container min-width queries") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* container = box.create("section", "container");
    auto* card = box.create("div", "card");
    root->append(container);
    container->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
      }

      #container {
        container-type: inline-size;
        width: 120px;
      }

      #card {
        width: 24px;
        height: 12px;
        color: #111111;
      }

      @container (min-width: 100px) {
        #card {
          width: 72px;
          color: #223344;
        }
      }
    )");
    box.update();

    check(approx_eq(container->layout_width(), 120.0f, 0.1f));
    check(approx_eq(card->style_.width, 72.0f, 0.0));
    require_color(card->style_.text_color, 0x22 / 255.0f, 0x33 / 255.0f,
                  0x44 / 255.0f);
  }
}

spec("StyleEngine applies inline-size container max-width queries") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* container = box.create("section", "container");
    auto* card = box.create("div", "card");
    root->append(container);
    container->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
      }

      #container {
        container-type: inline-size;
        width: 80px;
      }

      #card {
        width: 60px;
        height: 16px;
        background-color: #111111;
      }

      @container (max-width: 90px) {
        #card {
          width: 36px;
          background-color: #445566;
        }
      }
    )");
    box.update();

    check(approx_eq(container->layout_width(), 80.0f, 0.1f));
    check(approx_eq(card->style_.width, 36.0f, 0.0));
    require_color(card->style_.background_color, 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f);
  }
}

spec("StyleEngine applies named container queries") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* sidebar = box.create("aside", "sidebar");
    auto* content = box.create("main", "content");
    auto* sidebar_card = box.create("div", "sidebar_card");
    auto* content_card = box.create("div", "content_card");
    sidebar_card->add_class("card");
    content_card->add_class("card");
    root->append(sidebar);
    root->append(content);
    sidebar->append(sidebar_card);
    content->append(content_card);
    box.set_root(root);
    box.set_viewport(480.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: row;
        align-items: start;
        gap: 20px;
      }

      #sidebar {
        container-type: inline-size;
        container-name: sidebar;
        width: 140px;
      }

      #content {
        container-type: inline-size;
        container-name: content;
        width: 140px;
      }

      .card {
        width: 24px;
        color: #111111;
      }

      @container sidebar (min-width: 100px) {
        .card {
          width: 88px;
          color: #557799;
        }
      }
    )");
    box.update();

    check(approx_eq(sidebar_card->style_.width, 88.0f, 0.0));
    require_color(sidebar_card->style_.text_color, 0x55 / 255.0f,
                  0x77 / 255.0f, 0x99 / 255.0f);
    check(approx_eq(content_card->style_.width, 24.0f, 0.0));
    require_color(content_card->style_.text_color, 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
  }
}

spec("StyleEngine recomputes container queries after container resize on update") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* container = box.create("section", "container");
    auto* card = box.create("div", "card");
    container->add_class("wide");
    root->append(container);
    container->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
      }

      #container {
        container-type: inline-size;
      }

      #container.wide {
        width: 140px;
      }

      #container.narrow {
        width: 60px;
      }

      #card {
        width: 20px;
        color: #111111;
      }

      @container (min-width: 100px) {
        #card {
          width: 84px;
          color: #aa5500;
        }
      }
    )");
    box.update();

    check(approx_eq(container->layout_width(), 140.0f, 0.1f));
    check(approx_eq(card->style_.width, 84.0f, 0.0));
    require_color(card->style_.text_color, 0xaa / 255.0f, 0x55 / 255.0f,
                  0x00 / 255.0f);

    container->remove_class("wide");
    container->add_class("narrow");

    check(approx_eq(card->style_.width, 84.0f, 0.0));
    require_color(card->style_.text_color, 0xaa / 255.0f, 0x55 / 255.0f,
                  0x00 / 255.0f);

    box.update();

    check(approx_eq(container->layout_width(), 60.0f, 0.1f));
    check(approx_eq(card->style_.width, 20.0f, 0.0));
    require_color(card->style_.text_color, 0x11 / 255.0f, 0x11 / 255.0f,
                  0x11 / 255.0f);
  }
}

spec("StyleEngine supports descendant and child combinators") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("section", "card");
    auto* icon = box.create("span", "icon");
    auto* body = box.create("div", "body");
    auto* cta = box.create("button", "cta");
    card->add_class("card");
    icon->add_class("icon");
    body->add_class("body");
    cta->add_class("cta");
    root->append(card);
    card->append(icon);
    card->append(body);
    body->append(cta);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .card .cta {
        width: 70px;
      }
      .card > .icon {
        height: 12px;
      }
      .card > .body > .cta {
        color: #112233;
      }
    )");
    box.update();
  
    check(approx_eq(cta->style_.width, 70.0f, 0.0));
    check(approx_eq(icon->style_.height, 12.0f, 0.0));
    require_color(cta->style_.text_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
  }
}

spec("StyleEngine supports sibling combinators") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* peer = box.create("button", "peer");
    auto* panel = box.create("div", "panel");
    auto* hint = box.create("div", "hint");
    auto* tail = box.create("div", "tail");
    peer->add_class("peer");
    panel->add_class("panel");
    hint->add_class("hint");
    tail->add_class("tail");
    root->append(peer);
    root->append(panel);
    root->append(hint);
    root->append(tail);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .peer:focus + .panel {
        background-color: #223344;
      }
      .peer:focus ~ .hint {
        width: 77px;
      }
      .peer:focus ~ .tail {
        height: 19px;
      }
    )");
  
    box.update();
    check(approx_eq(panel->style_.background_color.a, 0.0f, 0.001f));
    check(approx_eq(hint->style_.width, 0.0f, 0.0));
    check(approx_eq(tail->style_.height, 0.0f, 0.0));
  
    peer->set_focus(true);
    box.update();
  
    require_color(panel->style_.background_color, 0x22 / 255.0f, 0x33 / 255.0f,
                  0x44 / 255.0f);
    check(approx_eq(hint->style_.width, 77.0f, 0.0));
    check(approx_eq(tail->style_.height, 19.0f, 0.0));
  }
}

spec("StyleEngine supports structural pseudo classes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* sequence = box.create("div", "sequence");
    auto* first_div = box.create("div", "first-div");
    auto* first_span = box.create("span", "first-span");
    auto* second_div = box.create("div", "second-div");
    auto* third_div = box.create("div", "third-div");
    auto* second_span = box.create("span", "second-span");
    auto* last_div = box.create("div", "last-div");
    auto* empties = box.create("div", "empties");
    auto* empty_box = box.create("div", "empty-box");
    auto* filled_box = box.create("div", "filled-box");
    auto* only_parent = box.create("div", "only-parent");
    auto* only = box.create("span", "only");
  
    first_div->add_class("seq-item");
    first_span->add_class("seq-item");
    second_div->add_class("seq-item");
    third_div->add_class("seq-item");
    second_span->add_class("seq-item");
    last_div->add_class("seq-item");
    filled_box->set_text("x");
  
    sequence->append(first_div);
    sequence->append(first_span);
    sequence->append(second_div);
    sequence->append(third_div);
    sequence->append(second_span);
    sequence->append(last_div);
    empties->append(empty_box);
    empties->append(filled_box);
    only_parent->append(only);
  
    root->append(sequence);
    root->append(empties);
    root->append(only_parent);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #sequence > :first-child {
        width: 10px;
      }
      #sequence > :last-child {
        width: 20px;
      }
      #sequence > :nth-child(odd) {
        height: 11px;
      }
      #sequence > :nth-child(even) {
        height: 22px;
      }
      #sequence > :nth-child(4) {
        background-color: #112233;
      }
      #sequence > div:nth-of-type(odd) {
        border-width: 3px;
      }
      #sequence > span:nth-of-type(even) {
        color: #445566;
      }
      #sequence > div:nth-of-type(2) {
        width: 40px;
      }
      #sequence > .seq-item:not(:first-child) {
        opacity: 0.5;
      }
      #empty-box:empty {
        width: 13px;
      }
      #filled-box:empty {
        width: 99px;
      }
      #only-parent > :only-child {
        height: 33px;
      }
    )");
    box.update();
  
    check(approx_eq(first_div->style_.width, 10.0f, 0.0));
    check(approx_eq(last_div->style_.width, 20.0f, 0.0));
    check(approx_eq(second_div->style_.width, 40.0f, 0.0));
  
    check(approx_eq(first_div->style_.height, 11.0f, 0.0));
    check(approx_eq(first_span->style_.height, 22.0f, 0.0));
    check(approx_eq(second_div->style_.height, 11.0f, 0.0));
    check(approx_eq(third_div->style_.height, 22.0f, 0.0));
    check(approx_eq(second_span->style_.height, 11.0f, 0.0));
    check(approx_eq(last_div->style_.height, 22.0f, 0.0));
  
    require_color(third_div->style_.background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
    check(approx_eq(first_div->style_.border_width[0], 3.0f, 0.0));
    check(approx_eq(third_div->style_.border_width[0], 3.0f, 0.0));
    check(approx_eq(second_div->style_.border_width[0], 0.0f, 0.0));
    require_color(second_span->style_.text_color, 0x44 / 255.0f, 0x55 / 255.0f,
                  0x66 / 255.0f);
  
    check(approx_eq(first_div->style_.opacity, 1.0f, 0.0));
    check(approx_eq(first_span->style_.opacity, 0.5f, 0.0));
    check(approx_eq(last_div->style_.opacity, 0.5f, 0.0));
  
    check(approx_eq(empty_box->style_.width, 13.0f, 0.0));
    check(approx_eq(filled_box->style_.width, 0.0f, 0.0));
    check(approx_eq(only->style_.height, 33.0f, 0.0));
  }
}

spec("StyleEngine honors source order for equal specificity rules") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    card->add_class("panel");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .panel { background-color: #111111; width: 10px; }
      .panel { background-color: #222222; width: 30px; }
    )");
    box.update();
  
    require_color(card->style_.background_color, 0x22 / 255.0f, 0x22 / 255.0f,
                  0x22 / 255.0f);
    check(approx_eq(card->style_.width, 30.0f, 0.0));
    check(approx_eq(card->layout_width(), 30.0f, 0.0));
  }
}

spec("StyleEngine resolves variable fallbacks, nesting, and local overrides") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    auto* chip = box.create("span", "chip");
    card->add_class("themed");
    chip->add_class("themed");
    chip->add_class("nested");
    root->append(card);
    root->append(chip);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      div {
        --brand: #112233;
        --size: 12px;
      }
      .themed {
        background-color: var(--brand, #000000);
        width: var(--size, 4px);
        color: var(--missing, #445566);
      }
      .nested {
        border-color: var(--also-missing, var(--brand, #000000));
      }
      #card {
        --brand: #778899;
        --size: 24px;
      }
    )");
    box.update();
  
    require_color(card->style_.background_color, 0x77 / 255.0f, 0x88 / 255.0f,
                  0x99 / 255.0f);
    check(approx_eq(card->style_.width, 24.0f, 0.0));
    require_color(card->style_.text_color, 0x44 / 255.0f, 0x55 / 255.0f,
                  0x66 / 255.0f);
  
    require_color(chip->style_.background_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
    check(approx_eq(chip->style_.width, 12.0f, 0.0));
    require_color(chip->style_.border_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
  }
}

spec("StyleEngine resolves shadcn-style hsl and modern rgb color functions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* surface = box.create("div", "surface");
    auto* accent = box.create("div", "accent");
    auto* chip = box.create("div", "chip");
    auto* destructive = box.create("div", "destructive");
    auto* muted = box.create("div", "muted");
    root->append(surface);
    root->append(accent);
    root->append(chip);
    root->append(destructive);
    root->append(muted);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        --background: 0 0% 100%;
        --primary: 222.2 47.4% 11.2%;
        --rgb-token: 10 20 30;
        --destructive: 0.637 0.237 25.331;
      }

      #surface {
        background-color: hsl(var(--background));
        color: hsl(var(--primary) / 0.5);
      }

      #accent {
        background-color: hsla(120, 100%, 25%, 75%);
      }

      #chip {
        color: rgb(var(--rgb-token) / 50%);
      }

      #destructive {
        background-color: oklch(var(--destructive) / 80%);
      }

      #muted {
        color: oklch(98.5% 0 0);
      }
    )");
    box.update();

    require_color(surface->style_.background_color, 1.0f, 1.0f, 1.0f);
    require_color(surface->style_.text_color, 0.0589f, 0.0904f, 0.1651f, 0.5f);
    require_color(accent->style_.background_color, 0.0f, 0.5f, 0.0f, 0.75f);
    require_color(chip->style_.text_color, 10.0f / 255.0f, 20.0f / 255.0f,
                  30.0f / 255.0f, 0.5f);
    require_color(destructive->style_.background_color, 0.9827f, 0.1718f,
                  0.2131f, 0.8f);
    require_color(muted->style_.text_color, 0.9803f, 0.9803f, 0.9803f);
  }
}

spec("StyleEngine resolves currentColor across shadcn effect properties") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        border: 2px solid currentColor;
        box-shadow: 0 2px 4px 0 currentColor;
        color: oklch(0.637 0.237 25.331);
        outline-color: currentColor;
        ring: 3px currentColor 1px;
        scrollbar-color: currentColor transparent;
      }
    )");
    box.update();

    require_color(card->style_.text_color, 0.9827f, 0.1718f, 0.2131f);
    require_color(card->style_.border_color, 0.9827f, 0.1718f, 0.2131f);
    require_color(card->style_.outline_color, 0.9827f, 0.1718f, 0.2131f);
    require_color(card->style_.ring_color, 0.9827f, 0.1718f, 0.2131f);
    check(card->style_.has_shadow);
    require_color(card->style_.shadow.color, 0.9827f, 0.1718f, 0.2131f);
    require_color(card->style_.get_variable_color(Symbol("--scrollbar-thumb"), Color{}),
                  251.0f / 255.0f, 44.0f / 255.0f, 54.0f / 255.0f);
    require_color(card->style_.get_variable_color(Symbol("--scrollbar-bg"), Color{}),
                  0.0f, 0.0f, 0.0f, 0.0f);
  }
}

spec("StyleEngine resolves modern shadcn color functions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        background-color: color-mix(in srgb, #000000 25%, #ffffff);
        color: light-dark(#112233, #ffffff);
        border-color: color-mix(in oklab, transparent 50%, #336699);
      }
    )");
    box.update();

    require_color(card->style_.background_color, 0.75f, 0.75f, 0.75f);
    require_color(card->style_.text_color,
                  0x11 / 255.0f,
                  0x22 / 255.0f,
                  0x33 / 255.0f);
    require_color(card->style_.border_color,
                  0x33 / 255.0f * 0.5f,
                  0x66 / 255.0f * 0.5f,
                  0x99 / 255.0f * 0.5f,
                  0.5f);
  }
}

spec("ComputedStyle parses raw CSS color custom properties") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        --tooltip-bg: rgba(15, 23, 42, 0.9);
        --brand-token: oklch(0.62 0.18 252);
        color: oklch(0.62 0.18 252);
      }
    )");
    box.update();

    require_color(card->computed_style->get_variable_color(Symbol("--tooltip-bg"),
                                                           Color{}),
                  15.0f / 255.0f, 23.0f / 255.0f, 42.0f / 255.0f, 0.9f);
    require_color(card->computed_style->get_variable_color(Symbol("--brand-token"),
                                                           Color{}),
                  card->computed_style->text_color.r,
                  card->computed_style->text_color.g,
                  card->computed_style->text_color.b,
                  card->computed_style->text_color.a);
  }
}

spec("StyleEngine resolves CSS math functions for shadcn radius tokens") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #root {
        --radius: 0.5rem;
      }

      #card {
        border-radius: calc(var(--radius) * 0.8);
        margin: max(2px, 0.25rem);
        padding: clamp(4px, 0.75rem, 20px);
        width: min(80px, 10rem);
      }
    )");
    box.update();

    check(approx_eq(card->style_.border_radius[0], 6.4f, 0.001f));
    check(approx_eq(card->style_.margin[0], 4.0f, 0.001f));
    check(approx_eq(card->style_.padding[0], 12.0f, 0.001f));
    check(approx_eq(card->style_.width, 80.0f, 0.001f));
  }
}

spec("StyleEngine parses physical and logical corner radius longhands") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        border-radius: 2px;
        border-top-left-radius: 4px;
        border-top-right-radius: 6px;
        border-end-end-radius: 8px;
        border-end-start-radius: 10px;
      }
    )");
    box.update();

    check(approx_eq(card->style_.border_radius[0], 4.0f, 0.001f));
    check(approx_eq(card->style_.border_radius[1], 6.0f, 0.001f));
    check(approx_eq(card->style_.border_radius[2], 8.0f, 0.001f));
    check(approx_eq(card->style_.border_radius[3], 10.0f, 0.001f));
  }
}

spec("StyleEngine applies CSS math functions to layout constraints") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        width: 320px;
        height: 160px;
      }

      #panel {
        width: 300px;
        height: 10px;
        max-width: calc(50% - 20px);
        min-height: max(20px, 2rem);
      }
    )");
    box.update();

    check(approx_eq(panel->layout_width(), 140.0f, 0.001f));
    check(approx_eq(panel->layout_height(), 32.0f, 0.001f));
  }
}

spec("StyleEngine applies CSS math functions to grid tracks and gaps") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* first = box.create("div", "first");
    auto* second = box.create("div", "second");
    root->append(first);
    root->append(second);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);

    box.load_css(R"(
      #root {
        display: grid;
        width: 320px;
        height: 120px;
        column-gap: calc(4px + 6px);
        grid-template-columns: minmax(calc(2rem + 8px), 1fr) clamp(60px, 25%, 100px);
      }

      #first, #second {
        height: 20px;
      }
    )");
    box.update();

    check(approx_eq(first->x(), 0.0f, 0.001f));
    check(approx_eq(first->layout_width(), 230.0f, 0.001f));
    check(approx_eq(second->x(), 240.0f, 0.001f));
    check(approx_eq(second->layout_width(), 80.0f, 0.001f));
  }
}

spec("StyleEngine resolves viewport units across style and layout") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(400.0f, 300.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        width: 100vw;
        height: 100dvh;
        padding: 5vw 10vh;
      }

      #panel {
        width: 80vw;
        height: 10px;
        max-width: 50dvw;
        min-height: 20svh;
        margin-left: 10vw;
      }
    )");
    box.update();

    check(approx_eq(root->style_.width, 400.0f, 0.001f));
    check(approx_eq(root->style_.height, 300.0f, 0.001f));
    check(approx_eq(root->style_.padding[0], 20.0f, 0.001f));
    check(approx_eq(root->style_.padding[1], 30.0f, 0.001f));
    check(approx_eq(panel->style_.margin[3], 40.0f, 0.001f));
    check(approx_eq(panel->layout_width(), 200.0f, 0.001f));
    check(approx_eq(panel->layout_height(), 60.0f, 0.001f));
  }
}

spec("StyleEngine maps logical spacing and inset properties to physical axes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);

    box.load_css(R"(
      #panel {
        position: absolute;
        padding-inline: 2px 6px;
        padding-block: 4px 8px;
        margin-inline-start: 10px;
        margin-inline-end: 14px;
        margin-block: 3px 9px;
        inset-inline: 5px 15px;
        inset-block-start: 7px;
        inset-block-end: 11px;
      }
    )");
    box.update();

    check(approx_eq(panel->style_.padding[0], 4.0f, 0.001f));
    check(approx_eq(panel->style_.padding[1], 6.0f, 0.001f));
    check(approx_eq(panel->style_.padding[2], 8.0f, 0.001f));
    check(approx_eq(panel->style_.padding[3], 2.0f, 0.001f));
    check(approx_eq(panel->style_.margin[0], 3.0f, 0.001f));
    check(approx_eq(panel->style_.margin[1], 14.0f, 0.001f));
    check(approx_eq(panel->style_.margin[2], 9.0f, 0.001f));
    check(approx_eq(panel->style_.margin[3], 10.0f, 0.001f));
    check(approx_eq(panel->style_.left, 5.0f, 0.001f));
    check(approx_eq(panel->style_.right, 15.0f, 0.001f));
    check(approx_eq(panel->style_.top, 7.0f, 0.001f));
    check(approx_eq(panel->style_.bottom, 11.0f, 0.001f));
  }
}

spec("StyleEngine maps logical properties using rtl direction") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);

    box.load_css(R"(
      #panel {
        direction: rtl;
        position: absolute;
        padding-inline: 2px 6px;
        margin-inline-start: 10px;
        margin-inline-end: 14px;
        inset-inline: 5px 15px;
        border-inline-start: 3px solid #112233;
        border-inline-end: 7px solid #445566;
        border-start-start-radius: 9px;
        border-end-end-radius: 13px;
        text-align: start;
      }
    )");
    box.update();

    check(panel->style_.direction == Direction::Rtl);
    check(panel->style_.text_align == TextAlign::Start);
    check(approx_eq(panel->style_.padding[1], 2.0f, 0.001f));
    check(approx_eq(panel->style_.padding[3], 6.0f, 0.001f));
    check(approx_eq(panel->style_.margin[1], 10.0f, 0.001f));
    check(approx_eq(panel->style_.margin[3], 14.0f, 0.001f));
    check(approx_eq(panel->style_.right, 5.0f, 0.001f));
    check(approx_eq(panel->style_.left, 15.0f, 0.001f));
    check(approx_eq(panel->style_.border_width[1], 3.0f, 0.001f));
    check(approx_eq(panel->style_.border_width[3], 7.0f, 0.001f));
    check(panel->style_.border_style[1] == BorderStyle::Solid);
    check(panel->style_.border_style[3] == BorderStyle::Solid);
    check(approx_eq(panel->style_.border_radius[1], 9.0f, 0.001f));
    check(approx_eq(panel->style_.border_radius[3], 13.0f, 0.001f));
  }
}

spec("StyleEngine recomputes pseudo baselines across hover and active") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* button = box.create("button", "submit");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        background-color: #111111;
        width: 40px;
        height: 20px;
      }
      .btn:hover {
        background-color: #222222;
        width: 80px;
      }
      .btn:active {
        background-color: #333333;
        height: 30px;
      }
    )");
    box.update();
  
    require_color(button->style_.background_color, 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    check(approx_eq(button->style_.width, 40.0f, 0.0));
    check(approx_eq(button->style_.height, 20.0f, 0.0));
  
    button->set_hover(true);
    box.update();
    require_color(button->style_.background_color, 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);
    check(approx_eq(button->style_.width, 80.0f, 0.0));
    check(approx_eq(button->style_.height, 20.0f, 0.0));
  
    button->set_active(true);
    box.update();
    require_color(button->style_.background_color, 0x33 / 255.0f,
                  0x33 / 255.0f, 0x33 / 255.0f);
    check(approx_eq(button->style_.width, 80.0f, 0.0));
    check(approx_eq(button->style_.height, 30.0f, 0.0));
  
    button->set_hover(false);
    box.update();
    require_color(button->style_.background_color, 0x33 / 255.0f,
                  0x33 / 255.0f, 0x33 / 255.0f);
    check(approx_eq(button->style_.width, 40.0f, 0.0));
    check(approx_eq(button->style_.height, 30.0f, 0.0));
  
    button->set_active(false);
    box.update();
    require_color(button->style_.background_color, 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    check(approx_eq(button->style_.width, 40.0f, 0.0));
    check(approx_eq(button->style_.height, 20.0f, 0.0));
  }
}

spec("StyleEngine recomputes pseudo baselines across focus") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* input = box.create("input", "search");
    input->add_class("field");
    root->append(input);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .field {
        border-width: 1px;
        width: 120px;
      }
      .field:focus {
        border-width: 3px;
        width: 140px;
      }
    )");
    box.update();
  
    check(approx_eq(input->style_.border_width[0], 1.0f, 0.0));
    check(approx_eq(input->style_.width, 120.0f, 0.0));
  
    input->set_focus(true);
    box.update();
    check(approx_eq(input->style_.border_width[0], 3.0f, 0.0));
    check(approx_eq(input->style_.width, 140.0f, 0.0));
  
    input->set_focus(false);
    box.update();
    check(approx_eq(input->style_.border_width[0], 1.0f, 0.0));
    check(approx_eq(input->style_.width, 120.0f, 0.0));
  }
}

spec("StyleEngine matches focus-within on focused descendants") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* wrapper = box.create("div", "wrapper");
    auto* input = box.create("input", "search");
    wrapper->add_class("field-wrap");
    input->add_class("field");
    root->append(wrapper);
    wrapper->append(input);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .field-wrap {
        border-width: 1px;
        width: 100px;
      }
      .field-wrap:focus-within {
        border-width: 4px;
        width: 140px;
      }
    )");
    box.update();
  
    check(approx_eq(wrapper->style_.border_width[0], 1.0f, 0.0));
    check(approx_eq(wrapper->style_.width, 100.0f, 0.0));
  
    box.set_focus(input);
    box.update();
    check(approx_eq(wrapper->style_.border_width[0], 4.0f, 0.0));
    check(approx_eq(wrapper->style_.width, 140.0f, 0.0));
  
    box.set_focus(nullptr);
    box.update();
    check(approx_eq(wrapper->style_.border_width[0], 1.0f, 0.0));
    check(approx_eq(wrapper->style_.width, 100.0f, 0.0));
  }
}

spec("StyleEngine bridges standard text properties into widget variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* label = box.create("label", "label");
    auto* textarea = box.create("textarea", "notes");
    label->add_class("copy");
    textarea->add_class("editor");
    root->append(label);
    root->append(textarea);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .copy {
        text-align: center;
        text-align-last: right;
        text-decoration: underline dashed #336699 3px;
        text-decoration-line: underline line-through overline;
        text-decoration-style: double;
        text-underline-offset: 4px;
        font-variant-numeric: tabular-nums;
        vertical-align: middle;
        white-space: nowrap;
        text-wrap: nowrap;
        text-wrap-mode: nowrap;
        text-overflow: ellipsis;
        overflow-wrap: break-word;
        word-break: break-all;
        line-clamp: 2;
      }
      .editor {
        line-height: 1.8;
        max-lines: 4;
        -webkit-line-clamp: 3;
      }
    )");
    box.update();
  
    check(label->computed_style->get_variable(Symbol("--text-align")) == "center");
    check(label->computed_style->get_variable(Symbol("--text-align-last")) ==
            "right");
    const std::string text_decoration =
        label->computed_style->get_variable(Symbol("--text-decoration"));
    check(text_decoration.find("underline") != std::string::npos);
    check(text_decoration.find("line-through") != std::string::npos);
    check(text_decoration.find("overline") != std::string::npos);
    check(label->computed_style->get_variable(Symbol("--text-decoration-color")) ==
            "51, 102, 153, 255");
    check(label->computed_style->get_variable(Symbol("--text-decoration-style")) ==
            "double");
    check(label->computed_style->get_variable(
              Symbol("--text-decoration-thickness")) == "3px");
    check(label->computed_style->get_variable(
              Symbol("--text-underline-offset")) == "4px");
    check(label->computed_style->get_variable(
              Symbol("--font-variant-numeric")) == "tabular-nums");
    check(label->computed_style->get_variable(Symbol("--vertical-align")) ==
            "middle");
    check(label->computed_style->get_variable(Symbol("--white-space")) ==
            "nowrap");
    check(label->computed_style->get_variable(Symbol("--text-wrap")) ==
            "nowrap");
    check(label->computed_style->get_variable(Symbol("--text-wrap-mode")) ==
            "nowrap");
    check(label->computed_style->get_variable(Symbol("--text-overflow")) ==
            "ellipsis");
    check(label->computed_style->get_variable(Symbol("--overflow-wrap")) ==
            "break-word");
    check(label->computed_style->get_variable(Symbol("--word-break")) ==
            "break-all");
    check(label->computed_style->get_variable(Symbol("--line-clamp")) == "2");
  
    check(textarea->computed_style->get_variable(Symbol("--line-height")) ==
            "1.8");
    check(textarea->computed_style->get_variable(Symbol("--max-lines")) ==
            "4");
    check(textarea->computed_style->get_variable(Symbol("--line-clamp")) ==
            "3");
  }
}

spec("StyleEngine stores text transform and letter spacing in computed text state") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* badge = box.create("span", "badge");
    root->append(badge);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #badge {
        text-transform: uppercase;
        letter-spacing: 0.25rem;
        word-spacing: 0.5rem;
        text-indent: 2rem;
        tab-size: 4;
      }
    )");
    box.update();

    check(badge->computed_style->text_transform == TextTransform::Uppercase);
    check(approx_eq(badge->computed_style->letter_spacing, 4.0f, 0.001f));
    check(approx_eq(badge->computed_style->word_spacing, 8.0f, 0.001f));
    check(approx_eq(badge->computed_style->text_indent, 32.0f, 0.001f));
    check(approx_eq(badge->computed_style->tab_size, 4.0f, 0.001f));
    check(badge->computed_style->get_variable(Symbol("--text-transform")) ==
          "uppercase");
    check(badge->computed_style->get_variable(Symbol("--letter-spacing")) ==
          "0.25rem");
    check(badge->computed_style->get_variable(Symbol("--word-spacing")) ==
          "0.5rem");
    check(badge->computed_style->get_variable(Symbol("--text-indent")) ==
          "2rem");
    check(badge->computed_style->get_variable(Symbol("--tab-size")) == "4");
  }
}

spec("StyleEngine parses font-size keyword values") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* medium = box.create("span", "medium");
    auto* large = box.create("span", "large");
    auto* larger = box.create("span", "larger");
    auto* smaller = box.create("span", "smaller");
    auto* bolder = box.create("span", "bolder");
    auto* lighter = box.create("span", "lighter");
    auto* oblique = box.create("span", "oblique");
    auto* family = box.create("span", "family");
    root->append(medium);
    root->append(large);
    root->append(larger);
    root->append(smaller);
    root->append(bolder);
    root->append(lighter);
    root->append(oblique);
    root->append(family);
    box.set_root(root);

    box.load_css(R"(
      #root { font-size: 20px; font-weight: 600; }
      #medium { font-size: medium; }
      #large { font-size: x-large; }
      #larger { font-size: larger; }
      #smaller { font-size: smaller; }
      #bolder { font-weight: bolder; }
      #lighter { font-weight: lighter; }
      #oblique { font-style: oblique 10deg; }
      #family { font-family: "Inter, Variable", system-ui, sans-serif; }
    )");
    box.update();

    check(approx_eq(medium->computed_style->font_size, 16.0f, 0.0f));
    check(approx_eq(large->computed_style->font_size, 24.0f, 0.0f));
    check(approx_eq(larger->computed_style->font_size, 24.0f, 0.001f));
    check(approx_eq(smaller->computed_style->font_size, 20.0f / 1.2f, 0.001f));
    check(bolder->computed_style->font_weight == FontWeight::Black);
    check(lighter->computed_style->font_weight == FontWeight::Normal);
    check(oblique->computed_style->font_style == FontStyle::Oblique);
    check(family->computed_style->font_family == "Inter, Variable");
  }
}

spec("StyleEngine bridges object-fit and object-position into widget variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* image = box.create("img", "hero");
    root->append(image);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #hero {
        object-fit: cover;
        object-position: right bottom;
      }
    )");
    box.update();
  
    check(image->computed_style->get_variable(Symbol("--object-fit")) == "cover");
    check(image->computed_style->get_variable(Symbol("--object-position")) ==
            "right bottom");
  }
}

spec("StyleEngine bridges caret-color and accent-color into widget variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* input = box.create("input", "input");
    auto* checkbox = box.create("button", "checkbox");
    root->append(input);
    root->append(checkbox);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #input {
        caret-color: #123456;
      }
      #checkbox {
        accent-color: #ff3300;
      }
    )");
    box.update();
  
    check(input->computed_style->get_variable(Symbol("--input-cursor")) ==
            "18, 52, 86, 255");
    check(input->computed_style->get_variable(Symbol("--textarea-cursor")) ==
            "18, 52, 86, 255");
    check(input->computed_style->get_variable(Symbol("--caret-color")) ==
            "18, 52, 86, 255");
    check(checkbox->computed_style->get_variable(
                Symbol("--checkbox-bg-checked")) == "255, 51, 0, 255");
    check(checkbox->computed_style->get_variable(
                Symbol("--radio-bg-checked")) == "255, 51, 0, 255");
    check(checkbox->computed_style->get_variable(
                Symbol("--switch-bg-on")) == "255, 51, 0, 255");
    check(checkbox->computed_style->get_variable(
                Symbol("--progress-fill")) == "255, 51, 0, 255");
    check(checkbox->computed_style->get_variable(
                Symbol("--track-fill")) == "255, 51, 0, 255");
    check(checkbox->computed_style->get_variable(Symbol("--accent-color")) ==
                "255, 51, 0, 255");
  }
}

spec("StyleEngine parses filter blur into internal effect variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #card {
        filter: blur(6px);
      }
    )");
    box.update();
  
    check(card->computed_style->get_variable(Symbol("--filter-blur")) == "6px");
  }
}

spec("StyleEngine parses filter drop-shadow and blur into internal effect variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        filter: blur(6px) drop-shadow(2px 4px 3px rgba(10, 20, 30, 0.5));
      }
    )");
    box.update();

    check(card->computed_style->get_variable(Symbol("--filter-blur")) == "6px");
    check(card->computed_style->get_variable(
              Symbol("__flex_filter_drop_shadow_offset_x")) == "2px");
    check(card->computed_style->get_variable(
              Symbol("__flex_filter_drop_shadow_offset_y")) == "4px");
    check(card->computed_style->get_variable(
              Symbol("__flex_filter_drop_shadow_blur")) == "3px");
    require_color(
        card->computed_style->get_variable_color(
            Symbol("__flex_filter_drop_shadow_color"), Color{}),
        10.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 0.5f);
  }
}

spec("StyleEngine parses filter opacity and none resets internal effect variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* percent = box.create("div", "percent");
    auto* number = box.create("div", "number");
    auto* reset = box.create("div", "reset");
    percent->add_class("filtered");
    number->add_class("filtered");
    reset->add_class("filtered");
    reset->add_class("plain");
    root->append(percent);
    root->append(number);
    root->append(reset);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .filtered {
        filter: blur(6px) opacity(50%) drop-shadow(2px 4px 3px rgba(10, 20, 30, 0.5));
      }
      #number {
        filter: opacity(0.25);
      }
      .plain {
        filter: none;
      }
    )");
    box.update();

    check(percent->computed_style->get_variable(Symbol("--filter-blur")) == "6px");
    check(percent->computed_style->get_variable(
              Symbol("__flex_filter_opacity")) == "0.5");
    check(percent->computed_style->get_variable(
              Symbol("__flex_filter_drop_shadow_offset_x")) == "2px");

    check(number->computed_style->get_variable(
              Symbol("__flex_filter_opacity")) == "0.25");
    check(number->computed_style->variables.find(Symbol("--filter-blur")) ==
          number->computed_style->variables.end());
    check(number->computed_style->variables.find(
              Symbol("__flex_filter_drop_shadow_offset_x")) ==
          number->computed_style->variables.end());

    check(reset->computed_style->variables.find(
              Symbol("__flex_filter_opacity")) ==
          reset->computed_style->variables.end());
    check(reset->computed_style->variables.find(Symbol("--filter-blur")) ==
          reset->computed_style->variables.end());
    check(reset->computed_style->variables.find(
              Symbol("__flex_filter_drop_shadow_offset_x")) ==
          reset->computed_style->variables.end());
  }
}

spec("StyleEngine parses backdrop-filter blur into internal effect variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #card {
        backdrop-filter: blur(8px);
      }
    )");
    box.update();
  
    check(card->computed_style->get_variable(Symbol("--backdrop-blur")) == "8px");
  }
}

spec("StyleEngine resets backdrop-filter none") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    card->add_class("glass");
    card->add_class("plain");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .glass {
        backdrop-filter: blur(8px);
      }
      .plain {
        backdrop-filter: none;
      }
    )");
    box.update();

    check(card->computed_style->variables.find(Symbol("--backdrop-blur")) ==
          card->computed_style->variables.end());
  }
}

spec("StyleEngine accepts webkit-prefixed backdrop-filter and user-select aliases") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        -webkit-backdrop-filter: blur(10px);
        -webkit-user-select: none;
      }
    )");
    box.update();

    check(card->computed_style->get_variable(Symbol("--backdrop-blur")) == "10px");
    check(card->computed_style->get_variable(Symbol("--user-select")) == "none");
  }
}

spec("StyleEngine applies @supports declaration queries with boolean operators") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        color: #111111;
      }

      @supports ((backdrop-filter: blur(4px)) or (-webkit-backdrop-filter: blur(4px))) and not (subgrid: auto) {
        #card {
          color: #223344;
          backdrop-filter: blur(4px);
        }
      }
    )");
    box.update();

    require_color(card->computed_style->text_color, 0x22 / 255.0f,
                  0x33 / 255.0f, 0x44 / 255.0f);
    check(card->computed_style->get_variable(Symbol("--backdrop-blur")) == "4px");
  }
}

spec("StyleEngine applies @supports selector queries") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        background-color: #111111;
      }

      @supports selector(:has(*)) {
        #card {
          background-color: #445566;
        }
      }
    )");
    box.update();

    require_color(card->computed_style->background_color, 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f);
  }
}

spec("StyleEngine stores cursor and user-select interaction variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #card {
        cursor: pointer;
        user-select: none;
      }
    )");
    box.update();
  
    check(card->computed_style->get_variable(Symbol("--cursor")) == "pointer");
    check(card->computed_style->get_variable(Symbol("--user-select")) == "none");
  }
}

spec("StyleEngine stores touch-action interaction variable") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* slider = box.create("div", "slider");
    root->append(slider);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #slider {
        touch-action: none;
      }
    )");
    box.update();

    check(slider->computed_style->get_variable(Symbol("--touch-action")) == "none");
  }
}

spec("StyleEngine stores appearance interaction variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    auto* input = box.create("input", "input");
    root->append(card);
    root->append(input);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        appearance: none;
      }

      #input {
        -webkit-appearance: textfield;
      }
    )");
    box.update();

    check(card->computed_style->get_variable(Symbol("--appearance")) == "none");
    check(input->computed_style->get_variable(Symbol("--appearance")) ==
          "textfield");
  }
}

spec("StyleEngine stores color-scheme interaction variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    auto* input = box.create("input", "input");
    root->append(card);
    root->append(input);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        color-scheme: dark;
      }

      #input {
        -webkit-color-scheme: light dark;
      }
    )");
    box.update();

    check(card->computed_style->get_variable(Symbol("--color-scheme")) == "dark");
    check(input->computed_style->get_variable(Symbol("--color-scheme")) ==
          "light dark");
  }
}

spec("StyleEngine bridges standard scrollbar properties into widget variables") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* scroll = box.create("div", "scroll");
    root->append(scroll);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #scroll {
        scrollbar-width: thin;
        scrollbar-color: #336699 #111111;
      }
    )");
    box.update();

    check(scroll->computed_style->get_variable(Symbol("--scrollbar-width")) ==
          "6px");
    check(scroll->computed_style->get_variable(
              Symbol("--listview-scrollbar-width")) == "6px");
    check(scroll->computed_style->get_variable(Symbol("--scrollbar-thumb")) ==
          "51, 102, 153, 255");
    check(scroll->computed_style->get_variable(Symbol("--scrollbar-bg")) ==
          "17, 17, 17, 255");
  }
}

spec("StyleEngine maps scrollbar-width none to hidden widget scrollbars") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* scroll = box.create("div", "scroll");
    root->append(scroll);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #scroll {
        scrollbar-width: none;
      }
    )");
    box.update();

    check(scroll->computed_style->get_variable(Symbol("--scrollbar-width")) ==
          "0px");
    check(scroll->computed_style->get_variable(
              Symbol("--listview-scrollbar-width")) == "0px");
  }
}

spec("StyleEngine recomputes variable-backed styles across pseudo states") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        --tone: #112233;
        background-color: var(--tone, #000000);
      }
      .btn:hover {
        --tone: #445566;
      }
      .btn:focus {
        --tone: #778899;
      }
    )");
    box.update();
  
    require_color(button->style_.background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
  
    button->set_hover(true);
    box.update();
    require_color(button->style_.background_color, 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f);
  
    button->set_focus(true);
    box.update();
    require_color(button->style_.background_color, 0x77 / 255.0f,
                  0x88 / 255.0f, 0x99 / 255.0f);
  
    button->set_hover(false);
    box.update();
    require_color(button->style_.background_color, 0x77 / 255.0f,
                  0x88 / 255.0f, 0x99 / 255.0f);
  
    button->set_focus(false);
    box.update();
    require_color(button->style_.background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
  }
}

spec("StyleEngine parses transform utilities and focus-visible outlines") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    auto* advanced = box.create("div", "advanced");
    auto* individual = box.create("div", "individual");
    auto* affine = box.create("div", "affine");
    auto* skewed = box.create("div", "skewed");
    auto* field = box.create("input", "field");
    panel->add_class("card");
    advanced->add_class("advanced");
    individual->add_class("individual");
    affine->add_class("affine");
    skewed->add_class("skewed");
    field->add_class("field");
    root->append(panel);
    root->append(advanced);
    root->append(individual);
    root->append(affine);
    root->append(skewed);
    root->append(field);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .card {
        transform: translateX(12px) translateY(34px) scaleX(1.5) scaleY(0.75) rotate(45deg);
        opacity: 0.25;
        overflow: hidden;
      }
      .advanced {
        transform: translate(12px) translate3d(4px, 5px, 0)
                   scale3d(2, 3, 1) rotateZ(0.25turn);
      }
      .individual {
        translate: 8px 13px;
        scale: 1.25 0.5;
        rotate: 0.5turn;
      }
      .affine {
        transform: matrix(1, 0.5, 0.25, 1, 7px, 9px);
      }
      .skewed {
        transform: skewX(45deg);
      }
      .field {
        outline: none;
        ring: none;
      }
      .field:focus-visible {
        outline: 2px solid #3366ff;
        outline-offset: 3px;
        ring: 4px rgba(16,32,48,0.5) 1px;
        ring-offset-color: #f8fafc;
      }
    )");
    box.update();
  
    check(approx_eq(panel->style_.transform_x, 12.0f, 0.0));
    check(approx_eq(panel->style_.transform_y, 34.0f, 0.0));
    check(approx_eq(panel->style_.transform_scale_x, 1.5f, 0.0));
    check(approx_eq(panel->style_.transform_scale_y, 0.75f, 0.0));
    check(approx_eq(panel->style_.transform_rotate, 45.0f, 0.0));
    check(approx_eq(panel->style_.transform_origin_x, 0.5f, 0.0));
    check(approx_eq(panel->style_.transform_origin_y, 0.5f, 0.0));
    check(panel->style_.transform_origin_x_percent);
    check(panel->style_.transform_origin_y_percent);
    check(approx_eq(panel->style_.opacity, 0.25f, 0.0));
    check(panel->style_.overflow_x == Overflow::Hidden);
    check(panel->style_.overflow_y == Overflow::Hidden);

    check(approx_eq(advanced->style_.transform_x, 4.0f, 0.0));
    check(approx_eq(advanced->style_.transform_y, 5.0f, 0.0));
    check(approx_eq(advanced->style_.transform_scale_x, 2.0f, 0.0));
    check(approx_eq(advanced->style_.transform_scale_y, 3.0f, 0.0));
    check(approx_eq(advanced->style_.transform_rotate, 90.0f, 0.001f));

    check(approx_eq(individual->style_.transform_x, 8.0f, 0.0));
    check(approx_eq(individual->style_.transform_y, 13.0f, 0.0));
    check(approx_eq(individual->style_.transform_scale_x, 1.25f, 0.0));
    check(approx_eq(individual->style_.transform_scale_y, 0.5f, 0.0));
    check(approx_eq(individual->style_.transform_rotate, 180.0f, 0.001f));

    check(affine->style_.has_transform_matrix);
    check(approx_eq(affine->style_.transform_matrix.data[0], 1.0f, 0.0f));
    check(approx_eq(affine->style_.transform_matrix.data[1], 0.25f, 0.0f));
    check(approx_eq(affine->style_.transform_matrix.data[2], 7.0f, 0.0f));
    check(approx_eq(affine->style_.transform_matrix.data[3], 0.5f, 0.0f));
    check(approx_eq(affine->style_.transform_matrix.data[4], 1.0f, 0.0f));
    check(approx_eq(affine->style_.transform_matrix.data[5], 9.0f, 0.0f));

    check(skewed->style_.has_transform_matrix);
    check(approx_eq(skewed->style_.transform_matrix.data[1], 1.0f, 0.001f));
    check(approx_eq(skewed->style_.transform_matrix.data[3], 0.0f, 0.001f));

    check(approx_eq(field->style_.outline_width, 0.0f, 0.0));
    check(approx_eq(field->style_.ring_width, 0.0f, 0.0));
  
    field->set_focus_visible(true);
    box.update();
    check(approx_eq(field->style_.outline_width, 2.0f, 0.0));
    check(approx_eq(field->style_.outline_offset, 3.0f, 0.0));
    require_color(field->style_.outline_color, 0x33 / 255.0f,
                  0x66 / 255.0f, 1.0f);
    check(approx_eq(field->style_.ring_width, 4.0f, 0.0));
    check(approx_eq(field->style_.ring_offset, 1.0f, 0.0));
    require_color(field->style_.ring_color, 16.0f / 255.0f, 32.0f / 255.0f,
                  48.0f / 255.0f, 0.5f);
    require_color(field->style_.ring_offset_color, 0xf8 / 255.0f,
                  0xfa / 255.0f, 0xfc / 255.0f);
  
    field->set_focus_visible(false);
    box.update();
    check(approx_eq(field->style_.outline_width, 0.0f, 0.0));
    check(approx_eq(field->style_.ring_width, 0.0f, 0.0));
  }
}

spec("StyleEngine parses transform-origin from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    auto* badge = box.create("div", "badge");
    auto* tooltip = box.create("div", "tooltip");
    root->append(panel);
    root->append(badge);
    root->append(tooltip);
    box.set_root(root);

    box.load_css(R"(
      #panel {
        transform-origin: left top;
      }
      #badge {
        transform-origin: 25% 30px;
      }
      #tooltip {
        --radix-tooltip-content-transform-origin: right bottom;
        transform-origin: var(--radix-tooltip-content-transform-origin);
      }
    )");
    box.update();
  
    check(approx_eq(panel->style_.transform_origin_x, 0.0f, 0.0));
    check(approx_eq(panel->style_.transform_origin_y, 0.0f, 0.0));
    check(panel->style_.transform_origin_x_percent);
    check(panel->style_.transform_origin_y_percent);
  
    check(approx_eq(badge->style_.transform_origin_x, 0.25f, 0.0));
    check(approx_eq(badge->style_.transform_origin_y, 30.0f, 0.0));
    check(badge->style_.transform_origin_x_percent);
    check_false(badge->style_.transform_origin_y_percent);

    check(approx_eq(tooltip->style_.transform_origin_x, 1.0f, 0.0));
    check(approx_eq(tooltip->style_.transform_origin_y, 1.0f, 0.0));
    check(tooltip->style_.transform_origin_x_percent);
    check(tooltip->style_.transform_origin_y_percent);
  }
}

spec("StyleEngine parses basic box-shadow subsets and none resets") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* shadowed = box.create("div", "shadowed");
    auto* cleared = box.create("div", "cleared");
    auto* inset = box.create("div", "inset");
    shadowed->add_class("shadowed");
    cleared->add_class("shadowed");
    cleared->add_class("plain");
    inset->add_class("inset");
    root->append(shadowed);
    root->append(cleared);
    root->append(inset);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .shadowed {
        box-shadow: 2px 4px 6px 8px rgba(10,20,30,0.25);
      }
      .plain {
        box-shadow: none;
      }
      .inset {
        box-shadow: inset 1px 2px 3px 0 rgba(40,50,60,0.5);
      }
    )");
    box.update();
  
    check(shadowed->style_.has_shadow);
    check(approx_eq(shadowed->style_.shadow.offset_x, 2.0f, 0.0));
    check(approx_eq(shadowed->style_.shadow.offset_y, 4.0f, 0.0));
    check(approx_eq(shadowed->style_.shadow.blur_radius, 6.0f, 0.0));
    check(approx_eq(shadowed->style_.shadow.spread_radius, 8.0f, 0.0));
    require_color(shadowed->style_.shadow.color, 10.0f / 255.0f, 20.0f / 255.0f,
                  30.0f / 255.0f, 0.25f);
    check_false(shadowed->style_.shadow.inset);
  
    check_false(cleared->style_.has_shadow);
  
    check(inset->style_.has_shadow);
    check(inset->style_.shadow.inset);
    check(approx_eq(inset->style_.shadow.offset_x, 1.0f, 0.0));
    check(approx_eq(inset->style_.shadow.offset_y, 2.0f, 0.0));
    check(approx_eq(inset->style_.shadow.blur_radius, 3.0f, 0.0));
    check(approx_eq(inset->style_.shadow.spread_radius, 0.0f, 0.0));
    require_color(inset->style_.shadow.color, 40.0f / 255.0f, 50.0f / 255.0f,
                  60.0f / 255.0f, 0.5f);
  }
}

spec("StyleEngine applies outline-style none as an explicit outline reset") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* field = box.create("input", "field");
    root->append(field);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #field {
        outline: 2px solid #3366ff;
        outline-offset: 3px;
        outline-style: none;
      }
    )");
    box.update();

    check(approx_eq(field->style_.outline_width, 0.0f, 0.0));
    check(approx_eq(field->style_.outline_offset, 0.0f, 0.0));
    require_color(field->style_.outline_color, 0.0f, 0.0f, 0.0f, 0.0f);
  }
}

spec("StyleEngine parses outline style tokens into computed outline state") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* field = box.create("div", "field");
    auto* shorthand = box.create("div", "shorthand");
    auto* relief = box.create("div", "relief");
    auto* keyword = box.create("div", "keyword");
    auto* keyword_shorthand = box.create("div", "keyword-shorthand");
    root->append(field);
    root->append(shorthand);
    root->append(relief);
    root->append(keyword);
    root->append(keyword_shorthand);
    box.set_root(root);
    box.set_viewport(200.0f, 120.0f);

    box.load_css(R"(
      #field {
        outline: 2px dashed #3366ff;
      }
      #field:hover {
        outline-style: double;
      }
      #shorthand {
        outline: 3px double #112233;
      }
      #relief {
        outline: 4px outset #445566;
      }
      #keyword {
        outline: thick solid #778899;
        outline-width: thin;
      }
      #keyword-shorthand {
        outline: thick solid #99aabb;
      }
    )");
    box.update();

    check(field->style_.outline_style == BorderStyle::Dashed);
    check(approx_eq(field->style_.outline_width, 2.0f, 0.0f));
    require_color(field->style_.outline_color, 0x33 / 255.0f, 0x66 / 255.0f,
                  1.0f, 1.0f);

    field->set_hover(true);
    box.update();

    check(field->style_.outline_style == BorderStyle::Double);
    check(approx_eq(field->style_.outline_width, 2.0f, 0.0f));

    check(shorthand->style_.outline_style == BorderStyle::Double);
    check(approx_eq(shorthand->style_.outline_width, 3.0f, 0.0f));
    require_color(shorthand->style_.outline_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f, 1.0f);

    check(relief->style_.outline_style == BorderStyle::Outset);
    check(approx_eq(relief->style_.outline_width, 4.0f, 0.0f));
    require_color(relief->style_.outline_color, 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f, 1.0f);

    check(keyword->style_.outline_style == BorderStyle::Solid);
    check(approx_eq(keyword->style_.outline_width, 1.0f, 0.0f));
    require_color(keyword->style_.outline_color, 0x77 / 255.0f,
                  0x88 / 255.0f, 0x99 / 255.0f, 1.0f);

    check(keyword_shorthand->style_.outline_style == BorderStyle::Solid);
    check(approx_eq(keyword_shorthand->style_.outline_width, 5.0f, 0.0f));
    require_color(keyword_shorthand->style_.outline_color, 0x99 / 255.0f,
                  0xaa / 255.0f, 0xbb / 255.0f, 1.0f);
  }
}

spec("StyleEngine parses layered box-shadow lists and preserves first layer compatibility") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* layered = box.create("div", "layered");
    root->append(layered);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #layered {
        box-shadow: #112233 1px 2px 3px 4px, inset rgba(40,50,60,0.5) 0 1px 2px 0, 5px 6px 0 0 #778899;
      }
    )");
    box.update();
  
    check(layered->style_.has_shadow);
    check(layered->style_.shadows.size() == 3);
    if (layered->style_.shadows.size() != 3) {
      return;
    }
  
    check(approx_eq(layered->style_.shadow.offset_x, 1.0f, 0.0));
    check(approx_eq(layered->style_.shadow.offset_y, 2.0f, 0.0));
    check(approx_eq(layered->style_.shadow.blur_radius, 3.0f, 0.0));
    check(approx_eq(layered->style_.shadow.spread_radius, 4.0f, 0.0));
    check_false(layered->style_.shadow.inset);
    require_color(layered->style_.shadow.color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f, 1.0f);
  
    check(layered->style_.shadows[1].inset);
    check(approx_eq(layered->style_.shadows[1].offset_x, 0.0f, 0.0));
    check(approx_eq(layered->style_.shadows[1].offset_y, 1.0f, 0.0));
    check(approx_eq(layered->style_.shadows[1].blur_radius, 2.0f, 0.0));
    require_color(layered->style_.shadows[1].color, 40.0f / 255.0f,
                  50.0f / 255.0f, 60.0f / 255.0f, 0.5f);
  
    check_false(layered->style_.shadows[2].inset);
    check(approx_eq(layered->style_.shadows[2].offset_x, 5.0f, 0.0));
    check(approx_eq(layered->style_.shadows[2].offset_y, 6.0f, 0.0));
    check(approx_eq(layered->style_.shadows[2].blur_radius, 0.0f, 0.0));
    check(approx_eq(layered->style_.shadows[2].spread_radius, 0.0f, 0.0));
    require_color(layered->style_.shadows[2].color, 0x77 / 255.0f, 0x88 / 255.0f,
                  0x99 / 255.0f, 1.0f);
  }
}

spec("StyleEngine parses text-shadow layers and none resets") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* shadowed = box.create("label", "shadowed");
    auto* color_first = box.create("label", "color-first");
    auto* cleared = box.create("label", "cleared");
    shadowed->add_class("shadowed");
    color_first->add_class("color-first");
    cleared->add_class("shadowed");
    cleared->add_class("plain");
    root->append(shadowed);
    root->append(color_first);
    root->append(cleared);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .shadowed {
        text-shadow: 1px 2px 3px rgba(10,20,30,0.5), #445566 4px 5px;
      }
      .color-first {
        text-shadow: #112233 1px 2px;
      }
      .plain {
        text-shadow: none;
      }
    )");
    box.update();

    check(shadowed->style_.has_text_shadow);
    check(shadowed->style_.text_shadows.size() == 2);
    if (shadowed->style_.text_shadows.size() != 2) {
      return;
    }

    check(approx_eq(shadowed->style_.text_shadow.offset_x, 1.0f, 0.0f));
    check(approx_eq(shadowed->style_.text_shadow.offset_y, 2.0f, 0.0f));
    check(approx_eq(shadowed->style_.text_shadow.blur_radius, 3.0f, 0.0f));
    require_color(shadowed->style_.text_shadow.color, 10.0f / 255.0f,
                  20.0f / 255.0f, 30.0f / 255.0f, 0.5f);

    check(approx_eq(shadowed->style_.text_shadows[1].offset_x, 4.0f, 0.0f));
    check(approx_eq(shadowed->style_.text_shadows[1].offset_y, 5.0f, 0.0f));
    check(approx_eq(shadowed->style_.text_shadows[1].blur_radius, 0.0f, 0.0f));
    require_color(shadowed->style_.text_shadows[1].color, 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f, 1.0f);

    check(color_first->style_.has_text_shadow);
    check(approx_eq(color_first->style_.text_shadow.offset_x, 1.0f, 0.0f));
    check(approx_eq(color_first->style_.text_shadow.offset_y, 2.0f, 0.0f));
    check(approx_eq(color_first->style_.text_shadow.blur_radius, 0.0f, 0.0f));
    require_color(color_first->style_.text_shadow.color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f, 1.0f);

    check_false(cleared->style_.has_text_shadow);
    check(cleared->style_.text_shadows.empty());
  }
}

spec("StyleEngine parses border shorthand and side border shorthands") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* frame = box.create("div", "frame");
    auto* sides = box.create("div", "sides");
    auto* relief = box.create("div", "relief");
    auto* width_keywords = box.create("div", "width-keywords");
    root->append(frame);
    root->append(sides);
    root->append(relief);
    root->append(width_keywords);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #frame {
        border: 3px solid #112233;
        border-style: solid dashed none dotted;
      }
      #sides {
        border-top: 4px solid #334455;
        border-right: none;
        border-bottom: 6px dashed #334455;
        border-left: 1px dotted #334455;
      }
      #relief {
        border-style: inset outset groove ridge;
      }
      #width-keywords {
        border-width: thin medium thick 7px;
        border-style: solid;
      }
    )");
    box.update();
  
    check(approx_eq(frame->style_.border_width[0], 3.0f, 0.0));
    check(approx_eq(frame->style_.border_width[1], 3.0f, 0.0));
    check(approx_eq(frame->style_.border_width[2], 0.0f, 0.0));
    check(approx_eq(frame->style_.border_width[3], 3.0f, 0.0));
    require_color(frame->style_.border_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
  
    check(approx_eq(sides->style_.border_width[0], 4.0f, 0.0));
    check(approx_eq(sides->style_.border_width[1], 0.0f, 0.0));
    check(approx_eq(sides->style_.border_width[2], 6.0f, 0.0));
    check(approx_eq(sides->style_.border_width[3], 1.0f, 0.0));
    require_color(sides->style_.border_color, 0x33 / 255.0f, 0x44 / 255.0f,
                  0x55 / 255.0f);
    check(frame->style_.border_style[0] == BorderStyle::Solid);
    check(frame->style_.border_style[1] == BorderStyle::Dashed);
    check(frame->style_.border_style[2] == BorderStyle::None);
    check(frame->style_.border_style[3] == BorderStyle::Dotted);
    check(sides->style_.border_style[0] == BorderStyle::Solid);
    check(sides->style_.border_style[1] == BorderStyle::None);
    check(sides->style_.border_style[2] == BorderStyle::Dashed);
    check(sides->style_.border_style[3] == BorderStyle::Dotted);
    check(relief->style_.border_style[0] == BorderStyle::Inset);
    check(relief->style_.border_style[1] == BorderStyle::Outset);
    check(relief->style_.border_style[2] == BorderStyle::Groove);
    check(relief->style_.border_style[3] == BorderStyle::Ridge);
    check(approx_eq(width_keywords->style_.border_width[0], 1.0f, 0.0));
    check(approx_eq(width_keywords->style_.border_width[1], 3.0f, 0.0));
    check(approx_eq(width_keywords->style_.border_width[2], 5.0f, 0.0));
    check(approx_eq(width_keywords->style_.border_width[3], 7.0f, 0.0));
  }
}

spec("StyleEngine parses side border longhands and logical border shorthands") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* physical = box.create("div", "physical");
    auto* logical = box.create("div", "logical");
    auto* logical_longhand = box.create("div", "logical-longhand");
    auto* logical_rtl = box.create("div", "logical-rtl");
    root->append(physical);
    root->append(logical);
    root->append(logical_longhand);
    root->append(logical_rtl);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #physical {
        border-top-width: 2px;
        border-right-width: 4px;
        border-bottom-width: 6px;
        border-left-width: 8px;
        border-top-style: solid;
        border-right-style: dashed;
        border-bottom-style: none;
        border-left-style: dotted;
        border-top-color: #556677;
      }

      #logical {
        border-inline: 3px solid #112233;
        border-block-start: 5px dashed #112233;
        border-block-end: none;
      }

      #logical-longhand {
        border-inline-width: thin thick;
        border-inline-style: dotted dashed;
        border-inline-color: #112233 #445566;
        border-block-width: medium 7px;
        border-block-style: solid hidden;
        border-block-color: #778899 #aabbcc;
      }

      #logical-rtl {
        direction: rtl;
        border-inline-start-width: 4px;
        border-inline-end-width: 6px;
        border-inline-start-style: groove;
        border-inline-end-style: ridge;
        border-inline-start-color: #102030;
        border-inline-end-color: #405060;
      }
    )");
    box.update();

    check(approx_eq(physical->style_.border_width[0], 2.0f, 0.0));
    check(approx_eq(physical->style_.border_width[1], 4.0f, 0.0));
    check(approx_eq(physical->style_.border_width[2], 6.0f, 0.0));
    check(approx_eq(physical->style_.border_width[3], 8.0f, 0.0));
    check(physical->style_.border_style[0] == BorderStyle::Solid);
    check(physical->style_.border_style[1] == BorderStyle::Dashed);
    check(physical->style_.border_style[2] == BorderStyle::None);
    check(physical->style_.border_style[3] == BorderStyle::Dotted);
    require_color(physical->style_.border_color, 0x55 / 255.0f, 0x66 / 255.0f,
                  0x77 / 255.0f);
    require_color(physical->style_.border_colors[0], 0x55 / 255.0f,
                  0x66 / 255.0f, 0x77 / 255.0f);

    check(approx_eq(logical->style_.border_width[0], 5.0f, 0.0));
    check(approx_eq(logical->style_.border_width[1], 3.0f, 0.0));
    check(approx_eq(logical->style_.border_width[2], 0.0f, 0.0));
    check(approx_eq(logical->style_.border_width[3], 3.0f, 0.0));
    check(logical->style_.border_style[0] == BorderStyle::Dashed);
    check(logical->style_.border_style[1] == BorderStyle::Solid);
    check(logical->style_.border_style[2] == BorderStyle::None);
    check(logical->style_.border_style[3] == BorderStyle::Solid);
    require_color(logical->style_.border_color, 0x11 / 255.0f, 0x22 / 255.0f,
                  0x33 / 255.0f);
    require_color(logical->style_.border_colors[0], 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
    require_color(logical->style_.border_colors[1], 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
    require_color(logical->style_.border_colors[3], 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);

    check(approx_eq(logical_longhand->style_.border_width[0], 3.0f, 0.0));
    check(approx_eq(logical_longhand->style_.border_width[1], 5.0f, 0.0));
    check(approx_eq(logical_longhand->style_.border_width[2], 0.0f, 0.0));
    check(approx_eq(logical_longhand->style_.border_width[3], 1.0f, 0.0));
    check(logical_longhand->style_.border_style[0] == BorderStyle::Solid);
    check(logical_longhand->style_.border_style[1] == BorderStyle::Dashed);
    check(logical_longhand->style_.border_style[2] == BorderStyle::None);
    check(logical_longhand->style_.border_style[3] == BorderStyle::Dotted);
    require_color(logical_longhand->style_.border_colors[0], 0x77 / 255.0f,
                  0x88 / 255.0f, 0x99 / 255.0f);
    require_color(logical_longhand->style_.border_colors[1], 0x44 / 255.0f,
                  0x55 / 255.0f, 0x66 / 255.0f);
    require_color(logical_longhand->style_.border_colors[2], 0xaa / 255.0f,
                  0xbb / 255.0f, 0xcc / 255.0f);
    require_color(logical_longhand->style_.border_colors[3], 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);

    check(approx_eq(logical_rtl->style_.border_width[1], 4.0f, 0.0));
    check(approx_eq(logical_rtl->style_.border_width[3], 6.0f, 0.0));
    check(logical_rtl->style_.border_style[1] == BorderStyle::Groove);
    check(logical_rtl->style_.border_style[3] == BorderStyle::Ridge);
    require_color(logical_rtl->style_.border_colors[1], 0x10 / 255.0f,
                  0x20 / 255.0f, 0x30 / 255.0f);
    require_color(logical_rtl->style_.border_colors[3], 0x40 / 255.0f,
                  0x50 / 255.0f, 0x60 / 255.0f);
  }
}

spec("StyleEngine expands border-color values per side") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* one = box.create("div", "one");
    auto* two = box.create("div", "two");
    auto* three = box.create("div", "three");
    auto* four = box.create("div", "four");
    auto* cascade = box.create("div", "cascade");
    root->append(one);
    root->append(two);
    root->append(three);
    root->append(four);
    root->append(cascade);
    box.set_root(root);

    box.load_css(R"(
      #one { border-color: #111111; }
      #two { border-color: #111111 #222222; }
      #three { border-color: #111111 #222222 #333333; }
      #four { border-color: #111111 #222222 #333333 #444444; }
      #cascade {
        border-color: #111111;
        border-left-color: #556677;
      }
    )");
    box.update();

    require_color(one->style_.border_colors[0], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    require_color(one->style_.border_colors[3], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);

    require_color(two->style_.border_colors[0], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    require_color(two->style_.border_colors[1], 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);
    require_color(two->style_.border_colors[2], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    require_color(two->style_.border_colors[3], 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);

    require_color(three->style_.border_colors[0], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    require_color(three->style_.border_colors[1], 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);
    require_color(three->style_.border_colors[2], 0x33 / 255.0f,
                  0x33 / 255.0f, 0x33 / 255.0f);
    require_color(three->style_.border_colors[3], 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);

    require_color(four->style_.border_colors[0], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    require_color(four->style_.border_colors[1], 0x22 / 255.0f,
                  0x22 / 255.0f, 0x22 / 255.0f);
    require_color(four->style_.border_colors[2], 0x33 / 255.0f,
                  0x33 / 255.0f, 0x33 / 255.0f);
    require_color(four->style_.border_colors[3], 0x44 / 255.0f,
                  0x44 / 255.0f, 0x44 / 255.0f);

    require_color(cascade->style_.border_colors[0], 0x11 / 255.0f,
                  0x11 / 255.0f, 0x11 / 255.0f);
    require_color(cascade->style_.border_colors[3], 0x55 / 255.0f,
                  0x66 / 255.0f, 0x77 / 255.0f);
    require_color(cascade->style_.border_color, 0x55 / 255.0f,
                  0x66 / 255.0f, 0x77 / 255.0f);
  }
}

spec("LayoutManager clamps layout sizes against min and max constraints") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* explicit_box = box.create("div", "explicit");
    auto* auto_text = box.create("div", "auto-text");
    auto* wrap = box.create("div", "wrap");
    auto* inner = box.create("div", "inner");
    auto_text->set_text("Hi");
    wrap->append(inner);
    root->append(explicit_box);
    root->append(auto_text);
    root->append(wrap);
    box.set_root(root);
    box.set_viewport(480.0f, 320.0f);
  
    box.load_css(R"(
      #root {
        width: 400px;
        height: 300px;
      }
      #explicit {
        width: 50px;
        min-width: 120px;
        height: 200px;
        max-height: 80px;
      }
      #auto-text {
        min-width: 100px;
        min-height: 40px;
      }
      #wrap {
        max-width: 180px;
      }
      #inner {
        width: 260px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(explicit_box->layout_width(), 120.0f, 0.0));
    check(approx_eq(explicit_box->layout_height(), 80.0f, 0.0));
    check(approx_eq(auto_text->layout_width(), 100.0f, 0.0));
    check(approx_eq(auto_text->layout_height(), 40.0f, 0.0));
    check(approx_eq(wrap->layout_width(), 180.0f, 0.0));
  }
}

spec("StyleEngine applies percentage sizes from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);
    box.set_viewport(480.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        width: 400px;
        height: 200px;
      }
      #child {
        width: 50%;
        height: 25%;
      }
    )");
    box.update();
  
    check(child->computed_style->width_is_percent);
    check(child->computed_style->height_is_percent);
    check(approx_eq(child->layout_width(), 200.0f, 0.1f));
    check(approx_eq(child->layout_height(), 50.0f, 0.1f));
  }
}

spec("StyleEngine maps logical size properties to physical layout axes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* child = box.create("div", "child");
    root->append(child);
    box.set_root(root);
    box.set_viewport(480.0f, 240.0f);

    box.load_css(R"(
      #root {
        width: 400px;
        height: 200px;
      }
      #child {
        inline-size: 50%;
        block-size: 25%;
        min-inline-size: 230px;
        max-block-size: 40px;
      }
    )");
    box.update();

    check(child->computed_style->width_is_percent);
    check(child->computed_style->height_is_percent);
    check(approx_eq(child->layout_width(), 230.0f, 0.1f));
    check(approx_eq(child->layout_height(), 40.0f, 0.1f));
  }
}

spec("StyleEngine applies aspect-ratio from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* hero = box.create("div", "hero");
    auto* poster = box.create("div", "poster");
    root->append(hero);
    root->append(poster);
    box.set_root(root);
    box.set_viewport(400.0f, 300.0f);
  
    box.load_css(R"(
      #hero {
        width: 160px;
        aspect-ratio: 16 / 9;
      }
      #poster {
        height: 120px;
        aspect-ratio: 4 / 3;
      }
    )");
    box.update();
  
    check(approx_eq(hero->layout_width(), 160.0f, 0.1f));
    check(approx_eq(hero->layout_height(), 90.0f, 0.1f));
    check(approx_eq(poster->layout_width(), 160.0f, 0.1f));
    check(approx_eq(poster->layout_height(), 120.0f, 0.1f));
  }
}

spec("LayoutManager applies line-height to plain text auto height") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* copy = box.create("div", "copy");
    copy->set_text("Hello");
    root->append(copy);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #copy {
        font-size: 10px;
        line-height: 2;
      }
    )");
    box.update();
  
    check(approx_eq(copy->layout_height(), 20.0f, 0.1f));
  }
}

spec("LayoutManager applies multiline pre-wrap text height to plain text auto height") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* copy = box.create("div", "copy");
    copy->set_text("One\nTwo\nThree");
    root->append(copy);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #copy {
        width: 120px;
        font-size: 10px;
        line-height: 2;
        white-space: pre-wrap;
      }
    )");
    box.update();

    check(approx_eq(copy->layout_height(), 60.0f, 0.1f));
  }
}

spec("StyleEngine applies absolute and fixed positioning from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* absolute = box.create("div", "absolute");
    auto* fixed = box.create("div", "fixed");
    root->append(absolute);
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);
  
    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #absolute {
        position: absolute;
        left: 15px;
        top: 10px;
        width: 40px;
        height: 30px;
      }
      #fixed {
        position: fixed;
        right: 10px;
        bottom: 20px;
        width: 50px;
        height: 30px;
      }
    )");
    box.update();
  
    check(absolute->position_mode() == flex::PositionMode::Absolute);
    check(approx_eq(absolute->x(), 15.0f, 0.1f));
    check(approx_eq(absolute->y(), 10.0f, 0.1f));
    check(fixed->position_mode() == flex::PositionMode::Fixed);
    check(approx_eq(fixed->x(), 440.0f, 0.1f));
    check(approx_eq(fixed->y(), 350.0f, 0.1f));
  }
}

spec("StyleEngine applies inset shorthand to positioned elements") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* absolute = box.create("div", "absolute");
    auto* fixed = box.create("div", "fixed");
    root->append(absolute);
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);
  
    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #absolute {
        position: absolute;
        inset: 10px 20px 30px 40px;
        width: 40px;
        height: 30px;
      }
      #fixed {
        position: fixed;
        inset: 15px;
        width: 50px;
        height: 30px;
      }
    )");
    box.update();
  
    check(approx_eq(absolute->x(), 40.0f, 0.1f));
    check(approx_eq(absolute->y(), 10.0f, 0.1f));
    check(approx_eq(absolute->computed_style->right, 20.0f, 0.1f));
    check(approx_eq(absolute->computed_style->bottom, 30.0f, 0.1f));
  
    check(approx_eq(fixed->x(), 15.0f, 0.1f));
    check(approx_eq(fixed->y(), 15.0f, 0.1f));
    check(approx_eq(fixed->computed_style->top, 15.0f, 0.1f));
    check(approx_eq(fixed->computed_style->left, 15.0f, 0.1f));
  }
}

spec("StyleEngine applies inset-x and inset-y shorthands to positioned elements") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* absolute = box.create("div", "absolute");
    auto* fixed = box.create("div", "fixed");
    root->append(absolute);
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);
  
    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #absolute {
        position: absolute;
        inset-x: 40px 20px;
        inset-y: 10px 30px;
        width: 50px;
        height: 40px;
      }
      #fixed {
        position: fixed;
        inset-x: 25px;
        inset-y: 15px 35px;
        width: 60px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(absolute->x(), 40.0f, 0.1f));
    check(approx_eq(absolute->y(), 10.0f, 0.1f));
    check(approx_eq(absolute->computed_style->right, 20.0f, 0.1f));
    check(approx_eq(absolute->computed_style->bottom, 30.0f, 0.1f));
  
    check(approx_eq(fixed->x(), 25.0f, 0.1f));
    check(approx_eq(fixed->y(), 15.0f, 0.1f));
    check(approx_eq(fixed->computed_style->left, 25.0f, 0.1f));
    check(approx_eq(fixed->computed_style->right, 25.0f, 0.1f));
    check(approx_eq(fixed->computed_style->top, 15.0f, 0.1f));
    check(approx_eq(fixed->computed_style->bottom, 35.0f, 0.1f));
  }
}

spec("StyleEngine parses linear-gradient backgrounds into computed style") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    root->append(gradient);
    box.set_root(root);
  
    box.load_css(R"(
      #gradient {
        background: linear-gradient(to right, #ff0000 0%, #0000ff 100%);
      }
    )");
    box.update();
  
    check(gradient->computed_style->has_gradient);
    check(gradient->computed_style->gradient.stop_count == 2);
    check(approx_eq(gradient->computed_style->gradient.angle, 90.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[0].offset, 0.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[1].offset, 1.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[0].color.r, 1.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[1].color.b, 1.0f, 0.001f));
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_gradient_type")) == "linear");
  }
}

spec("StyleEngine parses radial-gradient background images and background tiling") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    root->append(gradient);
    box.set_root(root);
  
    box.load_css(R"(
      #gradient {
        background-color: #112233;
        background-image: radial-gradient(circle at right top, #ff0000 0%, transparent 70%);
        background-position: center;
        background-size: 40px 20px;
        background-repeat: no-repeat;
      }
    )");
    box.update();
  
    check(gradient->computed_style->has_gradient);
    check(gradient->computed_style->gradient.stop_count == 2);
    check(approx_eq(gradient->computed_style->gradient.stops[0].offset, 0.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[1].offset, 0.7f, 0.001f));
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_gradient_type")) == "radial");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_radial_position")) == "right top");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_radial_size")) == "farthest-corner");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_position")) == "center");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_size")) == "40px 20px");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_repeat")) == "no-repeat");
    require_color(gradient->computed_style->background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
  }
}

spec("StyleEngine parses layered gradient background images and per-layer metadata") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    root->append(gradient);
    box.set_root(root);

    box.load_css(R"(
      #gradient {
        background-image:
          linear-gradient(90deg, #ff0000 0%, #0000ff 100%),
          radial-gradient(circle at right top, #00ff00 0%, transparent 70%);
        background-position: left top, center;
        background-size: 100% 100%, 40px 20px;
        background-repeat: space, round;
      }
    )");
    box.update();

    check(gradient->computed_style->background_layers.size() == 2);
    if (gradient->computed_style->background_layers.size() != 2) {
      return;
    }

    check(gradient->computed_style->has_gradient);
    check(gradient->computed_style->gradient.stop_count == 2);
    check(approx_eq(gradient->computed_style->gradient.angle, 90.0f, 0.001f));
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_position")) == "left top");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_size")) == "100% 100%");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_repeat")) == "space");

    const auto& first = gradient->computed_style->background_layers[0];
    check(first.gradient_type == "linear");
    check(first.position == "left top");
    check(first.size == "100% 100%");
    check(first.repeat == "space");
    check(approx_eq(first.gradient.angle, 90.0f, 0.001f));

    const auto& second = gradient->computed_style->background_layers[1];
    check(second.gradient_type == "radial");
    check(second.position == "center");
    check(second.size == "40px 20px");
    check(second.repeat == "round");
    check(second.radial_position == "right top");
    check(second.radial_size == "farthest-corner");
    check(approx_eq(second.gradient.stops[1].offset, 0.7f, 0.001f));
  }
}

spec("StyleEngine merges background-position-x and background-position-y") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    auto* layered = box.create("div", "layered");
    root->append(panel);
    root->append(layered);
    box.set_root(root);

    box.load_css(R"(
      #panel {
        background-position-x: right 10px;
        background-position-y: bottom 20px;
      }
      #layered {
        background-image:
          url("hero.png"),
          linear-gradient(90deg, #ff0000 0%, #0000ff 100%);
        background-position-x: right 10px, left;
        background-position-y: bottom 20px, top;
      }
    )");
    box.update();

    check(panel->computed_style->get_variable(
              Symbol("__flex_background_position")) ==
          "right 10px bottom 20px");
    check(panel->computed_style->get_variable(
              Symbol("__flex_background_position_x")) == "right 10px");
    check(panel->computed_style->get_variable(
              Symbol("__flex_background_position_y")) == "bottom 20px");

    check(layered->computed_style->background_layers.size() == 2);
    if (layered->computed_style->background_layers.size() != 2) {
      return;
    }
    check(layered->computed_style->background_layers[0].position ==
          "right 10px bottom 20px");
    check(layered->computed_style->background_layers[1].position ==
          "left top");
    check(layered->computed_style->get_variable(
              Symbol("__flex_background_position")) ==
          "right 10px bottom 20px");
  }
}

spec("StyleEngine parses url background images into layered metadata") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);

    box.load_css(R"(
      #panel {
        background-image: url("hero.png"), linear-gradient(90deg, #ff0000 0%, #0000ff 100%);
        background-position: center, left top;
        background-size: 24px 12px, 100% 100%;
        background-repeat: repeat-x, no-repeat;
      }
    )");
    box.update();

    check(panel->computed_style->background_layers.size() == 2);
    if (panel->computed_style->background_layers.size() != 2) {
      return;
    }

    const auto& first = panel->computed_style->background_layers[0];
    check_false(first.has_gradient);
    check(first.has_image_url);
    check(first.image_url == "hero.png");
    check(first.position == "center");
    check(first.size == "24px 12px");
    check(first.repeat == "repeat-x");

    const auto& second = panel->computed_style->background_layers[1];
    check(second.has_gradient);
    check_false(second.has_image_url);
    check(second.position == "left top");
    check(second.size == "100% 100%");
    check(second.repeat == "no-repeat");
  }
}

spec("StyleEngine preserves cover and contain background-size keywords in layered metadata") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);

    box.load_css(R"(
      #panel {
        background-image:
          url("hero.png"),
          linear-gradient(90deg, #ff0000 0%, #0000ff 100%);
        background-position: center, center;
        background-size: cover, contain;
        background-repeat: no-repeat, no-repeat;
      }
    )");
    box.update();

    check(panel->computed_style->background_layers.size() == 2);
    if (panel->computed_style->background_layers.size() != 2) {
      return;
    }

    const auto& image = panel->computed_style->background_layers[0];
    check(image.has_image_url);
    check(image.size == "cover");
    check(image.position == "center");
    check(image.repeat == "no-repeat");

    const auto& gradient = panel->computed_style->background_layers[1];
    check(gradient.has_gradient);
    check(gradient.size == "contain");
    check(gradient.position == "center");
    check(gradient.repeat == "no-repeat");
  }
}

spec("StyleEngine parses background shorthand with gradient tiling metadata") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    root->append(gradient);
    box.set_root(root);
  
    box.load_css(R"(
      #gradient {
        background: radial-gradient(circle at right top, #ff0000 0%, transparent 70%)
                    center / 40px 20px repeat no-repeat #112233;
      }
    )");
    box.update();
  
    check(gradient->computed_style->has_gradient);
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_gradient_type")) == "radial");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_radial_position")) == "right top");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_position")) == "center");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_size")) == "40px 20px");
    check(gradient->computed_style->get_variable(
                Symbol("__flex_background_repeat")) == "repeat no-repeat");
    require_color(gradient->computed_style->background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
  }
}

spec("StyleEngine parses layered background shorthand gradients and color") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    root->append(gradient);
    box.set_root(root);

    box.load_css(R"(
      #gradient {
        background:
          linear-gradient(90deg, #ff0000 0%, #0000ff 100%) left top / 100% 100% no-repeat,
          radial-gradient(circle at right top, #00ff00 0%, transparent 70%) center / 40px 20px repeat-x,
          #112233;
      }
    )");
    box.update();

    check(gradient->computed_style->background_layers.size() == 2);
    if (gradient->computed_style->background_layers.size() != 2) {
      return;
    }

    const auto& first = gradient->computed_style->background_layers[0];
    check(first.gradient_type == "linear");
    check(first.position == "left top");
    check(first.size == "100% 100%");
    check(first.repeat == "no-repeat");

    const auto& second = gradient->computed_style->background_layers[1];
    check(second.gradient_type == "radial");
    check(second.position == "center");
    check(second.size == "40px 20px");
    check(second.repeat == "repeat-x");
    check(second.radial_position == "right top");

    require_color(gradient->computed_style->background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
  }
}

spec("StyleEngine parses layered background shorthand with url image and color") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);

    box.load_css(R"(
      #panel {
        background:
          url("hero.png") center / 24px 12px repeat-x,
          linear-gradient(90deg, #ff0000 0%, #0000ff 100%) left top / 100% 100% no-repeat,
          #112233;
      }
    )");
    box.update();

    check(panel->computed_style->background_layers.size() == 2);
    if (panel->computed_style->background_layers.size() != 2) {
      return;
    }

    const auto& first = panel->computed_style->background_layers[0];
    check(first.has_image_url);
    check(first.image_url == "hero.png");
    check(first.position == "center");
    check(first.size == "24px 12px");
    check(first.repeat == "repeat-x");

    const auto& second = panel->computed_style->background_layers[1];
    check(second.has_gradient);
    check(second.gradient_type == "linear");

    require_color(panel->computed_style->background_color, 0x11 / 255.0f,
                  0x22 / 255.0f, 0x33 / 255.0f);
  }
}

spec("StyleEngine background shorthand resets prior image tiling metadata") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    gradient->add_class("reset");
    root->append(gradient);
    box.set_root(root);
  
    box.load_css(R"(
      #gradient {
        background-image: radial-gradient(circle at right top, #ff0000 0%, transparent 70%);
        background-position: center;
        background-size: 40px 20px;
        background-repeat: no-repeat;
        background-color: #112233;
      }
      #gradient.reset {
        background: red;
      }
    )");
    box.update();
  
    check_false(gradient->computed_style->has_gradient);
    check(gradient->computed_style->variables.find(
                Symbol("__flex_background_gradient_type")) ==
            gradient->computed_style->variables.end());
    check(gradient->computed_style->variables.find(
                Symbol("__flex_background_position")) ==
            gradient->computed_style->variables.end());
    check(gradient->computed_style->variables.find(
                Symbol("__flex_background_size")) ==
            gradient->computed_style->variables.end());
    check(gradient->computed_style->variables.find(
                Symbol("__flex_background_repeat")) ==
            gradient->computed_style->variables.end());
    require_color(gradient->computed_style->background_color, 1.0f, 0.0f, 0.0f);
  }
}

spec("StyleEngine parses background-clip boxes into computed style") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    root->append(panel);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #panel {
        background-clip: border-box;
      }
      #panel.padding {
        background-clip: padding-box;
      }
      #panel.content {
        background-clip: content-box;
      }
    )");
    box.update();

    check(panel->computed_style->background_clip == BackgroundClip::BorderBox);
    check(panel->computed_style->get_variable(Symbol("--background-clip")) ==
          "border-box");

    panel->add_class("padding");
    box.update();

    check(panel->computed_style->background_clip == BackgroundClip::PaddingBox);
    check(panel->computed_style->get_variable(Symbol("--background-clip")) ==
          "padding-box");

    panel->remove_class("padding");
    panel->add_class("content");
    box.update();

    check(panel->computed_style->background_clip == BackgroundClip::ContentBox);
    check(panel->computed_style->get_variable(Symbol("--background-clip")) ==
          "content-box");
  }
}

spec("StyleEngine parses background-origin boxes into computed style") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* panel = box.create("div", "panel");
    auto* layered = box.create("div", "layered");
    auto* shorthand = box.create("div", "shorthand");
    auto* reset = box.create("div", "reset");
    root->append(panel);
    root->append(layered);
    root->append(shorthand);
    root->append(reset);
    box.set_root(root);

    box.load_css(R"(
      #panel {
        background-origin: content-box;
      }
      #layered {
        background-image: url(hero.png), linear-gradient(90deg, red, blue);
        background-origin: content-box, border-box;
        background-clip: padding-box, content-box;
      }
      #shorthand {
        background: linear-gradient(90deg, red, blue) content-box border-box no-repeat;
      }
      #reset {
        background-origin: content-box;
        background: red;
      }
    )");
    box.update();

    check(panel->computed_style->background_origin ==
          BackgroundClip::ContentBox);
    check(panel->computed_style->get_variable(
              Symbol("__flex_background_origin")) == "content-box");

    check(layered->computed_style->background_layers.size() == 2);
    if (layered->computed_style->background_layers.size() == 2) {
      check(layered->computed_style->background_layers[0].origin ==
            "content-box");
      check(layered->computed_style->background_layers[1].origin ==
            "border-box");
      check(layered->computed_style->background_layers[0].clip ==
            "padding-box");
      check(layered->computed_style->background_layers[1].clip ==
            "content-box");
    }
    check(layered->computed_style->background_origin ==
          BackgroundClip::ContentBox);
    check(layered->computed_style->background_clip ==
          BackgroundClip::PaddingBox);

    check(shorthand->computed_style->background_layers.size() == 1);
    if (shorthand->computed_style->background_layers.size() == 1) {
      check(shorthand->computed_style->background_layers[0].origin ==
            "content-box");
      check(shorthand->computed_style->background_layers[0].clip ==
            "border-box");
    }
    check(shorthand->computed_style->background_origin ==
          BackgroundClip::ContentBox);
    check(shorthand->computed_style->background_clip ==
          BackgroundClip::BorderBox);

    check(reset->computed_style->background_origin ==
          BackgroundClip::PaddingBox);
    check(reset->computed_style->background_clip ==
          BackgroundClip::BorderBox);
    check(reset->computed_style->variables.find(
              Symbol("__flex_background_origin")) ==
          reset->computed_style->variables.end());
  }
}

spec("StyleEngine distributes omitted linear-gradient stop offsets") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* gradient = box.create("div", "gradient");
    root->append(gradient);
    box.set_root(root);
  
    box.load_css(R"(
      #gradient {
        background: linear-gradient(90deg, red, blue 75%);
      }
    )");
    box.update();
  
    check(gradient->computed_style->has_gradient);
    check(gradient->computed_style->gradient.stop_count == 2);
    check(approx_eq(gradient->computed_style->gradient.angle, 90.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[0].offset, 0.0f, 0.001f));
    check(approx_eq(gradient->computed_style->gradient.stops[1].offset, 0.75f, 0.001f));
  }
}

spec("StyleEngine resolves fixed percentage sizes against viewport") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* fixed = box.create("div", "fixed");
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);
  
    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #fixed {
        position: fixed;
        right: 10px;
        bottom: 20px;
        width: 50%;
        height: 25%;
      }
    )");
    box.update();
  
    check(approx_eq(fixed->layout_width(), 250.0f, 0.1f));
    check(approx_eq(fixed->layout_height(), 100.0f, 0.1f));
    check(approx_eq(fixed->x(), 240.0f, 0.1f));
    check(approx_eq(fixed->y(), 280.0f, 0.1f));
  }
}

spec("StyleEngine stretches auto-sized fixed elements across opposing insets") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* fixed = box.create("div", "fixed");
    root->append(fixed);
    box.set_root(root);
    box.set_viewport(500.0f, 400.0f);

    box.load_css(R"(
      #root {
        width: 300px;
        height: 200px;
      }
      #fixed {
        position: fixed;
        left: 10px;
        right: 20px;
        top: 15px;
        bottom: 25px;
      }
    )");
    box.update();

    check(approx_eq(fixed->layout_width(), 470.0f, 0.1f));
    check(approx_eq(fixed->layout_height(), 360.0f, 0.1f));
    check(approx_eq(fixed->x(), 10.0f, 0.1f));
    check(approx_eq(fixed->y(), 15.0f, 0.1f));
  }
}

spec("StyleEngine applies sticky top positioning inside scroll containers") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* scroll = box.create("div", "scroll");
    auto* intro = box.create("div", "intro");
    auto* header = box.create("div", "header");
    auto* body = box.create("div", "body");
    root->append(scroll);
    scroll->append(intro);
    scroll->append(header);
    scroll->append(body);
    box.set_root(root);
    box.set_viewport(160.0f, 120.0f);

    box.load_css(R"(
      #scroll {
        width: 120px;
        height: 60px;
        overflow: auto;
      }

      #intro {
        width: 120px;
        height: 30px;
      }

      #header {
        width: 120px;
        height: 20px;
        position: sticky;
        top: 0;
      }

      #body {
        width: 120px;
        height: 120px;
      }
    )");
    box.update();

    check(header->computed_style->position == Position::Sticky);
    check(approx_eq(header->absolute_y(), 30.0f, 0.001f));

    check(scroll->set_scroll_offset(0.0f, 40.0f));
    box.update();

    check(approx_eq(header->absolute_y(), 0.0f, 0.001f));
  }
}

spec("StyleEngine applies visibility and z-index from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* elem = box.create("div", "elem");
    auto* collapsed = box.create("div", "collapsed");
    root->append(elem);
    root->append(collapsed);
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);

    box.load_css(R"(
      #elem {
        visibility: hidden;
        z-index: 7;
        width: 40px;
        height: 20px;
      }
      #collapsed {
        visibility: collapse;
      }
    )");
    box.update();

    check(elem->computed_style->visibility == Visibility::Hidden);
    check_false(elem->is_visible());
    check(elem->z_index() == 7);
    check(collapsed->computed_style->visibility == Visibility::Collapse);
    check_false(collapsed->is_visible());
  }
}

spec("StyleEngine stores transition shorthand from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "elem");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      #elem {
        transition: opacity 0.2s ease-in 50ms;
      }
    )");
    box.update();
  
    check(elem->computed_style->transition == "opacity 0.2s ease-in 50ms");
  }
}

spec("StyleEngine stores transition list shorthand from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "elem");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      #elem {
        transition: opacity 0.1s linear, transform 0.2s ease-out 50ms;
      }
    )");
    box.update();
  
    check(elem->computed_style->transition ==
            "opacity 0.1s linear, transform 0.2s ease-out 50ms");
  }
}

spec("StyleEngine synthesizes transition longhands into shorthand") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "elem");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      #elem {
        transition-property: opacity, transform;
        transition-duration: 0.1s, 0.2s;
        transition-timing-function: linear, ease-out;
        transition-delay: 0ms, 50ms;
      }
    )");
    box.update();
  
    check(elem->computed_style->transition ==
            "opacity 0.1s linear 0ms, transform 0.2s ease-out 50ms");
  }
}

spec("parse_transition handles duration delay and easing") {
  it("runs") {
    const auto def = parse_transition("opacity 0.2s ease-in 50ms");
    check(def.property == "opacity");
    check(approx_eq(def.duration_ms, 200.0f, 0.1f));
    check(approx_eq(def.delay_ms, 50.0f, 0.1f));
    check(def.easing == EasingType::EaseIn);
  }
}

spec("parse_transition defaults property to all when omitted") {
  it("runs") {
    const auto def = parse_transition("150ms linear");
    check(def.property == "all");
    check(approx_eq(def.duration_ms, 150.0f, 0.1f));
    check(approx_eq(def.delay_ms, 0.0f, 0.1f));
    check(def.easing == EasingType::Linear);
  }
}

spec("StyleEngine parses keyframes and animation shorthand from CSS") {
  it("runs") {
    StyleEngine engine;
    engine.parse_css(R"(
      @keyframes fade-in {
        from { opacity: 0; transform: translateX(0px); }
        50% { opacity: 0.4; }
        to { opacity: 1; transform: translateX(20px); }
      }
    )");
  
    const auto* keyframes = engine.keyframes("fade-in");
    check(keyframes != nullptr);
    check(keyframes->size() == 3);
    if (keyframes->size() != 3) {
      return;
    }
    check(approx_eq((*keyframes)[0].offset, 0.0f, 0.001f));
    check(approx_eq((*keyframes)[1].offset, 0.5f, 0.001f));
    check(approx_eq((*keyframes)[2].offset, 1.0f, 0.001f));
    const auto opacity_it = (*keyframes)[0].properties.find("opacity");
    check(opacity_it != (*keyframes)[0].properties.end());
    check(opacity_it->second == "0");
    const auto transform_it = (*keyframes)[2].properties.find("transform");
    check(transform_it != (*keyframes)[2].properties.end());
    check(transform_it->second == "translateX(20px)");
  
    Box box(nullptr);
    auto* elem = box.create("div", "elem");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #elem {
        animation: fade-in 0.2s linear 50ms infinite both reverse paused;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    check(elem->computed_style->animation_name == "fade-in");
    check(approx_eq(elem->computed_style->animation_duration_ms, 200.0f, 0.1f));
    check(approx_eq(elem->computed_style->animation_delay_ms, 50.0f, 0.1f));
    check(elem->computed_style->animation_timing == EasingType::Linear);
    check(elem->computed_style->animation_infinite);
    check(elem->computed_style->animation_fill_mode == AnimationFillMode::Both);
    check(elem->computed_style->animation_direction ==
            AnimationDirection::Reverse);
    check(elem->computed_style->animation_play_state ==
            AnimationPlayState::Paused);
    check(elem->computed_style->animations.size() == 1);
    if (elem->computed_style->animations.size() != 1) {
      return;
    }
  }
}

spec("StyleEngine parses multi animation shorthand lists from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "elem");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      #elem {
        animation:
          fade-in 0.2s linear forwards,
          slide-in 0.4s ease-in 50ms 2 alternate paused;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    check(elem->computed_style->animations.size() == 2);
    if (elem->computed_style->animations.size() != 2) {
      return;
    }
    check(elem->computed_style->animations[0].name == "fade-in");
    check(approx_eq(elem->computed_style->animations[0].duration_ms, 200.0f, 0.1f));
    check(elem->computed_style->animations[0].timing == EasingType::Linear);
    check(elem->computed_style->animations[0].fill_mode ==
            AnimationFillMode::Forwards);
    check(elem->computed_style->animations[1].name == "slide-in");
    check(approx_eq(elem->computed_style->animations[1].duration_ms, 400.0f, 0.1f));
    check(approx_eq(elem->computed_style->animations[1].delay_ms, 50.0f, 0.1f));
    check(approx_eq(elem->computed_style->animations[1].iteration_count, 2.0f, 0.001f));
    check(elem->computed_style->animations[1].direction ==
            AnimationDirection::Alternate);
    check(elem->computed_style->animations[1].play_state ==
            AnimationPlayState::Paused);
  }
}

spec("StyleEngine parses multi animation longhand lists from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "elem");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      #elem {
        animation-name: fade-in, slide-in;
        animation-duration: 0.2s, 0.4s;
        animation-delay: 0s, 50ms;
        animation-timing-function: linear, ease-in;
        animation-iteration-count: 1, 2;
        animation-fill-mode: forwards, both;
        animation-direction: normal, alternate;
        animation-play-state: running, paused;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    check(elem->computed_style->animations.size() == 2);
    if (elem->computed_style->animations.size() != 2) {
      return;
    }
    check(elem->computed_style->animations[0].name == "fade-in");
    check(approx_eq(elem->computed_style->animations[0].duration_ms, 200.0f, 0.1f));
    check(elem->computed_style->animations[0].timing == EasingType::Linear);
    check(elem->computed_style->animations[0].fill_mode ==
            AnimationFillMode::Forwards);
    check(elem->computed_style->animations[0].play_state ==
            AnimationPlayState::Running);
  
    check(elem->computed_style->animations[1].name == "slide-in");
    check(approx_eq(elem->computed_style->animations[1].duration_ms, 400.0f, 0.1f));
    check(approx_eq(elem->computed_style->animations[1].delay_ms, 50.0f, 0.1f));
    check(elem->computed_style->animations[1].timing == EasingType::EaseIn);
    check(approx_eq(elem->computed_style->animations[1].iteration_count, 2.0f, 0.001f));
    check(elem->computed_style->animations[1].fill_mode ==
            AnimationFillMode::Both);
    check(elem->computed_style->animations[1].direction ==
            AnimationDirection::Alternate);
    check(elem->computed_style->animations[1].play_state ==
            AnimationPlayState::Paused);
  }
}

spec("Box registers and samples CSS animations at runtime") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "anim");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #anim {
        opacity: 0.2;
        animation: fade-in 0.2s linear forwards;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
    check(box.animations().has_active(element_id, box.time()));
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.0f, 0.001f));
  
    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.5f, 0.02f));
  
    box.update_time(100.0f);
    check_false(box.animations().has_active(element_id, box.time()));
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 1.0f, 0.001f));
  }
}

spec("Box samples reverse and alternate CSS animations at runtime") {
  it("runs") {
    Box reverse_box(nullptr);
    auto* reverse_elem = reverse_box.create("div", "reverse");
    reverse_box.set_root(reverse_elem);
    reverse_box.set_viewport(320.0f, 120.0f);
  
    reverse_box.load_css(R"(
      @keyframes fade {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #reverse {
        animation: fade 0.2s linear reverse forwards;
        width: 40px;
        height: 20px;
      }
    )");
    reverse_box.update();
  
    const auto reverse_id = reinterpret_cast<std::uintptr_t>(reverse_elem);
    check(approx_eq(reverse_box.animations().get(reverse_id, "opacity", reverse_elem->computed_style->opacity, reverse_box.time()), 1.0f, 0.001f));
  
    reverse_box.update_time(100.0f);
    check(approx_eq(reverse_box.animations().get(reverse_id, "opacity", reverse_elem->computed_style->opacity, reverse_box.time()), 0.5f, 0.02f));
  
    reverse_box.update_time(100.0f);
    check(approx_eq(reverse_box.animations().get(reverse_id, "opacity", reverse_elem->computed_style->opacity, reverse_box.time()), 0.0f, 0.001f));
  
    Box alternate_box(nullptr);
    auto* alternate_elem = alternate_box.create("div", "alternate");
    alternate_box.set_root(alternate_elem);
    alternate_box.set_viewport(320.0f, 120.0f);
  
    alternate_box.load_css(R"(
      @keyframes fade {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #alternate {
        animation: fade 0.2s linear 2 alternate forwards;
        width: 40px;
        height: 20px;
      }
    )");
    alternate_box.update();
  
    const auto alternate_id = reinterpret_cast<std::uintptr_t>(alternate_elem);
    alternate_box.update_time(250.0f);
    check(approx_eq(alternate_box.animations().get(alternate_id, "opacity", alternate_elem->computed_style->opacity, alternate_box.time()), 0.75f, 0.02f));
  
    alternate_box.update_time(150.0f);
    check_false(alternate_box.animations().has_active(alternate_id,
                                                        alternate_box.time()));
    check(approx_eq(alternate_box.animations().get(alternate_id, "opacity", alternate_elem->computed_style->opacity, alternate_box.time()), 0.0f, 0.001f));
  }
}

spec("Box pauses and resumes CSS animations at runtime") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "anim");
    elem->add_class("paused");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #anim {
        opacity: 0.2;
        animation: fade-in 0.2s linear forwards;
        width: 40px;
        height: 20px;
      }
      #anim.paused {
        animation-play-state: paused;
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.0f, 0.001f));
  
    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.0f, 0.001f));
  
    elem->remove_class("paused");
    box.update();
    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.5f, 0.02f));
  
    elem->add_class("paused");
    box.update();
    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.5f, 0.02f));
  
    elem->remove_class("paused");
    box.update();
    box.update_time(100.0f);
    check_false(box.animations().has_active(element_id, box.time()));
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 1.0f, 0.001f));
  }
}

spec("Box runs multiple CSS animations from shorthand lists at runtime") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "anim");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      @keyframes slide-in {
        from { transform: translateX(0px); }
        to { transform: translateX(20px); }
      }
      #anim {
        opacity: 0.2;
        animation: fade-in 0.2s linear forwards,
                   slide-in 0.2s linear forwards;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
    check(box.animations().has_active(element_id, box.time()));
  
    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.5f, 0.02f));
    check(approx_eq(box.animations().get(element_id, "transform-x", elem->computed_style->transform_x, box.time()), 10.0f, 0.02f));
  }
}

spec("Box runs multiple CSS animations from longhand lists at runtime") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "anim");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      @keyframes fade-in {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      @keyframes slide-in {
        from { transform: translateX(0px); }
        to { transform: translateX(20px); }
      }
      #anim {
        opacity: 0.2;
        animation-name: fade-in, slide-in;
        animation-duration: 0.2s, 0.2s;
        animation-timing-function: linear, linear;
        animation-fill-mode: forwards, forwards;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
    check(box.animations().has_active(element_id, box.time()));
  
    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "opacity", elem->computed_style->opacity, box.time()), 0.5f, 0.02f));
    check(approx_eq(box.animations().get(element_id, "transform-x", elem->computed_style->transform_x, box.time()), 10.0f, 0.02f));
  }
}

spec("Box starts opacity transitions on pseudo state changes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        opacity: 1;
        transition: opacity 0.2s linear;
      }
      .btn:hover {
        opacity: 0.2;
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(button);
    button->set_hover(true);
    box.update();
  
    check(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(element_id, "opacity", button->computed_style->opacity, box.time()), 1.0f, 0.001f));
  
    box.update_time(100.0f);
    check(approx_eq(box.transitions().get(element_id, "opacity", button->computed_style->opacity, box.time()), 0.6f, 0.01f));
  
    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(element_id, "opacity", button->computed_style->opacity, box.time()), 0.2f, 0.001f));
  }
}

spec("Box starts transform transitions on pseudo state changes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        transform: translate(0, 0) scale(1);
        transition: transform 0.2s linear;
      }
      .btn:hover {
        transform: translate(20, 10) scale(1.5);
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(button);
    button->set_hover(true);
    box.update();
  
    check(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 0.0f, 0.001f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.0f, 0.001f));
  
    box.update_time(100.0f);
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 10.0f, 0.02f));
    check(approx_eq(box.transitions().get(element_id, "transform-y", button->computed_style->transform_y, box.time()), 5.0f, 0.02f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.25f, 0.02f));
  
    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 20.0f, 0.001f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.5f, 0.001f));
  }
}

spec("Box applies transition lists with per-property durations") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        opacity: 1;
        transform: translate(0, 0) scale(1);
        transition: opacity 0.1s linear, transform 0.2s linear;
      }
      .btn:hover {
        opacity: 0.2;
        transform: translate(20, 10) scale(1.5);
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(button);
    button->set_hover(true);
    box.update();
  
    check(box.transitions().has_active(element_id, box.time()));
  
    box.update_time(100.0f);
    check(approx_eq(box.transitions().get(element_id, "opacity", button->computed_style->opacity, box.time()), 0.2f, 0.001f));
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 10.0f, 0.02f));
    check(approx_eq(box.transitions().get(element_id, "transform-y", button->computed_style->transform_y, box.time()), 5.0f, 0.02f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.25f, 0.02f));
    check(box.transitions().has_active(element_id, box.time()));
  
    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 20.0f, 0.001f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.5f, 0.001f));
  }
}

spec("Box applies transition longhands with per-property delays") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      .btn {
        opacity: 1;
        transform: translate(0, 0) scale(1);
        transition-property: opacity, transform;
        transition-duration: 0.1s, 0.2s;
        transition-timing-function: linear, linear;
        transition-delay: 0ms, 50ms;
      }
      .btn:hover {
        opacity: 0.2;
        transform: translate(20, 10) scale(1.5);
      }
    )");
    box.update();
  
    const auto element_id = reinterpret_cast<std::uintptr_t>(button);
    button->set_hover(true);
    box.update();
  
    check(box.transitions().has_active(element_id, box.time()));
  
    box.update_time(100.0f);
    check(approx_eq(box.transitions().get(element_id, "opacity", button->computed_style->opacity, box.time()), 0.2f, 0.001f));
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 5.0f, 0.03f));
    check(approx_eq(box.transitions().get(element_id, "transform-y", button->computed_style->transform_y, box.time()), 2.5f, 0.03f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.125f, 0.03f));
    check(box.transitions().has_active(element_id, box.time()));
  
    box.update_time(150.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(element_id, "transform-x", button->computed_style->transform_x, box.time()), 20.0f, 0.001f));
    check(approx_eq(box.transitions().get(element_id, "transform-scale", button->computed_style->transform_scale, box.time()), 1.5f, 0.001f));
  }
}

spec("Box starts background-color transitions on pseudo state changes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .btn {
        background-color: #336699;
        transition: background-color 0.2s linear;
      }
      .btn:hover {
        background-color: #99cc33;
      }
    )");
    box.update();

    const auto element_id = reinterpret_cast<std::uintptr_t>(button);
    const auto* property = detail::style_property_descriptor(
        detail::StylePropertyId::BackgroundColor);
    check_not_null(property);
    if (!property) return;

    button->set_hover(true);
    box.update();

    check(box.transitions().has_active(element_id, box.time()));
    require_color(
        box.transitions().get_color(
            element_id, *property, button->computed_style->background_color,
            box.time()),
        0x33 / 255.0f, 0x66 / 255.0f, 0x99 / 255.0f);

    box.update_time(100.0f);
    require_color(
        box.transitions().get_color(
            element_id, *property, button->computed_style->background_color,
            box.time()),
        0.4f, 0.6f, 0.4f, 1.0f, 0.02f);

    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    require_color(
        box.transitions().get_color(
            element_id, *property, button->computed_style->background_color,
            box.time()),
        0x99 / 255.0f, 0xcc / 255.0f, 0x33 / 255.0f);
  }
}

spec("Box starts border-color transitions on pseudo state changes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "cta");
    button->add_class("btn");
    root->append(button);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .btn {
        border-width: 1px;
        border-color: #224466;
        transition: border-color 0.2s linear;
      }
      .btn:hover {
        border-color: #88aa22;
      }
    )");
    box.update();

    const auto element_id = reinterpret_cast<std::uintptr_t>(button);
    const auto* property = detail::style_property_descriptor(
        detail::StylePropertyId::BorderColor);
    check_not_null(property);
    if (!property) return;

    button->set_hover(true);
    box.update();

    check(box.transitions().has_active(element_id, box.time()));
    require_color(
        box.transitions().get_color(
            element_id, *property, button->computed_style->border_color,
            box.time()),
        0x22 / 255.0f, 0x44 / 255.0f, 0x66 / 255.0f);

    box.update_time(100.0f);
    require_color(
        box.transitions().get_color(
            element_id, *property, button->computed_style->border_color,
            box.time()),
        85.0f / 255.0f, 119.0f / 255.0f, 68.0f / 255.0f, 1.0f,
        0.02f);

    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    require_color(
        box.transitions().get_color(
            element_id, *property, button->computed_style->border_color,
            box.time()),
        0x88 / 255.0f, 0xaa / 255.0f, 0x22 / 255.0f);
  }
}

spec("Box starts outline and ring transitions on focus-visible changes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* field = box.create("input", "field");
    field->add_class("field");
    root->append(field);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      .field {
        outline: none;
        ring: none;
        transition:
          outline-width 0.2s linear,
          outline-offset 0.2s linear,
          outline-color 0.2s linear,
          ring-width 0.2s linear,
          ring-offset 0.2s linear,
          ring-color 0.2s linear,
          ring-offset-color 0.2s linear;
      }
      .field:focus-visible {
        outline: 2px solid #3366ff;
        outline-offset: 4px;
        ring: 6px rgba(16,32,48,0.5) 2px;
        ring-offset-color: #f8fafc;
      }
    )");
    box.update();

    const auto element_id = reinterpret_cast<std::uintptr_t>(field);
    const auto* outline_color = detail::style_property_descriptor(
        detail::StylePropertyId::OutlineColor);
    const auto* ring_color = detail::style_property_descriptor(
        detail::StylePropertyId::RingColor);
    const auto* ring_offset_color = detail::style_property_descriptor(
        detail::StylePropertyId::RingOffsetColor);
    check_not_null(outline_color);
    check_not_null(ring_color);
    check_not_null(ring_offset_color);
    if (!outline_color || !ring_color || !ring_offset_color) return;

    field->set_focus_visible(true);
    box.update();

    check(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(
                        element_id, "outline-width",
                        field->computed_style->outline_width, box.time()),
                    0.0f, 0.001f));
    check(approx_eq(box.transitions().get(
                        element_id, "ring-width",
                        field->computed_style->ring_width, box.time()),
                    0.0f, 0.001f));

    box.update_time(100.0f);
    check(approx_eq(box.transitions().get(
                        element_id, "outline-width",
                        field->computed_style->outline_width, box.time()),
                    1.0f, 0.02f));
    check(approx_eq(box.transitions().get(
                        element_id, "outline-offset",
                        field->computed_style->outline_offset, box.time()),
                    2.0f, 0.02f));
    require_color(
        box.transitions().get_color(
            element_id, *outline_color, field->computed_style->outline_color,
            box.time()),
        (0x33 / 255.0f) * 0.5f, (0x66 / 255.0f) * 0.5f, 0.5f, 0.5f,
        0.02f);
    check(approx_eq(box.transitions().get(
                        element_id, "ring-width",
                        field->computed_style->ring_width, box.time()),
                    3.0f, 0.02f));
    check(approx_eq(box.transitions().get(
                        element_id, "ring-offset",
                        field->computed_style->ring_offset, box.time()),
                    1.0f, 0.02f));
    require_color(
        box.transitions().get_color(
            element_id, *ring_color, field->computed_style->ring_color,
            box.time()),
        8.0f / 255.0f, 16.0f / 255.0f, 24.0f / 255.0f, 0.25f,
        0.02f);
    require_color(
        box.transitions().get_color(
            element_id, *ring_offset_color,
            field->computed_style->ring_offset_color, box.time()),
        (0xf8 / 255.0f) * 0.5f, (0xfa / 255.0f) * 0.5f,
        (0xfc / 255.0f) * 0.5f, 0.5f, 0.02f);

    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(
                        element_id, "outline-width",
                        field->computed_style->outline_width, box.time()),
                    2.0f, 0.001f));
    check(approx_eq(box.transitions().get(
                        element_id, "outline-offset",
                        field->computed_style->outline_offset, box.time()),
                    4.0f, 0.001f));
    require_color(
        box.transitions().get_color(
            element_id, *outline_color, field->computed_style->outline_color,
            box.time()),
        0x33 / 255.0f, 0x66 / 255.0f, 1.0f, 1.0f);
    check(approx_eq(box.transitions().get(
                        element_id, "ring-width",
                        field->computed_style->ring_width, box.time()),
                    6.0f, 0.001f));
    check(approx_eq(box.transitions().get(
                        element_id, "ring-offset",
                        field->computed_style->ring_offset, box.time()),
                    2.0f, 0.001f));
    require_color(
        box.transitions().get_color(
            element_id, *ring_color, field->computed_style->ring_color,
            box.time()),
        16.0f / 255.0f, 32.0f / 255.0f, 48.0f / 255.0f, 0.5f);
    require_color(
        box.transitions().get_color(
            element_id, *ring_offset_color,
            field->computed_style->ring_offset_color, box.time()),
        0xf8 / 255.0f, 0xfa / 255.0f, 0xfc / 255.0f, 1.0f);
  }
}

spec("Box registers and samples CSS box-shadow animations at runtime") {
  it("runs") {
    Box box(nullptr);
    auto* elem = box.create("div", "anim");
    box.set_root(elem);
    box.set_viewport(320.0f, 120.0f);

    box.load_css(R"(
      @keyframes shadow-in {
        from { box-shadow: 0 1px 2px 0 rgba(10, 20, 30, 0.2); }
        to { box-shadow: 0 5px 10px 2px rgba(110, 120, 130, 0.8); }
      }
      #anim {
        box-shadow: 0 1px 2px 0 rgba(10, 20, 30, 0.2);
        animation: shadow-in 0.2s linear forwards;
        width: 40px;
        height: 20px;
      }
    )");
    box.update();

    const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
    check(box.animations().has_active(element_id, box.time()));
    check(approx_eq(box.animations().get(element_id, "box-shadow-offset-y",
                                         elem->computed_style->shadow.offset_y,
                                         box.time()),
                    1.0f, 0.001f));
    check(approx_eq(box.animations().get(element_id, "box-shadow-blur",
                                         elem->computed_style->shadow.blur_radius,
                                         box.time()),
                    2.0f, 0.001f));

    box.update_time(100.0f);
    check(approx_eq(box.animations().get(element_id, "box-shadow-offset-y",
                                         elem->computed_style->shadow.offset_y,
                                         box.time()),
                    3.0f, 0.02f));
    check(approx_eq(box.animations().get(element_id, "box-shadow-blur",
                                         elem->computed_style->shadow.blur_radius,
                                         box.time()),
                    6.0f, 0.02f));
    check(approx_eq(box.animations().get(element_id, "box-shadow-spread",
                                         elem->computed_style->shadow.spread_radius,
                                         box.time()),
                    1.0f, 0.02f));
    check(approx_eq(box.animations().get(element_id, "box-shadow-color-a",
                                         elem->computed_style->shadow.color.a,
                                         box.time()),
                    0.5f, 0.02f));
  }
}

spec("Box starts box-shadow transitions on pseudo state changes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #card {
        box-shadow: 0 1px 2px 0 rgba(10, 20, 30, 0.2);
        transition: box-shadow 0.2s linear;
      }
      #card:hover {
        box-shadow: 0 5px 10px 2px rgba(110, 120, 130, 0.8);
      }
    )");
    box.update();

    const auto element_id = reinterpret_cast<std::uintptr_t>(card);
    const auto* shadow_color = detail::style_property_descriptor(
        detail::StylePropertyId::BoxShadowColor);
    check_not_null(shadow_color);
    if (!shadow_color) return;

    card->set_hover(true);
    box.update();

    check(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-offset-y",
                        card->computed_style->shadow.offset_y, box.time()),
                    1.0f, 0.001f));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-blur",
                        card->computed_style->shadow.blur_radius, box.time()),
                    2.0f, 0.001f));
    require_color(
        box.transitions().get_color(
            element_id, *shadow_color, card->computed_style->shadow.color,
            box.time()),
        10.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 0.2f);

    box.update_time(100.0f);
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-offset-y",
                        card->computed_style->shadow.offset_y, box.time()),
                    3.0f, 0.02f));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-blur",
                        card->computed_style->shadow.blur_radius, box.time()),
                    6.0f, 0.02f));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-spread",
                        card->computed_style->shadow.spread_radius, box.time()),
                    1.0f, 0.02f));
    require_color(
        box.transitions().get_color(
            element_id, *shadow_color, card->computed_style->shadow.color,
            box.time()),
        60.0f / 255.0f, 70.0f / 255.0f, 80.0f / 255.0f, 0.5f,
        0.02f);

    box.update_time(100.0f);
    check_false(box.transitions().has_active(element_id, box.time()));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-offset-y",
                        card->computed_style->shadow.offset_y, box.time()),
                    5.0f, 0.001f));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-blur",
                        card->computed_style->shadow.blur_radius, box.time()),
                    10.0f, 0.001f));
    check(approx_eq(box.transitions().get(
                        element_id, "box-shadow-spread",
                        card->computed_style->shadow.spread_radius, box.time()),
                    2.0f, 0.001f));
    require_color(
        box.transitions().get_color(
            element_id, *shadow_color, card->computed_style->shadow.color,
            box.time()),
        110.0f / 255.0f, 120.0f / 255.0f, 130.0f / 255.0f, 0.8f);
  }
}

spec("StyleEngine applies overflow clipping from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* hidden = box.create("div", "hidden");
    auto* clipped = box.create("div", "clipped");
    auto* visible = box.create("div", "visible");
    root->append(hidden);
    root->append(clipped);
    root->append(visible);
    box.set_root(root);
    box.set_viewport(320.0f, 160.0f);
  
    box.load_css(R"(
      #root {
        width: 300px;
        height: 120px;
      }
      #hidden {
        overflow: hidden;
        clip-path: inset(10px 20% 5px 4px round 8px);
        width: 100px;
        height: 40px;
      }
      #clipped {
        overflow-x: clip;
        overflow-y: clip;
        width: 100px;
        height: 40px;
      }
      #visible {
        overflow-x: visible;
        overflow-y: visible;
        width: 100px;
        height: 40px;
      }
    )");
    box.update();
  
    check(hidden->computed_style->overflow_x == Overflow::Hidden);
    check(hidden->computed_style->overflow_y == Overflow::Hidden);
    check(hidden->computed_style->get_variable(Symbol("--clip-path")) ==
          "inset(10px 20% 5px 4px round 8px)");
    check(hidden->clip());
    check(approx_eq(hidden->clip_width(), 100.0f, 0.1f));
    check(approx_eq(hidden->clip_height(), 40.0f, 0.1f));

    check(clipped->computed_style->overflow_x == Overflow::Hidden);
    check(clipped->computed_style->overflow_y == Overflow::Hidden);
    check(clipped->clip());

    check(visible->computed_style->overflow_x == Overflow::Visible);
    check(visible->computed_style->overflow_y == Overflow::Visible);
    check_false(visible->clip());
  }
}

spec("StyleEngine applies box-sizing from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* border = box.create("div", "border");
    auto* content = box.create("div", "content");
    root->append(border);
    root->append(content);
    box.set_root(root);
    box.set_viewport(320.0f, 160.0f);
  
    box.load_css(R"(
      #border {
        box-sizing: border-box;
        width: 100px;
        height: 40px;
      }
      #content {
        box-sizing: content-box;
        width: 100px;
        height: 40px;
      }
    )");
    box.update();
  
    check(border->computed_style->box_sizing == BoxSizing::BorderBox);
    check(content->computed_style->box_sizing == BoxSizing::ContentBox);
    check(border->box_sizing() == flex::BoxSizing::BorderBox);
    check(content->box_sizing() == flex::BoxSizing::ContentBox);
  }
}

spec("StyleEngine applies box-sizing to explicit dimensions") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* border = box.create("div", "border");
    auto* content = box.create("div", "content");
    root->append(border);
    root->append(content);
    box.set_root(root);
    box.set_viewport(360.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 10px;
        width: 320px;
        height: 200px;
      }
      #border, #content {
        width: 100px;
        height: 50px;
        padding: 10px 20px;
        border-width: 5px;
      }
      #border {
        box-sizing: border-box;
      }
      #content {
        box-sizing: content-box;
      }
    )");
    box.update();
  
    check(approx_eq(border->layout_width(), 100.0f, 0.1f));
    check(approx_eq(border->layout_height(), 50.0f, 0.1f));
    check(approx_eq(content->layout_width(), 150.0f, 0.1f));
    check(approx_eq(content->layout_height(), 80.0f, 0.1f));
  }
}

spec("StyleEngine applies flex row and gap layout from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: row;
        gap: 10px;
        width: 200px;
        height: 60px;
      }
      #a { width: 30px; height: 20px; }
      #b { width: 40px; height: 20px; }
      #c { width: 50px; height: 20px; }
    )");
    box.update();
  
    check(approx_eq(a->x(), 0.0f, 0.0));
    check(approx_eq(b->x(), 40.0f, 0.0));
    check(approx_eq(c->x(), 90.0f, 0.0));
    check(approx_eq(a->y(), 0.0f, 0.0));
    check(approx_eq(b->y(), 0.0f, 0.0));
    check(approx_eq(c->y(), 0.0f, 0.0));
    check(root->style_.display == Display::Flex);
    check(approx_eq(root->style_.gap, 10.0f, 0.0));
  }
}

spec("StyleEngine applies flex column layout from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        gap: 5px;
        width: 120px;
        height: 120px;
      }
      #a { width: 20px; height: 20px; }
      #b { width: 30px; height: 30px; }
      #c { width: 40px; height: 40px; }
    )");
    box.update();
  
    check(approx_eq(a->x(), 0.0f, 0.0));
    check(approx_eq(b->x(), 0.0f, 0.0));
    check(approx_eq(c->x(), 0.0f, 0.0));
    check(approx_eq(a->y(), 0.0f, 0.0));
    check(approx_eq(b->y(), 25.0f, 0.0));
    check(approx_eq(c->y(), 60.0f, 0.0));
  }
}

spec("StyleEngine applies flex-wrap from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: row;
        flex-wrap: wrap;
        width: 100px;
        height: 80px;
      }
      #a, #b, #c {
        width: 40px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(a->x(), 0.0f, 0.1f));
    check(approx_eq(a->y(), 0.0f, 0.1f));
    check(approx_eq(b->x(), 40.0f, 0.1f));
    check(approx_eq(b->y(), 0.0f, 0.1f));
    check(approx_eq(c->x(), 0.0f, 0.1f));
    check(approx_eq(c->y(), 20.0f, 0.1f));
  }
}

spec("LayoutManager grows auto height for wrapped flex rows") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    auto* after = box.create("div", "after");
    root->append(a);
    root->append(b);
    root->append(c);
    root->append(after);
    box.set_root(root);
    box.set_viewport(640.0f, 260.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-wrap: wrap;
        gap: 10px;
        width: 220px;
      }
      #a, #b, #c {
        width: 100px;
        height: 30px;
      }
      #after {
        width: 100px;
        height: 20px;
      }
    )");
    box.update();

    check(approx_eq(a->y(), 0.0f, 0.1f));
    check(approx_eq(b->y(), 0.0f, 0.1f));
    check(approx_eq(c->y(), 40.0f, 0.1f));
    check(approx_eq(after->y(), 40.0f, 0.1f));
    check(approx_eq(root->layout_height(), 70.0f, 0.1f));
  }
}

spec("LayoutManager reflows nested percentages after flex allocation") {
  it("uses the parent's final flex item width for descendants") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* rail = box.create("div", "rail");
    auto* workspace = box.create("div", "workspace");
    auto* panel = box.create("div", "panel");
    auto* grid = box.create("div", "grid");
    auto* first = box.create("div", "first");
    auto* second = box.create("div", "second");
    auto* host = box.create("div", "host");

    root->append(rail);
    root->append(workspace);
    workspace->append(panel);
    panel->append(grid);
    grid->append(first);
    grid->append(second);
    first->append(host);
    box.set_root(root);
    box.set_viewport(800.0f, 400.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: row;
        align-items: stretch;
        width: 800px;
        height: 400px;
        gap: 20px;
      }
      #rail { width: 200px; }
      #workspace {
        display: flex;
        flex-direction: column;
        flex: 1;
        min-width: 100px;
        min-height: 0;
      }
      #panel, #grid, #host { width: 100%; }
      #grid {
        display: flex;
        flex-direction: row;
        flex-wrap: wrap;
        gap: 16px;
      }
      #first, #second {
        display: flex;
        flex-direction: column;
        flex: 1 1 260px;
        min-width: 0;
      }
      #host { min-height: 52px; }
    )");
    box.update();

    check(approx_eq(workspace->layout_width(), 580.0f, 0.1f));
    check(approx_eq(panel->layout_width(), 580.0f, 0.1f));
    check(approx_eq(grid->layout_width(), 580.0f, 0.1f));
    check(approx_eq(first->layout_width(), 282.0f, 0.1f));
    check(approx_eq(second->layout_width(), 282.0f, 0.1f));
    check(approx_eq(second->x(), 298.0f, 0.1f));
    check(approx_eq(host->layout_width(), 282.0f, 0.1f));
  }
}

spec("LayoutManager applies intrinsic size for auto-sized widget leaves") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create_widget<ButtonWidget>("button", "button", "Save");
    root->append(button);
    box.set_root(root);
    box.set_viewport(640.0f, 240.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        width: 320px;
      }
      #button {
        font-size: 14px;
      }
    )");
    box.update();

    check(button->layout_width() > 60.0f);
    check(button->layout_width() < 180.0f);
    check(button->layout_height() >= 30.0f);
  }
}

spec("StyleEngine applies row-gap and column-gap to flex main axis") {
  it("runs") {
    Box box(nullptr);
  
    auto* row = box.create("div", "row");
    auto* row_a = box.create("div", "row-a");
    auto* row_b = box.create("div", "row-b");
    row->append(row_a);
    row->append(row_b);
  
    auto* column = box.create("div", "column");
    auto* col_a = box.create("div", "col-a");
    auto* col_b = box.create("div", "col-b");
    column->append(col_a);
    column->append(col_b);
  
    auto* root = box.create("div", "root");
    root->append(row);
    root->append(column);
    box.set_root(root);
    box.set_viewport(400.0f, 220.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        row-gap: 20px;
        width: 400px;
        height: 220px;
      }
      #row {
        display: flex;
        flex-direction: row;
        column-gap: 12px;
        width: 200px;
        height: 40px;
      }
      #column {
        display: flex;
        flex-direction: column;
        row-gap: 7px;
        width: 120px;
        height: 80px;
      }
      #row-a, #row-b, #col-a, #col-b {
        width: 30px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(row_b->x(), 42.0f, 0.1f));
    check(approx_eq(col_b->y(), 27.0f, 0.1f));
    check(approx_eq(column->y(), 60.0f, 0.1f));
  }
}

spec("StyleEngine applies basic grid columns and auto placement from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(360.0f, 200.0f);
  
    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: 10px;
        width: 210px;
        height: 120px;
      }
      #a, #b, #c {
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(a->x(), 0.0f, 0.1f));
    check(approx_eq(a->y(), 0.0f, 0.1f));
    check(approx_eq(a->layout_width(), 100.0f, 0.1f));
  
    check(approx_eq(b->x(), 110.0f, 0.1f));
    check(approx_eq(b->y(), 0.0f, 0.1f));
    check(approx_eq(b->layout_width(), 100.0f, 0.1f));
  
    check(approx_eq(c->x(), 0.0f, 0.1f));
    check(approx_eq(c->y(), 30.0f, 0.1f));
    check(approx_eq(c->layout_width(), 100.0f, 0.1f));
  }
}

spec("StyleEngine applies grid column spans from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* lead = box.create("div", "lead");
    auto* side = box.create("div", "side");
    auto* body = box.create("div", "body");
    root->append(lead);
    root->append(side);
    root->append(body);
    box.set_root(root);
    box.set_viewport(400.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: 100px 100px 100px;
        column-gap: 10px;
        row-gap: 8px;
        width: 320px;
        height: 160px;
      }
      #lead {
        grid-column: span 2;
        height: 24px;
      }
      #side, #body {
        height: 24px;
      }
    )");
    box.update();
  
    check(approx_eq(lead->x(), 0.0f, 0.1f));
    check(approx_eq(lead->y(), 0.0f, 0.1f));
    check(approx_eq(lead->layout_width(), 210.0f, 0.1f));
  
    check(approx_eq(side->x(), 220.0f, 0.1f));
    check(approx_eq(side->y(), 0.0f, 0.1f));
    check(approx_eq(side->layout_width(), 100.0f, 0.1f));
  
    check(approx_eq(body->x(), 0.0f, 0.1f));
    check(approx_eq(body->y(), 32.0f, 0.1f));
  }
}

spec("StyleEngine applies grid rows and explicit cell placement from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* top = box.create("div", "top");
    auto* middle = box.create("div", "middle");
    auto* pinned = box.create("div", "pinned");
    root->append(top);
    root->append(middle);
    root->append(pinned);
    box.set_root(root);
    box.set_viewport(400.0f, 260.0f);
  
    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: repeat(2, 100px);
        grid-template-rows: 30px 40px;
        row-gap: 10px;
        column-gap: 12px;
        width: 212px;
        height: 120px;
      }
      #top {
        height: 20px;
      }
      #middle {
        grid-row: 2;
        height: 20px;
      }
      #pinned {
        grid-column: 2;
        grid-row: 2;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(top->x(), 0.0f, 0.1f));
    check(approx_eq(top->y(), 0.0f, 0.1f));
    check(approx_eq(top->layout_width(), 100.0f, 0.1f));
  
    check(approx_eq(middle->x(), 0.0f, 0.1f));
    check(approx_eq(middle->y(), 40.0f, 0.1f));
  
    check(approx_eq(pinned->x(), 112.0f, 0.1f));
    check(approx_eq(pinned->y(), 40.0f, 0.1f));
  }
}

spec("StyleEngine applies grid auto flow columns and implicit tracks from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(400.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: grid;
        grid-template-rows: 20px 20px;
        grid-auto-flow: column;
        grid-auto-columns: 60px;
        row-gap: 5px;
        column-gap: 10px;
        width: 200px;
        height: 80px;
      }
      #a, #b, #c {
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(a->x(), 0.0f, 0.1f));
    check(approx_eq(a->y(), 0.0f, 0.1f));
    check(approx_eq(a->layout_width(), 60.0f, 0.1f));
  
    check(approx_eq(b->x(), 0.0f, 0.1f));
    check(approx_eq(b->y(), 25.0f, 0.1f));
    check(approx_eq(b->layout_width(), 60.0f, 0.1f));
  
    check(approx_eq(c->x(), 70.0f, 0.1f));
    check(approx_eq(c->y(), 0.0f, 0.1f));
    check(approx_eq(c->layout_width(), 60.0f, 0.1f));
  }
}

spec("StyleEngine applies grid auto rows from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* first = box.create("div", "first");
    auto* second = box.create("div", "second");
    root->append(first);
    root->append(second);
    box.set_root(root);
    box.set_viewport(320.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: 100px;
        grid-auto-rows: 30px;
        row-gap: 5px;
        width: 100px;
        height: 100px;
      }
      #first, #second {
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(first->y(), 0.0f, 0.1f));
    check(approx_eq(second->y(), 35.0f, 0.1f));
  }
}

spec("StyleEngine maps inline display keywords to existing layout modes") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* inline_block = box.create("div", "inline-block");
    auto* inline_flex = box.create("div", "inline-flex");
    auto* inline_grid = box.create("div", "inline-grid");
    root->append(inline_block);
    root->append(inline_flex);
    root->append(inline_grid);
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);

    box.load_css(R"(
      #inline-block { display: inline-block; }
      #inline-flex { display: inline-flex; }
      #inline-grid { display: inline-grid; }
    )");
    box.update();

    check(inline_block->style_.display == Display::Block);
    check(inline_flex->style_.display == Display::Flex);
    check(inline_grid->style_.display == Display::Grid);
  }
}

spec("StyleEngine applies grid auto-fit tracks from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* first = box.create("div", "first");
    auto* second = box.create("div", "second");
    root->append(first);
    root->append(second);
    box.set_root(root);
    box.set_viewport(480.0f, 240.0f);

    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
        column-gap: 10px;
        row-gap: 8px;
        width: 400px;
        height: 120px;
      }
      #first, #second {
        height: 20px;
      }
    )");
    box.update();

    check(approx_eq(first->x(), 0.0f, 0.1f));
    check(approx_eq(first->layout_width(), 195.0f, 0.1f));
    check(approx_eq(second->x(), 205.0f, 0.1f));
    check(approx_eq(second->layout_width(), 195.0f, 0.1f));
  }
}

spec("StyleEngine applies grid auto-fill tracks from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* first = box.create("div", "first");
    auto* second = box.create("div", "second");
    root->append(first);
    root->append(second);
    box.set_root(root);
    box.set_viewport(480.0f, 240.0f);

    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
        column-gap: 10px;
        row-gap: 8px;
        width: 400px;
        height: 120px;
      }
      #first, #second {
        height: 20px;
      }
    )");
    box.update();

    check(approx_eq(first->x(), 0.0f, 0.1f));
    check(approx_eq(first->layout_width(), 126.67f, 0.2f));
    check(approx_eq(second->x(), 136.67f, 0.2f));
    check(approx_eq(second->layout_width(), 126.67f, 0.2f));
  }
}

spec("StyleEngine applies named grid template areas from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* header = box.create("div", "header");
    auto* sidebar = box.create("div", "sidebar");
    auto* content = box.create("div", "content");
    root->append(header);
    root->append(sidebar);
    root->append(content);
    box.set_root(root);
    box.set_viewport(520.0f, 280.0f);

    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: 120px 1fr;
        grid-template-rows: 40px 60px;
        grid-template-areas:
          "header header"
          "sidebar content";
        column-gap: 10px;
        row-gap: 8px;
        width: 330px;
        height: 120px;
      }
      #header {
        grid-area: header;
        height: 20px;
      }
      #sidebar {
        grid-area: sidebar;
        height: 30px;
      }
      #content {
        grid-area: content;
        height: 30px;
      }
    )");
    box.update();

    check(approx_eq(header->x(), 0.0f, 0.1f));
    check(approx_eq(header->y(), 0.0f, 0.1f));
    check(approx_eq(header->layout_width(), 330.0f, 0.1f));

    check(approx_eq(sidebar->x(), 0.0f, 0.1f));
    check(approx_eq(sidebar->y(), 48.0f, 0.1f));
    check(approx_eq(sidebar->layout_width(), 120.0f, 0.1f));

    check(approx_eq(content->x(), 130.0f, 0.1f));
    check(approx_eq(content->y(), 48.0f, 0.1f));
    check(approx_eq(content->layout_width(), 200.0f, 0.1f));
  }
}

spec("StyleEngine applies grid-area shorthand placement from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(520.0f, 280.0f);

    box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: 100px 100px 100px;
        grid-template-rows: 30px 40px;
        column-gap: 10px;
        row-gap: 8px;
        width: 320px;
        height: 120px;
      }
      #card {
        grid-area: 1 / 2 / 3 / 4;
      }
    )");
    box.update();

    check(approx_eq(card->x(), 110.0f, 0.1f));
    check(approx_eq(card->y(), 0.0f, 0.1f));
    check(approx_eq(card->layout_width(), 210.0f, 0.1f));
    check(approx_eq(card->layout_height(), 78.0f, 0.1f));
  }
}

spec("StyleEngine distinguishes sparse and dense grid auto flow") {
  it("runs") {
    Box sparse_box(nullptr);
    auto* sparse_root = sparse_box.create("div", "root");
    auto* sparse_a = sparse_box.create("div", "a");
    auto* sparse_b = sparse_box.create("div", "b");
    auto* sparse_c = sparse_box.create("div", "c");
    sparse_root->append(sparse_a);
    sparse_root->append(sparse_b);
    sparse_root->append(sparse_c);
    sparse_box.set_root(sparse_root);
    sparse_box.set_viewport(420.0f, 180.0f);

    sparse_box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: 100px 100px 100px;
        grid-auto-rows: 20px;
        grid-auto-flow: row;
        width: 300px;
        height: 80px;
      }
      #a, #b {
        grid-column: span 2;
        height: 20px;
      }
      #c {
        height: 20px;
      }
    )");
    sparse_box.update();

    check(approx_eq(sparse_a->x(), 0.0f, 0.1f));
    check(approx_eq(sparse_a->y(), 0.0f, 0.1f));
    check(approx_eq(sparse_b->x(), 0.0f, 0.1f));
    check(approx_eq(sparse_b->y(), 20.0f, 0.1f));
    check(approx_eq(sparse_c->x(), 200.0f, 0.1f));
    check(approx_eq(sparse_c->y(), 20.0f, 0.1f));

    Box dense_box(nullptr);
    auto* dense_root = dense_box.create("div", "root");
    auto* dense_a = dense_box.create("div", "a");
    auto* dense_b = dense_box.create("div", "b");
    auto* dense_c = dense_box.create("div", "c");
    dense_root->append(dense_a);
    dense_root->append(dense_b);
    dense_root->append(dense_c);
    dense_box.set_root(dense_root);
    dense_box.set_viewport(420.0f, 180.0f);

    dense_box.load_css(R"(
      #root {
        display: grid;
        grid-template-columns: 100px 100px 100px;
        grid-auto-rows: 20px;
        grid-auto-flow: row dense;
        width: 300px;
        height: 80px;
      }
      #a, #b {
        grid-column: span 2;
        height: 20px;
      }
      #c {
        height: 20px;
      }
    )");
    dense_box.update();

    check(approx_eq(dense_a->x(), 0.0f, 0.1f));
    check(approx_eq(dense_a->y(), 0.0f, 0.1f));
    check(approx_eq(dense_b->x(), 0.0f, 0.1f));
    check(approx_eq(dense_b->y(), 20.0f, 0.1f));
    check(approx_eq(dense_c->x(), 200.0f, 0.1f));
    check(approx_eq(dense_c->y(), 0.0f, 0.1f));
  }
}

spec("StyleEngine applies grid self alignment and place-items from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* inherited = box.create("div", "inherited");
    auto* explicit_self = box.create("div", "explicit-self");
    root->append(inherited);
    root->append(explicit_self);
    box.set_root(root);
    box.set_viewport(400.0f, 200.0f);
  
    box.load_css(R"(
      #root {
        place-items: center end;
        display: grid;
        grid-template-columns: repeat(2, 100px);
        grid-template-rows: 40px;
        column-gap: 12px;
        width: 212px;
        height: 40px;
      }
      #inherited, #explicit-self {
        width: 40px;
        height: 20px;
      }
      #explicit-self {
        place-self: end center;
      }
    )");
    box.update();
  
    check(approx_eq(inherited->x(), 60.0f, 0.1f));
    check(approx_eq(inherited->y(), 10.0f, 0.1f));
    check(approx_eq(explicit_self->x(), 142.0f, 0.1f));
    check(approx_eq(explicit_self->y(), 20.0f, 0.1f));
  }
}

spec("StyleEngine applies place-content to grid track distribution from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* top = box.create("div", "top");
    auto* bottom = box.create("div", "bottom");
    root->append(top);
    root->append(bottom);
    box.set_root(root);
    box.set_viewport(400.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        place-content: end center;
        display: grid;
        grid-template-columns: 80px;
        grid-template-rows: 20px 20px;
        width: 240px;
        height: 100px;
      }
      #top, #bottom {
        width: 80px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(top->x(), 80.0f, 0.1f));
    check(approx_eq(top->y(), 60.0f, 0.1f));
    check(approx_eq(bottom->x(), 80.0f, 0.1f));
    check(approx_eq(bottom->y(), 80.0f, 0.1f));
  }
}

spec("StyleEngine applies justify-content and align-items from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* centered = box.create("div", "centered");
    auto* left = box.create("div", "left");
    auto* right = box.create("div", "right");
    centered->append(left);
    centered->append(right);
  
    auto* spaced = box.create("div", "spaced");
    auto* first = box.create("div", "first");
    auto* second = box.create("div", "second");
    auto* third = box.create("div", "third");
    spaced->append(first);
    spaced->append(second);
    spaced->append(third);
  
    auto* root = box.create("div", "root");
    root->append(centered);
    root->append(spaced);
    box.set_root(root);
    box.set_viewport(640.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 20px;
        width: 640px;
        height: 240px;
      }
      #centered {
        display: flex;
        justify-content: center;
        align-items: center;
        width: 300px;
        height: 100px;
      }
      #left, #right {
        width: 50px;
      }
      #left { height: 20px; }
      #right { height: 40px; }
  
      #spaced {
        display: flex;
        justify-content: space-between;
        align-items: end;
        width: 300px;
        height: 100px;
      }
      #first, #second, #third {
        width: 50px;
      }
      #first { height: 20px; }
      #second { height: 40px; }
      #third { height: 60px; }
    )");
    box.update();
  
    check(approx_eq(left->x(), 100.0f, 0.1f));
    check(approx_eq(right->x(), 150.0f, 0.1f));
    check(approx_eq(left->y(), 40.0f, 0.1f));
    check(approx_eq(right->y(), 30.0f, 0.1f));
  
    check(approx_eq(first->x(), 0.0f, 0.1f));
    check(approx_eq(second->x(), 125.0f, 0.1f));
    check(approx_eq(third->x(), 250.0f, 0.1f));
    check(approx_eq(first->y(), 80.0f, 0.1f));
    check(approx_eq(second->y(), 60.0f, 0.1f));
    check(approx_eq(third->y(), 40.0f, 0.1f));
  }
}

spec("StyleEngine accepts flex-start aliases for flex alignment") {
  it("runs") {
    Box box(nullptr);
    auto* row = box.create("div", "row");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    row->append(a);
    row->append(b);
    box.set_root(row);
    box.set_viewport(320.0f, 120.0f);
  
    box.load_css(R"(
      #row {
        display: flex;
        justify-content: flex-start;
        align-items: flex-start;
        width: 300px;
        height: 100px;
      }
      #a {
        width: 60px;
        height: 20px;
      }
      #b {
        width: 40px;
        height: 30px;
      }
    )");
    box.update();
  
    check(row->style_.justify_content == JustifyContent::Start);
    check(row->style_.align_items == AlignItems::Start);
    check(approx_eq(a->x(), 0.0f, 0.1f));
    check(approx_eq(b->x(), 60.0f, 0.1f));
    check(approx_eq(a->y(), 0.0f, 0.1f));
    check(approx_eq(b->y(), 0.0f, 0.1f));
  }
}

spec("StyleEngine applies flex grow shrink and basis from CSS") {
  it("runs") {
    Box box(nullptr);
  
    auto* grow = box.create("div", "grow");
    auto* grow_a = box.create("div", "grow-a");
    auto* grow_b = box.create("div", "grow-b");
    grow->append(grow_a);
    grow->append(grow_b);
  
    auto* shrink = box.create("div", "shrink");
    auto* shrink_a = box.create("div", "shrink-a");
    auto* shrink_b = box.create("div", "shrink-b");
    auto* shrink_c = box.create("div", "shrink-c");
    shrink->append(shrink_a);
    shrink->append(shrink_b);
    shrink->append(shrink_c);
  
    auto* basis = box.create("div", "basis");
    auto* basis_a = box.create("div", "basis-a");
    auto* basis_b = box.create("div", "basis-b");
    basis->append(basis_a);
    basis->append(basis_b);
  
    auto* root = box.create("div", "root");
    root->append(grow);
    root->append(shrink);
    root->append(basis);
    box.set_root(root);
    box.set_viewport(640.0f, 320.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 20px;
        width: 640px;
        height: 320px;
      }
  
      #grow, #shrink, #basis {
        display: flex;
        flex-direction: row;
        height: 60px;
      }
  
      #grow { width: 300px; }
      #grow-a { width: 50px; height: 20px; flex-grow: 1; }
      #grow-b { width: 50px; height: 20px; flex-grow: 2; }
  
      #shrink { width: 100px; }
      #shrink-a, #shrink-b, #shrink-c {
        width: 50px;
        height: 20px;
        flex-shrink: 1;
      }
  
      #basis { width: 300px; }
      #basis-a { width: 50px; height: 20px; flex-basis: 100px; }
      #basis-b { width: 50px; height: 20px; }
    )");
    box.update();
  
    check(approx_eq(grow_a->layout_width(), 116.666f, 1.0f));
    check(approx_eq(grow_b->layout_width(), 183.333f, 1.0f));
    check(approx_eq(grow_b->x(), 116.666f, 1.0f));
  
    check(approx_eq(shrink_a->layout_width(), 33.333f, 1.0f));
    check(approx_eq(shrink_b->layout_width(), 33.333f, 1.0f));
    check(approx_eq(shrink_c->layout_width(), 33.333f, 1.0f));
    check(approx_eq(shrink_b->x(), 33.333f, 1.0f));
    check(approx_eq(shrink_c->x(), 66.666f, 1.0f));
  
    check(approx_eq(basis_a->layout_width(), 100.0f, 0.1f));
    check(approx_eq(basis_b->x(), 100.0f, 0.1f));
  }
}

spec("StyleEngine applies space-around and space-evenly from CSS") {
  it("runs") {
    Box box(nullptr);
  
    auto* around = box.create("div", "around");
    auto* around_a = box.create("div", "around-a");
    auto* around_b = box.create("div", "around-b");
    around->append(around_a);
    around->append(around_b);
  
    auto* evenly = box.create("div", "evenly");
    auto* evenly_a = box.create("div", "evenly-a");
    auto* evenly_b = box.create("div", "evenly-b");
    evenly->append(evenly_a);
    evenly->append(evenly_b);
  
    auto* root = box.create("div", "root");
    root->append(around);
    root->append(evenly);
    box.set_root(root);
    box.set_viewport(640.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 20px;
        width: 640px;
        height: 240px;
      }
      #around, #evenly {
        display: flex;
        width: 300px;
        height: 60px;
      }
      #around { justify-content: space-around; }
      #evenly { justify-content: space-evenly; }
      #around-a, #around-b, #evenly-a, #evenly-b {
        width: 50px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(around_a->x(), 50.0f, 0.1f));
    check(approx_eq(around_b->x(), 200.0f, 0.1f));
    check(approx_eq(evenly_a->x(), 66.666f, 1.0f));
    check(approx_eq(evenly_b->x(), 183.333f, 1.0f));
  }
}

spec("StyleEngine applies flex-end alignment from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    root->append(a);
    root->append(b);
    box.set_root(root);
    box.set_viewport(400.0f, 120.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        justify-content: end;
        width: 300px;
        height: 60px;
      }
      #a, #b {
        width: 50px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(a->x(), 200.0f, 0.1f));
    check(approx_eq(b->x(), 250.0f, 0.1f));
  }
}

spec("StyleEngine accepts flex-end aliases for flex alignment") {
  it("runs") {
    Box box(nullptr);
    auto* row = box.create("div", "row");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    row->append(a);
    row->append(b);
    box.set_root(row);
    box.set_viewport(340.0f, 140.0f);
  
    box.load_css(R"(
      #row {
        display: flex;
        justify-content: flex-end;
        align-items: flex-end;
        width: 300px;
        height: 100px;
      }
      #a {
        width: 60px;
        height: 20px;
      }
      #b {
        width: 40px;
        height: 30px;
      }
    )");
    box.update();
  
    check(row->style_.justify_content == JustifyContent::End);
    check(row->style_.align_items == AlignItems::End);
    check(approx_eq(a->x(), 200.0f, 0.1f));
    check(approx_eq(b->x(), 260.0f, 0.1f));
    check(approx_eq(a->y(), 80.0f, 0.1f));
    check(approx_eq(b->y(), 70.0f, 0.1f));
  }
}

spec("StyleEngine applies align-self overrides from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* row = box.create("div", "row");
    auto* auto_item = box.create("div", "auto");
    auto* end_item = box.create("div", "end");
    auto* center_item = box.create("div", "center");
    row->append(auto_item);
    row->append(end_item);
    row->append(center_item);
    box.set_root(row);
    box.set_viewport(360.0f, 140.0f);
  
    box.load_css(R"(
      #row {
        display: flex;
        align-items: flex-start;
        width: 320px;
        height: 100px;
      }
      #auto, #end, #center {
        width: 40px;
        height: 20px;
      }
      #end {
        align-self: flex-end;
      }
      #center {
        align-self: center;
      }
    )");
    box.update();
  
    check(auto_item->align_self() == AlignSelf::Auto);
    check(end_item->align_self() == AlignSelf::End);
    check(center_item->align_self() == AlignSelf::Center);
    check(approx_eq(auto_item->y(), 0.0f, 0.1f));
    check(approx_eq(end_item->y(), 80.0f, 0.1f));
    check(approx_eq(center_item->y(), 40.0f, 0.1f));
  }
}

spec("StyleEngine applies align-self stretch from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* row = box.create("div", "row");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    row->append(a);
    row->append(b);
    box.set_root(row);
    box.set_viewport(360.0f, 140.0f);
  
    box.load_css(R"(
      #row {
        display: flex;
        align-items: flex-start;
        width: 320px;
        height: 100px;
      }
      #a {
        width: 40px;
        height: 20px;
      }
      #b {
        width: 40px;
        height: 20px;
        align-self: stretch;
      }
    )");
    box.update();
  
    check(b->align_self() == AlignSelf::Stretch);
    check(approx_eq(a->layout_height(), 20.0f, 0.1f));
    check(approx_eq(b->layout_height(), 100.0f, 0.1f));
  }
}

spec("StyleEngine applies reverse flex directions from CSS") {
  it("runs") {
    Box box(nullptr);
  
    auto* row = box.create("div", "row");
    auto* row_a = box.create("div", "row-a");
    auto* row_b = box.create("div", "row-b");
    auto* row_c = box.create("div", "row-c");
    row->append(row_a);
    row->append(row_b);
    row->append(row_c);
  
    auto* column = box.create("div", "column");
    auto* column_a = box.create("div", "column-a");
    auto* column_b = box.create("div", "column-b");
    auto* column_c = box.create("div", "column-c");
    column->append(column_a);
    column->append(column_b);
    column->append(column_c);
  
    auto* root = box.create("div", "root");
    root->append(row);
    root->append(column);
    box.set_root(root);
    box.set_viewport(400.0f, 220.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 20px;
        width: 400px;
        height: 220px;
      }
      #row {
        display: flex;
        flex-direction: row-reverse;
        gap: 10px;
        width: 200px;
        height: 60px;
      }
      #column {
        display: flex;
        flex-direction: column-reverse;
        gap: 5px;
        width: 120px;
        height: 120px;
      }
      #row-a, #row-b, #row-c {
        width: 30px;
        height: 20px;
      }
      #column-a, #column-b, #column-c {
        width: 20px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(row_a->x(), 170.0f, 0.1f));
    check(approx_eq(row_b->x(), 130.0f, 0.1f));
    check(approx_eq(row_c->x(), 90.0f, 0.1f));
  
    check(approx_eq(column_a->y(), 100.0f, 0.1f));
    check(approx_eq(column_b->y(), 75.0f, 0.1f));
    check(approx_eq(column_c->y(), 50.0f, 0.1f));
  }
}

spec("StyleEngine applies rtl direction to flex row semantics") {
  it("runs") {
    Box box(nullptr);

    auto* rtl_row = box.create("div", "rtl-row");
    auto* rtl_a = box.create("div", "rtl-a");
    auto* rtl_b = box.create("div", "rtl-b");
    auto* rtl_c = box.create("div", "rtl-c");
    rtl_row->append(rtl_a);
    rtl_row->append(rtl_b);
    rtl_row->append(rtl_c);

    auto* rtl_row_reverse = box.create("div", "rtl-row-reverse");
    auto* rev_a = box.create("div", "rev-a");
    auto* rev_b = box.create("div", "rev-b");
    auto* rev_c = box.create("div", "rev-c");
    rtl_row_reverse->append(rev_a);
    rtl_row_reverse->append(rev_b);
    rtl_row_reverse->append(rev_c);

    auto* root = box.create("div", "root");
    root->append(rtl_row);
    root->append(rtl_row_reverse);
    box.set_root(root);
    box.set_viewport(400.0f, 180.0f);

    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 20px;
        width: 400px;
        height: 180px;
      }
      #rtl-row, #rtl-row-reverse {
        display: flex;
        direction: rtl;
        gap: 10px;
        width: 200px;
        height: 40px;
      }
      #rtl-row {
        flex-direction: row;
      }
      #rtl-row-reverse {
        flex-direction: row-reverse;
      }
      #rtl-a, #rtl-b, #rtl-c, #rev-a, #rev-b, #rev-c {
        width: 30px;
        height: 20px;
      }
    )");
    box.update();

    check(approx_eq(rtl_a->x(), 170.0f, 0.1f));
    check(approx_eq(rtl_b->x(), 130.0f, 0.1f));
    check(approx_eq(rtl_c->x(), 90.0f, 0.1f));

    check(approx_eq(rev_a->x(), 0.0f, 0.1f));
    check(approx_eq(rev_b->x(), 40.0f, 0.1f));
    check(approx_eq(rev_c->x(), 80.0f, 0.1f));
  }
}

spec("StyleEngine preserves margins in reverse flex layout") {
  it("runs") {
    Box box(nullptr);
    auto* row = box.create("div", "row");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    row->append(a);
    row->append(b);
    box.set_root(row);
    box.set_viewport(260.0f, 120.0f);
  
    box.load_css(R"(
      #row {
        display: flex;
        flex-direction: row-reverse;
        width: 200px;
        height: 60px;
      }
      #a {
        width: 30px;
        height: 20px;
        margin-left: 10px;
        margin-right: 5px;
      }
      #b {
        width: 20px;
        height: 20px;
        margin-left: 4px;
      }
    )");
    box.update();
  
    check(approx_eq(a->x(), 160.0f, 0.1f));
    check(approx_eq(b->x(), 131.0f, 0.1f));
  }
}

spec("StyleEngine applies stretch alignment from CSS") {
  it("runs") {
    Box box(nullptr);
  
    auto* row = box.create("div", "row");
    auto* row_a = box.create("div", "row-a");
    auto* row_b = box.create("div", "row-b");
    row->append(row_a);
    row->append(row_b);
  
    auto* column = box.create("div", "column");
    auto* column_a = box.create("div", "column-a");
    auto* column_b = box.create("div", "column-b");
    column->append(column_a);
    column->append(column_b);
  
    auto* root = box.create("div", "root");
    root->append(row);
    root->append(column);
    box.set_root(root);
    box.set_viewport(640.0f, 260.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 20px;
        width: 640px;
        height: 260px;
      }
      #row {
        display: flex;
        align-items: stretch;
        width: 300px;
        height: 80px;
      }
      #column {
        display: flex;
        flex-direction: column;
        align-items: stretch;
        width: 120px;
        height: 100px;
      }
      #row-a, #row-b {
        width: 50px;
        height: 10px;
        align-self: stretch;
      }
      #column-a, #column-b {
        width: 20px;
        height: 20px;
        align-self: stretch;
      }
    )");
    box.update();
  
    check(approx_eq(row_a->layout_height(), 80.0f, 0.1f));
    check(approx_eq(row_b->layout_height(), 80.0f, 0.1f));
    check(approx_eq(column_a->layout_width(), 120.0f, 0.1f));
    check(approx_eq(column_b->layout_width(), 120.0f, 0.1f));
  }
}

spec("StyleEngine applies padding and margin offsets in flex layout") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    root->append(a);
    root->append(b);
    box.set_root(root);
    box.set_viewport(400.0f, 160.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        align-items: start;
        padding: 10px 20px 30px 40px;
        width: 300px;
        height: 120px;
      }
      #a {
        width: 50px;
        height: 20px;
        margin: 5px 6px 7px 8px;
      }
      #b {
        width: 60px;
        height: 20px;
        margin-left: 4px;
        margin-top: 3px;
      }
    )");
    box.update();
  
    check(approx_eq(a->x(), 48.0f, 0.1f));
    check(approx_eq(a->y(), 15.0f, 0.1f));
    check(approx_eq(b->x(), 108.0f, 0.1f));
    check(approx_eq(b->y(), 13.0f, 0.1f));
  }
}

spec("StyleEngine composes padding and gap in flex layout") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(360.0f, 160.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        align-items: start;
        gap: 15px;
        padding: 10px 20px 30px 40px;
        width: 320px;
        height: 120px;
      }
      #a, #b, #c {
        width: 50px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(root->style_.gap, 15.0f, 0.0));
    check(approx_eq(a->x(), 40.0f, 0.1f));
    check(approx_eq(b->x(), 105.0f, 0.1f));
    check(approx_eq(c->x(), 170.0f, 0.1f));
    check(approx_eq(a->y(), 10.0f, 0.1f));
    check(approx_eq(b->y(), 10.0f, 0.1f));
    check(approx_eq(c->y(), 10.0f, 0.1f));
  }
}

spec("StyleEngine composes column padding and gap in flex layout") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* a = box.create("div", "a");
    auto* b = box.create("div", "b");
    auto* c = box.create("div", "c");
    root->append(a);
    root->append(b);
    root->append(c);
    box.set_root(root);
    box.set_viewport(240.0f, 240.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: column;
        align-items: start;
        gap: 12px;
        padding: 10px 20px 30px 40px;
        width: 200px;
        height: 200px;
      }
      #a, #b, #c {
        width: 50px;
        height: 20px;
      }
    )");
    box.update();
  
    check(approx_eq(root->style_.gap, 12.0f, 0.0));
    check(approx_eq(a->x(), 40.0f, 0.1f));
    check(approx_eq(b->x(), 40.0f, 0.1f));
    check(approx_eq(c->x(), 40.0f, 0.1f));
    check(approx_eq(a->y(), 10.0f, 0.1f));
    check(approx_eq(b->y(), 42.0f, 0.1f));
    check(approx_eq(c->y(), 74.0f, 0.1f));
  }
}

spec("StyleEngine applies supported flex shorthand forms from CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* none = box.create("div", "none");
    auto* one = box.create("div", "one");
    auto* two = box.create("div", "two");
    auto* auto_item = box.create("div", "auto_item");
    auto* basis_item = box.create("div", "basis_item");
    root->append(none);
    root->append(one);
    root->append(two);
    root->append(auto_item);
    root->append(basis_item);
    box.set_root(root);
    box.set_viewport(800.0f, 120.0f);
  
    box.load_css(R"(
      #root {
        display: flex;
        flex-direction: row;
        width: 700px;
        height: 60px;
      }
      #none {
        width: 50px;
        height: 20px;
        flex: none;
      }
      #one {
        height: 20px;
        flex: 1;
      }
      #two {
        height: 20px;
        flex: 2 0 0;
      }
      #auto_item {
        width: 50px;
        height: 20px;
        flex: auto;
      }
      #basis_item {
        height: 20px;
        flex: 1 1 50px;
      }
    )");
    box.update();
  
    check(approx_eq(none->style_.get_variable_float(Symbol("flex-grow"), -1.0f), 0.0f, 0.0));
    check(approx_eq(none->style_.get_variable_float(Symbol("flex-shrink"), -1.0f), 0.0f, 0.0));
    check(approx_eq(none->layout_width(), 50.0f, 0.1f));
  
    check(approx_eq(one->style_.get_variable_float(Symbol("flex-grow"), -1.0f), 1.0f, 0.0));
    check(approx_eq(one->style_.get_variable_float(Symbol("flex-basis"), -1.0f), 0.0f, 0.0));
    check_false(one->flex_basis_auto());
    check(approx_eq(one->layout_width(), 110.0f, 1.0f));
  
    check(approx_eq(two->style_.get_variable_float(Symbol("flex-grow"), -1.0f), 2.0f, 0.0));
    check(approx_eq(two->style_.get_variable_float(Symbol("flex-shrink"), -1.0f), 0.0f, 0.0));
    check(approx_eq(two->style_.get_variable_float(Symbol("flex-basis"), -1.0f), 0.0f, 0.0));
    check(approx_eq(two->layout_width(), 220.0f, 1.0f));
  
    check(approx_eq(auto_item->style_.get_variable_float(Symbol("flex-grow"), -1.0f), 1.0f, 0.0));
    check(approx_eq(auto_item->style_.get_variable_float(Symbol("flex-shrink"), -1.0f), 1.0f, 0.0));
    check(approx_eq(auto_item->layout_width(), 160.0f, 1.0f));
  
    check(approx_eq(basis_item->style_.get_variable_float(Symbol("flex-grow"), -1.0f), 1.0f, 0.0));
    check(approx_eq(basis_item->style_.get_variable_float(Symbol("flex-shrink"), -1.0f), 1.0f, 0.0));
    check(approx_eq(basis_item->style_.get_variable_float(Symbol("flex-basis"), -1.0f), 50.0f, 0.0));
    check(approx_eq(basis_item->layout_width(), 160.0f, 1.0f));
  
    check(approx_eq(none->x(), 0.0f, 0.1f));
    check(approx_eq(one->x(), 50.0f, 1.0f));
    check(approx_eq(two->x(), 160.0f, 1.0f));
    check(approx_eq(auto_item->x(), 380.0f, 1.0f));
    check(approx_eq(basis_item->x(), 540.0f, 1.0f));
  }
}
