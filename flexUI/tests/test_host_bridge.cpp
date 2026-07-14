#include <tinytest.h>
#undef group
#include "test_support.h"

#include <flexUI/box.h>
#include <flexUI/host_bridge.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/datepicker_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/menu_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/popover_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/radio_widget.h>
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/tree_widget.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/divider_widget.h>

using namespace flexUI;

namespace {

struct HostBridgeHarness {
  Box box{nullptr};
  Element* root = nullptr;
  Element* input = nullptr;
  InputWidget* input_widget = nullptr;
  Element* label = nullptr;
  LabelWidget* label_widget = nullptr;

  HostBridgeHarness() {
    root = box.create("div", "root");
    box.set_root(root);

    input = box.create_widget<InputWidget>("input", "input");
    input_widget = static_cast<InputWidget*>(input->widget);
    root->append(input);

    label = box.create_widget<LabelWidget>("label", "label", "static");
    label_widget = static_cast<LabelWidget*>(label->widget);
    root->append(label);
  }
};

}  // namespace

spec("Host bridge gates text input to focused text widgets") {
  it("runs") {
    HostBridgeHarness harness;
  
    check_false(host::box_wants_text_input(&harness.box));
    check(host::focused_text_input_element(&harness.box) == nullptr);
    check_false(host::dispatch_text_input_if_focused(&harness.box, "x"));
    check(harness.input_widget->text().empty());
  
    harness.box.set_focus(harness.label);
    check_false(host::box_wants_text_input(&harness.box));
    check(host::focused_text_input_element(&harness.box) == nullptr);
    check_false(host::dispatch_text_input_if_focused(&harness.box, "x"));
    check(harness.input_widget->text().empty());
  
    harness.box.set_focus(harness.input);
    check(host::box_wants_text_input(&harness.box));
    check(host::focused_text_input_element(&harness.box) == harness.input);
    check_false(host::dispatch_text_input_if_focused(&harness.box, ""));
    check(host::dispatch_text_input_if_focused(&harness.box, "x"));
    check(harness.input_widget->text() == "x");

    harness.input_widget->set_readonly(true);
    check_false(host::box_wants_text_input(&harness.box));
    check(host::focused_text_input_element(&harness.box) == nullptr);
    check_false(host::dispatch_text_input_if_focused(&harness.box, "y"));
    check(harness.input_widget->text() == "x");

    harness.input_widget->set_readonly(false);
    harness.input_widget->set_disabled(true);
    check_false(host::box_wants_text_input(&harness.box));
    check(host::focused_text_input_element(&harness.box) == nullptr);
    check_false(host::dispatch_text_input_if_focused(&harness.box, "z"));
    check(harness.input_widget->text() == "x");
  }
}

spec("Host bridge ignores focused text widgets hidden by CSS") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* hidden_input = box.create_widget<InputWidget>("input", "hidden-input");
    auto* hidden_panel = box.create("div", "hidden-panel");
    auto* nested_input = box.create_widget<InputWidget>("input", "nested-input");
    auto* visible_input = box.create_widget<InputWidget>("input", "visible-input");
    hidden_panel->append(nested_input);
    root->append(hidden_input);
    root->append(hidden_panel);
    root->append(visible_input);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #hidden-input {
        display: none;
      }

      #hidden-panel {
        visibility: hidden;
      }
    )");
    box.update();

    auto* hidden_widget = static_cast<InputWidget*>(hidden_input->widget);
    auto* nested_widget = static_cast<InputWidget*>(nested_input->widget);
    auto* visible_widget = static_cast<InputWidget*>(visible_input->widget);

    box.set_focus(hidden_input);
    check_false(host::box_wants_text_input(&box));
    check(host::focused_text_input_element(&box) == nullptr);
    check_false(host::dispatch_text_input_if_focused(&box, "x"));
    check(hidden_widget->text().empty());

    box.set_focus(nested_input);
    check_false(host::box_wants_text_input(&box));
    check(host::focused_text_input_element(&box) == nullptr);
    check_false(host::dispatch_text_input_if_focused(&box, "y"));
    check(nested_widget->text().empty());

    box.set_focus(visible_input);
    check(host::box_wants_text_input(&box));
    check(host::focused_text_input_element(&box) == visible_input);
    check(host::dispatch_text_input_if_focused(&box, "z"));
    check(visible_widget->text() == "z");
  }
}

spec("Host bridge gates composition to focused text widgets") {
  it("runs") {
    HostBridgeHarness harness;
  
    check_false(host::dispatch_composition_start_if_focused(&harness.box));
    check_false(host::dispatch_composition_update_if_focused(&harness.box, "zh"));
    check_false(host::dispatch_composition_end_if_focused(&harness.box));
  
    harness.box.set_focus(harness.label);
    check_false(host::dispatch_composition_start_if_focused(&harness.box));
    check_false(host::dispatch_composition_update_if_focused(&harness.box, "zh"));
    check_false(host::dispatch_composition_end_if_focused(&harness.box));
  
    harness.box.set_focus(harness.input);
    check(host::dispatch_composition_start_if_focused(&harness.box));
    check(host::dispatch_composition_update_if_focused(&harness.box, "zh"));
    check(harness.input_widget->text().empty());
    check(host::dispatch_composition_end_if_focused(&harness.box));
    check(harness.input_widget->text().empty());
  }
}

spec("Host bridge resolves focused caret anchor through CSS transforms") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* input = box.create_widget<InputWidget>("input", "input");
    auto* input_widget = static_cast<InputWidget*>(input->widget);
    root->append(input);
    box.set_root(root);
    box.set_viewport(320.0f, 200.0f);

    box.load_css(R"(
      #input {
        width: 120px;
        height: 32px;
        transform: translateX(40px);
      }
    )");
    box.update();
    input_widget->set_text("abc");
    box.set_focus(input);

    float local_x = 0.0f;
    float local_y = 0.0f;
    float local_w = 0.0f;
    float local_h = 0.0f;
    input->widget->get_caret_rect(*input, local_x, local_y, local_w, local_h);

    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    check(host::focused_text_input_caret_anchor(&box, anchor_x, anchor_y));
    check(approx_eq(anchor_x, input->absolute_x() + local_x + 40.0f, 0.001f));
    check(approx_eq(anchor_y, input->absolute_y() + local_y + local_h, 0.001f));
  }
}

spec("Host bridge synchronizes native capture against box capture state") {
  it("runs") {
    HostBridgeHarness harness;
    int acquire_calls = 0;
    int release_calls = 0;
  
    check_false(host::sync_mouse_capture(
        &harness.box, false, [&] { ++acquire_calls; }, [&] { ++release_calls; }));
    check(acquire_calls == 0);
    check(release_calls == 0);
  
    harness.box.set_mouse_capture(harness.input);
    check(host::sync_mouse_capture(
        &harness.box, false, [&] { ++acquire_calls; }, [&] { ++release_calls; }));
    check(acquire_calls == 1);
    check(release_calls == 0);
  
    check_false(host::sync_mouse_capture(
        &harness.box, true, [&] { ++acquire_calls; }, [&] { ++release_calls; }));
    check(acquire_calls == 1);
    check(release_calls == 0);
  
    harness.box.release_mouse_capture(harness.input);
    check(host::sync_mouse_capture(
        &harness.box, true, [&] { ++acquire_calls; }, [&] { ++release_calls; }));
    check(acquire_calls == 1);
    check(release_calls == 1);
  }
}

spec("Host bridge clears focus and capture explicitly") {
  it("runs") {
    HostBridgeHarness harness;
  
    harness.box.set_focus(harness.input);
    harness.box.set_mouse_capture(harness.input);
    check(host::clear_focus_and_capture(&harness.box));
    check(harness.box.focused_element() == nullptr);
    check(harness.box.capturing_element() == nullptr);
  
    check_false(host::clear_focus_and_capture(&harness.box));
  }
}

spec("Host bridge clears partial focus and capture state") {
  it("runs") {
    HostBridgeHarness harness;
  
    harness.box.set_focus(harness.input);
    check(host::clear_focus_and_capture(&harness.box));
    check(harness.box.focused_element() == nullptr);
    check(harness.box.capturing_element() == nullptr);
  
    harness.box.set_mouse_capture(harness.input);
    check(host::clear_focus_and_capture(&harness.box));
    check(harness.box.focused_element() == nullptr);
    check(harness.box.capturing_element() == nullptr);
  }
}

spec("Focus-visible follows keyboard modality instead of pointer modality") {
  it("runs") {
    HostBridgeHarness harness;
    harness.input->set_layout_bounds(10.0f, 10.0f, 120.0f, 32.0f);
    harness.input->focusable = true;
  
    Event mouse = Event::mouse_down(20.0f, 20.0f);
    harness.box.dispatch_event(mouse);
    check(harness.box.focused_element() == harness.input);
    check(harness.input->is_focus());
    check_false(harness.input->is_focus_visible());
  
    harness.box.set_focus(nullptr);
    Event tab = Event::key_down(KeyCode::Tab);
    harness.box.dispatch_event(tab);
    harness.box.set_focus(harness.input);
    check(harness.box.focused_element() == harness.input);
    check(harness.input->is_focus());
    check(harness.input->is_focus_visible());
  }
}

spec("Host bridge propagates focus-within through the focus ancestry") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
    auto* wrapper = box.create("div", "wrapper");
    auto* input = box.create_widget<InputWidget>("input", "input");
    root->append(wrapper);
    wrapper->append(input);
    box.set_root(root);
  
    box.set_focus(input);
    check(input->has_state("focus"));
    check(input->has_state("focus-within"));
    check(wrapper->has_state("focus-within"));
    check(root->has_state("focus-within"));
  
    box.set_focus(nullptr);
    check_false(input->has_state("focus-within"));
    check_false(wrapper->has_state("focus-within"));
    check_false(root->has_state("focus-within"));
  }
}

spec("Host bridge exposes cursor and user-select policies for hovered and captured elements") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    auto* input = box.create_widget<InputWidget>("input", "input");
    root->append(card);
    root->append(input);
    box.set_root(root);
    box.set_viewport(300.0f, 200.0f);

    box.load_css(R"(
      #card {
        cursor: pointer;
        user-select: none;
      }

      #input {
        cursor: text;
        user-select: text;
      }
    )");
    box.update();
    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    card->set_layout_bounds(10.0f, 10.0f, 120.0f, 40.0f);
    input->set_layout_bounds(10.0f, 70.0f, 120.0f, 32.0f);

    Event hover_card = Event::mouse_move(20.0f, 20.0f);
    box.dispatch_event(hover_card);
    check(box.hovered_element() == card);
    check(host::requested_cursor(&box) == "pointer");
    check(host::requested_user_select(&box) == "none");
    check_false(host::should_enable_native_text_selection(&box));

    std::string host_cursor = "default";
    check(host::sync_cursor(&box, host_cursor, [&](const std::string& value) {
      host_cursor = value;
    }));
    check(host_cursor == "pointer");
    check_false(host::sync_cursor(&box, host_cursor, [&](const std::string& value) {
      host_cursor = value;
    }));

    Event hover_input = Event::mouse_move(20.0f, 80.0f);
    box.dispatch_event(hover_input);
    check(box.hovered_element() == input);
    check(host::requested_cursor(&box) == "text");
    check(host::requested_user_select(&box) == "text");
    check(host::should_enable_native_text_selection(&box));

    box.set_mouse_capture(card);
    check(host::interaction_target_element(&box) == card);
    check(host::requested_cursor(&box) == "pointer");
    check(host::requested_user_select(&box) == "none");
    box.release_mouse_capture(card);
  }
}

spec("Host bridge exposes appearance policy for hovered and captured elements") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* button = box.create("button", "button");
    auto* input = box.create_widget<InputWidget>("input", "input");
    root->append(button);
    root->append(input);
    box.set_root(root);
    box.set_viewport(300.0f, 200.0f);

    box.load_css(R"(
      #button {
        appearance: none;
      }

      #input {
        -webkit-appearance: textfield;
      }
    )");
    box.update();
    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    button->set_layout_bounds(10.0f, 10.0f, 120.0f, 40.0f);
    input->set_layout_bounds(10.0f, 70.0f, 120.0f, 32.0f);

    Event hover_button = Event::mouse_move(20.0f, 20.0f);
    box.dispatch_event(hover_button);
    check(box.hovered_element() == button);
    check(host::requested_appearance(&box) == "none");
    check_false(host::should_use_native_form_appearance(&box));

    Event hover_input = Event::mouse_move(20.0f, 80.0f);
    box.dispatch_event(hover_input);
    check(box.hovered_element() == input);
    check(host::requested_appearance(&box) == "textfield");
    check(host::should_use_native_form_appearance(&box));

    box.set_mouse_capture(button);
    check(host::interaction_target_element(&box) == button);
    check(host::requested_appearance(&box) == "none");
    check_false(host::should_use_native_form_appearance(&box));
    box.release_mouse_capture(button);
  }
}

spec("Host bridge exposes touch-action policy for hovered and captured elements") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* slider = box.create("div", "slider");
    auto* scroll = box.create("div", "scroll");
    root->append(slider);
    root->append(scroll);
    box.set_root(root);
    box.set_viewport(300.0f, 200.0f);

    box.load_css(R"(
      #slider {
        touch-action: none;
      }

      #scroll {
        touch-action: pan-y;
      }
    )");
    box.update();
    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    slider->set_layout_bounds(10.0f, 10.0f, 120.0f, 40.0f);
    scroll->set_layout_bounds(10.0f, 70.0f, 120.0f, 32.0f);

    Event hover_slider = Event::mouse_move(20.0f, 20.0f);
    box.dispatch_event(hover_slider);
    check(box.hovered_element() == slider);
    check(host::requested_touch_action(&box) == "none");
    check(host::should_disable_native_touch_actions(&box));

    Event hover_scroll = Event::mouse_move(20.0f, 80.0f);
    box.dispatch_event(hover_scroll);
    check(box.hovered_element() == scroll);
    check(host::requested_touch_action(&box) == "pan-y");
    check_false(host::should_disable_native_touch_actions(&box));

    box.set_mouse_capture(slider);
    check(host::interaction_target_element(&box) == slider);
    check(host::requested_touch_action(&box) == "none");
    check(host::should_disable_native_touch_actions(&box));
    box.release_mouse_capture(slider);
  }
}

spec("Host bridge exposes color-scheme policy for hovered and captured elements") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* dark_card = box.create("div", "dark-card");
    auto* input = box.create_widget<InputWidget>("input", "input");
    root->append(dark_card);
    root->append(input);
    box.set_root(root);
    box.set_viewport(300.0f, 200.0f);

    box.load_css(R"(
      #dark-card {
        color-scheme: dark;
      }

      #input {
        -webkit-color-scheme: light dark;
      }
    )");
    box.update();
    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    dark_card->set_layout_bounds(10.0f, 10.0f, 120.0f, 40.0f);
    input->set_layout_bounds(10.0f, 70.0f, 120.0f, 32.0f);

    Event hover_dark = Event::mouse_move(20.0f, 20.0f);
    box.dispatch_event(hover_dark);
    check(box.hovered_element() == dark_card);
    check(host::requested_color_scheme(&box) == "dark");
    check(host::should_prefer_dark_color_scheme(&box));

    Event hover_input = Event::mouse_move(20.0f, 80.0f);
    box.dispatch_event(hover_input);
    check(box.hovered_element() == input);
    check(host::requested_color_scheme(&box) == "light dark");
    check_false(host::should_prefer_dark_color_scheme(&box));

    box.set_mouse_capture(dark_card);
    check(host::interaction_target_element(&box) == dark_card);
    check(host::requested_color_scheme(&box) == "dark");
    check(host::should_prefer_dark_color_scheme(&box));
    box.release_mouse_capture(dark_card);
  }
}

spec("Host bridge exposes document color-scheme policy from root styles") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* card = box.create("div", "card");
    root->append(card);
    box.set_root(root);
    box.set_viewport(300.0f, 200.0f);

    box.load_css(R"(
      #root {
        color-scheme: light dark;
      }

      #card {
        color-scheme: dark;
      }
    )");
    box.update();
    MediaEnvironment dark_env;
    dark_env.prefers_dark_scheme = true;
    box.set_media_environment(dark_env);
    box.update();
    root->set_layout_bounds(0.0f, 0.0f, 300.0f, 200.0f);
    card->set_layout_bounds(10.0f, 10.0f, 120.0f, 40.0f);

    Event hover_card = Event::mouse_move(20.0f, 20.0f);
    box.dispatch_event(hover_card);
    check(box.hovered_element() == card);
    check(host::requested_color_scheme(&box) == "dark");
    check(host::requested_document_color_scheme(&box) == "light dark");
    check(host::should_prefer_dark_document_color_scheme(&box));

    MediaEnvironment light_env;
    light_env.prefers_dark_scheme = false;
    box.set_media_environment(light_env);
    box.update();
    check_false(host::should_prefer_dark_document_color_scheme(&box));
  }
}

spec("Widget bridge reflects state into host data and aria attributes") {
  Box box(nullptr);
  auto* root = box.create("div");

  auto* dropdown_elem = box.create_widget<DropdownWidget>("button", "dropdown");
  auto* dropdown = static_cast<DropdownWidget*>(dropdown_elem->widget);
  dropdown->add_option("Open", "open");
  dropdown->add_option("Closed", "closed");

  auto* select_elem = box.create_widget<SelectWidget>(
      "div", "select", std::vector<std::string>{"Alpha", "Beta"}, 0);
  auto* select = static_cast<SelectWidget*>(select_elem->widget);

  auto* input_elem = box.create_widget<InputWidget>("input", "text-input", "Email");
  auto* input = static_cast<InputWidget*>(input_elem->widget);

  auto* textarea_elem =
      box.create_widget<TextAreaWidget>("textarea", "textarea", "", "Describe");
  auto* textarea = static_cast<TextAreaWidget*>(textarea_elem->widget);

  auto* switch_elem =
      box.create_widget<SwitchWidget>("button", "switch", "Airplane", false);
  auto* switch_widget = static_cast<SwitchWidget*>(switch_elem->widget);

  auto* checkbox_elem =
      box.create_widget<CheckboxWidget>("button", "checkbox", "Terms", false);
  auto* checkbox = static_cast<CheckboxWidget*>(checkbox_elem->widget);

  auto* radio_elem =
      box.create_widget<RadioWidget>("button", "radio", "Large", "lg", "size", false);
  auto* radio = static_cast<RadioWidget*>(radio_elem->widget);

  auto* tabs_elem = box.create_widget<TabsWidget>("div", "tabs");
  auto* tabs = static_cast<TabsWidget*>(tabs_elem->widget);
  tabs->add_tab("General", "general");
  tabs->add_tab("Advanced", "advanced");

  auto* accordion_elem = box.create_widget<AccordionWidget>("div", "accordion");
  auto* accordion = static_cast<AccordionWidget*>(accordion_elem->widget);
  accordion->add_section("Section A", "section-a");
  accordion->add_section("Section B", "section-b");

  std::vector<ToggleGroupWidget::Option> toggle_options{
      {"left", "Left"}, {"center", "Center"}, {"right", "Right"}};
  auto* toggle_group_elem =
      box.create_widget<ToggleGroupWidget>("div", "toggle-group", toggle_options);
  auto* toggle_group = static_cast<ToggleGroupWidget*>(toggle_group_elem->widget);

  auto* modal_elem = box.create_widget<ModalWidget>("div", "modal", "Settings");
  auto* modal = static_cast<ModalWidget*>(modal_elem->widget);

  auto* popover_elem =
      box.create_widget<PopoverWidget>("button", "popover", "More details");
  auto* popover = static_cast<PopoverWidget*>(popover_elem->widget);

  auto* menu_elem = box.create_widget<MenuWidget>("div", "menu");
  auto* menu = static_cast<MenuWidget*>(menu_elem->widget);
  menu->add_item("new", "New File");

  auto* sidebar_elem = box.create_widget<SidebarWidget>("nav", "sidebar", true);
  auto* sidebar = static_cast<SidebarWidget*>(sidebar_elem->widget);
  sidebar->add_section("Workspace");
  sidebar->add_item("files", "F", "Files");
  sidebar->add_item("search", "S", "Search", "3");

  auto* searchbox_elem =
      box.create_widget<SearchBoxWidget>("input", "searchbox", "Search docs");
  auto* searchbox = static_cast<SearchBoxWidget*>(searchbox_elem->widget);
  searchbox->add_suggestion("api-ref", "API Reference");
  searchbox->add_suggestion("guide", "Guide", "Getting started");

  auto* tooltip_target = box.create("button", "tooltip-target");
  auto* tooltip_elem = box.create_widget<TooltipWidget>("div", "tooltip", "Helpful hint");
  auto* tooltip = static_cast<TooltipWidget*>(tooltip_elem->widget);
  tooltip_target->append(tooltip_elem);

  auto* toast_elem =
      box.create_widget<ToastWidget>("div", "toast", "Saved", ToastWidget::Type::Error);
  auto* toast = static_cast<ToastWidget*>(toast_elem->widget);

  root->append(dropdown_elem);
  root->append(select_elem);
  root->append(input_elem);
  root->append(textarea_elem);
  root->append(switch_elem);
  root->append(checkbox_elem);
  root->append(radio_elem);
  root->append(tabs_elem);
  root->append(accordion_elem);
  root->append(toggle_group_elem);
  root->append(modal_elem);
  root->append(popover_elem);
  root->append(menu_elem);
  root->append(sidebar_elem);
  root->append(searchbox_elem);
  root->append(tooltip_target);
  root->append(toast_elem);
  box.set_root(root);

  check(dropdown_elem->attribute("role") != nullptr);
  check(*dropdown_elem->attribute("role") == "combobox");
  check(*dropdown_elem->attribute("data-state") == "closed");
  check(*dropdown_elem->attribute("aria-expanded") == "false");
  dropdown->open();
  check(*dropdown_elem->attribute("data-state") == "open");
  check(*dropdown_elem->attribute("aria-expanded") == "true");
  dropdown->set_selected_value("closed");
  check(dropdown_elem->attribute("data-value") != nullptr);
  check(*dropdown_elem->attribute("data-value") == "closed");

  check(*select_elem->attribute("data-state") == "closed");
  check(*select_elem->attribute("data-value") == "Alpha");
  check(*select_elem->attribute("aria-expanded") == "false");
  select->set_expanded(true);
  select->set_disabled(true);
  check(*select_elem->attribute("data-state") == "open");
  check(*select_elem->attribute("aria-expanded") == "true");
  check(*select_elem->attribute("aria-disabled") == "true");
  check(select_elem->has_attribute("disabled"));

  check(input_elem->attribute("role") != nullptr);
  check(*input_elem->attribute("role") == "textbox");
  check(*input_elem->attribute("aria-readonly") == "false");
  check(*input_elem->attribute("aria-disabled") == "false");
  input->set_readonly(true);
  input->set_disabled(true);
  check(*input_elem->attribute("aria-readonly") == "true");
  check(*input_elem->attribute("aria-disabled") == "true");
  check(input_elem->has_attribute("readonly"));
  check(input_elem->has_attribute("disabled"));
  check(input_elem->has_state("readonly"));
  check(input_elem->has_state("disabled"));

  check(textarea_elem->attribute("role") != nullptr);
  check(*textarea_elem->attribute("role") == "textbox");
  check(*textarea_elem->attribute("aria-multiline") == "true");
  check(*textarea_elem->attribute("aria-readonly") == "false");
  check(*textarea_elem->attribute("aria-disabled") == "false");
  textarea->set_readonly(true);
  textarea->set_disabled(true);
  check(*textarea_elem->attribute("aria-readonly") == "true");
  check(*textarea_elem->attribute("aria-disabled") == "true");
  check(textarea_elem->has_attribute("readonly"));
  check(textarea_elem->has_attribute("disabled"));
  check(textarea_elem->has_state("readonly"));
  check(textarea_elem->has_state("disabled"));

  switch_widget->set_checked(true);
  check(*switch_elem->attribute("data-state") == "checked");
  check(*switch_elem->attribute("aria-checked") == "true");
  check(switch_elem->has_state("checked"));

  checkbox->set_checked(true);
  checkbox->set_disabled(true);
  check(*checkbox_elem->attribute("data-state") == "checked");
  check(*checkbox_elem->attribute("aria-checked") == "true");
  check(*checkbox_elem->attribute("aria-disabled") == "true");
  check(checkbox_elem->has_attribute("disabled"));

  radio->set_checked(true);
  check(*radio_elem->attribute("data-state") == "checked");
  check(*radio_elem->attribute("data-group") == "size");
  check(*radio_elem->attribute("data-value") == "lg");
  check(*radio_elem->attribute("aria-checked") == "true");

  check(tabs_elem->attribute("role") != nullptr);
  check(*tabs_elem->attribute("role") == "tablist");
  check(*tabs_elem->attribute("data-orientation") == "horizontal");
  check(*tabs_elem->attribute("data-active-id") == "general");
  check(*tabs_elem->attribute("aria-activedescendant") == "general");
  tabs->set_active_id("advanced");
  check(*tabs_elem->attribute("data-active-id") == "advanced");
  check(*tabs_elem->attribute("data-active-index") == "1");

  accordion->expand("section-a");
  accordion->collapse("section-a");
  check(*accordion_elem->attribute("data-state") == "closed");
  check(*accordion_elem->attribute("data-orientation") == "vertical");
  check(*accordion_elem->attribute("aria-expanded") == "false");
  accordion->expand("section-b");
  check(*accordion_elem->attribute("data-state") == "open");
  check(*accordion_elem->attribute("aria-expanded") == "true");
  check(*accordion_elem->attribute("data-expanded-id") == "section-b");
  check(*accordion_elem->attribute("data-expanded-count") == "1");

  toggle_group->set_selected_index(1);
  check(toggle_group_elem->attribute("role") != nullptr);
  check(*toggle_group_elem->attribute("role") == "radiogroup");
  check(*toggle_group_elem->attribute("data-orientation") == "horizontal");
  check(*toggle_group_elem->attribute("data-state") == "selected");
  check(*toggle_group_elem->attribute("data-value") == "center");
  check(*toggle_group_elem->attribute("data-selected-index") == "1");
  check(*toggle_group_elem->attribute("data-selected-count") == "1");
  toggle_group->set_multi_select(true);
  toggle_group->set_selected_indices({0, 2});
  check(*toggle_group_elem->attribute("role") == "group");
  check(*toggle_group_elem->attribute("data-selected-count") == "2");
  check_false(toggle_group_elem->has_attribute("data-value"));

  modal->open();
  check(modal_elem->attribute("role") != nullptr);
  check(*modal_elem->attribute("role") == "dialog");
  check(*modal_elem->attribute("data-state") == "open");
  check(*modal_elem->attribute("aria-modal") == "true");
  check(*modal_elem->attribute("aria-hidden") == "false");
  check(*modal_elem->attribute("aria-label") == "Settings");
  modal->close();
  check(*modal_elem->attribute("data-state") == "closed");
  check(*modal_elem->attribute("aria-modal") == "false");

  check(popover_elem->attribute("data-state") != nullptr);
  check(*popover_elem->attribute("data-state") == "closed");
  check(*popover_elem->attribute("data-side") == "bottom");
  check(*popover_elem->attribute("aria-haspopup") == "dialog");
  check(*popover_elem->attribute("aria-expanded") == "false");
  check(*popover_elem->attribute("aria-hidden") == "true");
  check(*popover_elem->attribute("aria-label") == "More details");
  popover->show();
  popover->set_position(PopoverWidget::Position::Left);
  check(*popover_elem->attribute("data-state") == "open");
  check(*popover_elem->attribute("data-side") == "left");
  check(*popover_elem->attribute("aria-expanded") == "true");
  check(*popover_elem->attribute("aria-hidden") == "false");

  check(menu_elem->attribute("role") != nullptr);
  check(*menu_elem->attribute("role") == "menu");
  check(*menu_elem->attribute("data-state") == "closed");
  check(*menu_elem->attribute("aria-orientation") == "vertical");
  check(*menu_elem->attribute("aria-hidden") == "true");
  menu->show(20.0f, 24.0f);
  check(*menu_elem->attribute("data-state") == "open");
  check(*menu_elem->attribute("aria-hidden") == "false");
  menu->hide();
  check(*menu_elem->attribute("data-state") == "closed");
  check(*menu_elem->attribute("aria-hidden") == "true");

  check(sidebar_elem->attribute("role") != nullptr);
  check(*sidebar_elem->attribute("role") == "navigation");
  check(*sidebar_elem->attribute("aria-orientation") == "vertical");
  check(*sidebar_elem->attribute("data-state") == "expanded");
  check(*sidebar_elem->attribute("data-collapsible") == "true");
  check(*sidebar_elem->attribute("data-item-count") == "2");
  check_false(sidebar_elem->has_attribute("data-selected-id"));
  sidebar->select("search");
  check(*sidebar_elem->attribute("data-selected-id") == "search");
  sidebar->set_collapsed(true);
  check(*sidebar_elem->attribute("data-state") == "collapsed");

  check(searchbox_elem->attribute("role") != nullptr);
  check(*searchbox_elem->attribute("role") == "combobox");
  check(*searchbox_elem->attribute("aria-autocomplete") == "list");
  check(*searchbox_elem->attribute("aria-haspopup") == "listbox");
  check(*searchbox_elem->attribute("aria-expanded") == "false");
  check(*searchbox_elem->attribute("aria-hidden") == "false");
  check(*searchbox_elem->attribute("data-state") == "closed");
  check(*searchbox_elem->attribute("data-result-count") == "2");
  check(*searchbox_elem->attribute("aria-label") == "Search docs");
  searchbox->set_text("api");
  searchbox->set_focused(true);
  searchbox->set_dropdown_open(true);
  check(*searchbox_elem->attribute("data-state") == "open");
  check(*searchbox_elem->attribute("aria-expanded") == "true");
  check(*searchbox_elem->attribute("data-result-count") == "1");
  check(*searchbox_elem->attribute("data-active-id") == "api-ref");
  check(*searchbox_elem->attribute("aria-activedescendant") == "api-ref");
  check(*searchbox_elem->attribute("data-value") == "api");
  check(*searchbox_elem->attribute("aria-label") == "api");

  tooltip->show();
  tooltip->update(600.0f, *tooltip_elem);
  check(tooltip_elem->attribute("role") != nullptr);
  check(*tooltip_elem->attribute("role") == "tooltip");
  check(*tooltip_elem->attribute("data-state") == "open");
  check(*tooltip_elem->attribute("data-side") == "top");
  check(*tooltip_elem->attribute("aria-hidden") == "false");
  check(*tooltip_elem->attribute("aria-label") == "Helpful hint");
  tooltip->set_position(TooltipWidget::Position::Bottom);
  tooltip->update(0.0f, *tooltip_elem);
  check(*tooltip_elem->attribute("data-side") == "bottom");
  tooltip->hide();
  tooltip->update(200.0f, *tooltip_elem);
  check(*tooltip_elem->attribute("data-state") == "closed");
  check(*tooltip_elem->attribute("aria-hidden") == "true");

  toast->show();
  check(toast_elem->attribute("role") != nullptr);
  check(*toast_elem->attribute("role") == "alert");
  check(*toast_elem->attribute("data-state") == "open");
  check(*toast_elem->attribute("data-type") == "error");
  check(*toast_elem->attribute("aria-live") == "assertive");
  check(*toast_elem->attribute("aria-hidden") == "false");
  check(*toast_elem->attribute("aria-atomic") == "true");
  toast->hide();
  check(*toast_elem->attribute("data-state") == "closed");
  check(*toast_elem->attribute("aria-hidden") == "true");
}

spec("Table and tree widgets expose collection state through host bridge") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");
  
    auto* table_elem = box.create_widget<TableWidget>("div", "table");
    auto* table = static_cast<TableWidget*>(table_elem->widget);
    table->add_column("Name", 140.0f);
    table->add_column("Role", 100.0f);
    table->add_row({"Ada", "Admin"});
    table->add_row({"Linus", "Owner"});
    table->set_selected_row(1);
  
    auto* tree_elem = box.create_widget<TreeWidget>("div", "tree");
    auto* tree = static_cast<TreeWidget*>(tree_elem->widget);
    auto root_node = tree->add_node("settings", "Settings");
    tree->add_node("display", "Display", root_node.get());
    tree->expand("settings");
    tree->set_selected("display");
  
    root->append(table_elem);
    root->append(tree_elem);
    box.set_root(root);
  
    check(table_elem->attribute("role") != nullptr);
    check(*table_elem->attribute("role") == "table");
    check(*table_elem->attribute("data-state") == "selected");
    check(*table_elem->attribute("data-column-count") == "2");
    check(*table_elem->attribute("data-row-count") == "2");
    check(*table_elem->attribute("data-selected-count") == "1");
    check(*table_elem->attribute("data-selected-row") == "1");
    check(*table_elem->attribute("aria-colcount") == "2");
    check(*table_elem->attribute("aria-rowcount") == "2");
    check(table_elem->has_state("selected"));
  
    table->clear();
    check(*table_elem->attribute("data-state") == "empty");
    check(*table_elem->attribute("data-row-count") == "0");
    check(*table_elem->attribute("data-selected-count") == "0");
    check_false(table_elem->has_attribute("data-selected-row"));
    check_false(table_elem->has_state("selected"));
  
    check(tree_elem->attribute("role") != nullptr);
    check(*tree_elem->attribute("role") == "tree");
    check(*tree_elem->attribute("data-state") == "selected");
    check(*tree_elem->attribute("data-node-count") == "2");
    check(*tree_elem->attribute("data-expanded-count") == "1");
    check(*tree_elem->attribute("data-selected-count") == "1");
    check(*tree_elem->attribute("data-selected-id") == "display");
    check(*tree_elem->attribute("aria-activedescendant") == "display");
    check(*tree_elem->attribute("aria-multiselectable") == "false");
    check(tree_elem->has_state("selected"));
    check(tree_elem->has_state("open"));
  
    tree->remove_node("settings");
    check(*tree_elem->attribute("data-state") == "empty");
    check(*tree_elem->attribute("data-node-count") == "0");
    check(*tree_elem->attribute("data-expanded-count") == "0");
    check(*tree_elem->attribute("data-selected-count") == "0");
    check_false(tree_elem->has_attribute("data-selected-id"));
    check_false(tree_elem->has_attribute("aria-activedescendant"));
    check_false(tree_elem->has_state("selected"));
    check_false(tree_elem->has_state("open"));
  }
}

spec("Progress and divider widgets expose primitive accessibility semantics") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");

    auto* progress_elem =
        box.create_widget<ProgressBarWidget>("div", "progress", 62.0f, false);
    auto* progress = static_cast<ProgressBarWidget*>(progress_elem->widget);

    auto* divider_elem = box.create_widget<DividerWidget>(
        "div", "separator", DividerWidget::Orientation::Vertical);
    auto* divider = static_cast<DividerWidget*>(divider_elem->widget);

    root->append(progress_elem);
    root->append(divider_elem);
    box.set_root(root);

    check(progress_elem->attribute("role") != nullptr);
    check(*progress_elem->attribute("role") == "progressbar");
    check(*progress_elem->attribute("aria-valuemin") == "0");
    check(*progress_elem->attribute("aria-valuemax") == "100");
    check(*progress_elem->attribute("aria-valuenow") == "62");
    check(*progress_elem->attribute("data-state") == "loading");

    progress->set_indeterminate(true);
    check(*progress_elem->attribute("data-state") == "indeterminate");
    check_false(progress_elem->has_attribute("aria-valuenow"));
    check(progress_elem->has_state("indeterminate"));

    progress->set_indeterminate(false);
    progress->set_value(100.0f);
    check(*progress_elem->attribute("aria-valuenow") == "100");
    check(*progress_elem->attribute("data-state") == "complete");

    check(divider_elem->attribute("role") != nullptr);
    check(*divider_elem->attribute("role") == "separator");
    check(*divider_elem->attribute("aria-orientation") == "vertical");

    divider->set_orientation(DividerWidget::Orientation::Horizontal);
    check(*divider_elem->attribute("aria-orientation") == "horizontal");
  }
}

spec("Slider and datepicker widgets expose shadcn style host semantics") {
  it("runs") {
    Box box(nullptr);
    auto* root = box.create("div");

    auto* slider_elem =
        box.create_widget<SliderWidget>("div", "slider", 0.0f, 100.0f, 42.0f, 0.25f);
    auto* slider = static_cast<SliderWidget*>(slider_elem->widget);

    auto* datepicker_elem =
        box.create_widget<DatePickerWidget>("button", "datepicker", Date{2024, 4, 22});
    auto* datepicker = static_cast<DatePickerWidget*>(datepicker_elem->widget);

    root->append(slider_elem);
    root->append(datepicker_elem);
    box.set_root(root);

    check(slider_elem->attribute("role") != nullptr);
    check(*slider_elem->attribute("role") == "slider");
    check(*slider_elem->attribute("aria-valuemin") == "0");
    check(*slider_elem->attribute("aria-valuemax") == "100");
    check(*slider_elem->attribute("aria-valuenow") == "42");
    check(*slider_elem->attribute("data-state") == "enabled");

    slider->set_min(-10.5f);
    slider->set_max(125.25f);
    slider->set_value(12.75f);
    check(*slider_elem->attribute("aria-valuemin") == "-10.5");
    check(*slider_elem->attribute("aria-valuemax") == "125.25");
    check(*slider_elem->attribute("aria-valuenow") == "12.75");

    slider->set_disabled(true);
    check(*slider_elem->attribute("data-state") == "disabled");
    check(*slider_elem->attribute("aria-disabled") == "true");

    check(datepicker_elem->attribute("role") != nullptr);
    check(*datepicker_elem->attribute("role") == "combobox");
    check(*datepicker_elem->attribute("aria-haspopup") == "dialog");
    check(*datepicker_elem->attribute("aria-expanded") == "false");
    check(*datepicker_elem->attribute("data-state") == "closed");
    check(*datepicker_elem->attribute("data-value") == "2024-04-22");

    datepicker->set_open(true);
    check(*datepicker_elem->attribute("aria-expanded") == "true");
    check(*datepicker_elem->attribute("data-state") == "open");
  }
}
