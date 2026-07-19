#ifndef WINDOWS_LEAN_AND_MEAN
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h> 
#include <flexUI.h>
#include "glfw_app.h"
#include <flex/bridge/renderer.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/shadcn_ir.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/tree_widget.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/image_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/widgets/card_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <filesystem>
#include <initializer_list>
#include "demo_styles.h"
#include "host_input_bridge.h"
#include "renderer_capability_label.h"

using namespace flexUI;

namespace {

std::string demo_asset_path(const char* relative_path) {
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    return (root / relative_path).string();
}

void add_utilities(flexUI::Element* elem,
                   std::initializer_list<const char*> utilities) {
    if (!elem) {
        return;
    }
    for (const char* utility : utilities) {
        if (utility && *utility) {
            elem->add_utility(utility);
        }
    }
}

} // namespace

class VisualDemo : public ::flex::GlfwApp {
public:
    VisualDemo() : ::flex::GlfwApp("flexUI Visual Demo", 1200, 900) {}

protected:
    bool on_init() override {
        // Try multiple paths for the font file
        const char* font_paths[] = {
            "fonts/NotoSansSC-Regular.ttf",
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/arial.ttf",
         };

        bool loaded = false;
        for (const char* path : font_paths) {
            if (load_font("NotoSansSC", path)) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            std::cerr << "Warning: Could not load NotoSansSC font." << std::endl;
        }

        if (!load_font("Consolas", "C:/Windows/Fonts/consola.ttf")) {
            std::cerr << "Warning: Could not load Consolas font." << std::endl;
        }

        box_ = std::make_unique<flexUI::Box>(renderer());
        box_->set_viewport((float)width(), (float)height());
        box_->load_css(examples::DEMO_CSS);
        box_->load_css(examples::DEMO_STATE_CSS);

        auto* root = box_->create("div", "root");
        add_utilities(root, {"flex", "flex-row", "w-[1200px]", "h-[900px]",
                           "bg-background"});
        box_->set_root(root);

        // Sidebar
        auto* sidebar = box_->create("div", "sidebar");
        add_utilities(sidebar, {"flex", "flex-col", "w-[260px]", "h-[900px]",
                              "p-5", "gap-2.5", "bg-muted", "shadow-lg"});
        root->append(sidebar);

        auto* brand = box_->create("div");
        brand->add_class("brand");
        add_utilities(brand, {"flex", "items-center", "text-2xl", "w-[220px]",
                            "h-[60px]", "pb-5", "text-foreground"});
        brand->append(box_->create_widget<LabelWidget>("span", "", "flexUI"));
        sidebar->append(brand);

        const char* menu_items[] = {"Gallery", "Calendar", "Data", "Tabs"};
        for (int i = 0; i < 4; ++i) {
            auto* btn = box_->create_widget<ButtonWidget>("button", "");
            btn->add_class("nav-item");
            add_utilities(btn, {"flex", "items-center", "w-[220px]", "h-[40px]",
                              "rounded-lg", "px-3", "bg-transparent",
                              "text-muted-foreground"});
            static_cast<ButtonWidget*>(btn->widget)->set_text(menu_items[i]);
            if (i == 0) btn->add_class("active");
            
            nav_buttons_[i] = btn;
            sidebar->append(btn);
        }

        auto* divider = box_->create("divider", "");
        add_utilities(divider, {"w-[220px]", "h-[1px]", "bg-border", "my-2.5"});
        sidebar->append(divider);

        // Main Content
        auto* main = box_->create("div", "main-content");
        add_utilities(main, {"flex", "flex-col", "w-[940px]", "h-[900px]",
                           "p-10", "gap-6", "bg-background"});
        root->append(main);

        // Header
        auto* header = box_->create("div");
        add_utilities(header, {"flex", "flex-row", "justify-between",
                             "items-center", "w-[860px]", "h-[60px]",
                             "mb-5"});

        title_label_ = box_->create_widget<LabelWidget>("label", "", "Component Gallery");
        add_utilities(title_label_, {"text-2xl", "w-[400px]", "text-foreground"});
        header->append(title_label_);

        backend_label_ = box_->create_widget<LabelWidget>(
            "label", "backend-status",
            std::string("Renderer ") +
                flexui_examples::renderer_capability_label(box_->renderer_capabilities()));
        add_utilities(backend_label_, {"text-[11px]", "w-[240px]",
                                     "text-muted-foreground", "overflow-hidden",
                                     "whitespace-nowrap"});
        header->append(backend_label_);
        
        auto* search = box_->create_widget<InputWidget>("input", "search-box");
        static_cast<InputWidget*>(search->widget)->set_placeholder("Search...");
        header->append(search);
        main->append(header);

        // View Container
        auto* container = box_->create("div", "view-container");
        add_utilities(container, {"flex", "relative", "w-[860px]", "h-[800px]"});
        main->append(container);

        // Build all views
        views_[0] = build_gallery_view();
        views_[1] = build_calendar_view();
        views_[2] = build_data_view();
        views_[3] = build_tabs_view();

        for (int i = 0; i < 4; ++i) {
            container->append(views_[i]);
            if (i > 0) views_[i]->add_class("hidden");
        }

        // Global event handler
        box_->set_event_callback([this](Element& elem, const Event& e) {
            if (e.type == EventType::MouseDown) {
                for (int i = 0; i < 4; ++i) {
                    if (nav_buttons_[i] == &elem) {
                        this->switch_view(i);
                        break;
                    }
                }
            }
        });

        return true;
    }

    void switch_view(int index) {
        if (index < 0 || index >= 4) return;
        active_view_ = index;
        
        static const char* titles[] = {"Component Gallery", "Calendar & Scheduling", "Data Management", "Interface Tabs"};
        static_cast<LabelWidget*>(title_label_->widget)->set_text(titles[index]);

        for (int i = 0; i < 4; ++i) {
            if (views_[i]) {
                if (i == index) views_[i]->remove_class("hidden");
                else views_[i]->add_class("hidden");
            }
            if (nav_buttons_[i]) {
                if (i == index) nav_buttons_[i]->add_class("active");
                else nav_buttons_[i]->remove_class("active");
            }
        }
        
        box_->invalidate();
    }

    void on_update(float dt) override {
        box_->update_time(dt * 1000.0f);
        sync_ime_caret();
    }

    void on_render() override {
        box_->update();
    }

    void on_resize(int w, int h) override {
        ::flex::GlfwApp::on_resize(w, h);
        if (box_) box_->set_viewport((float)w, (float)h);
    }

    void on_mouse_button(int button, int action, int mods) override {
        float x = 0.0f;
        float y = 0.0f;
        cursor_position(x, y);
        auto e = (action == GLFW_PRESS)
            ? Event::mouse_down(x, y, glfw_to_button(button))
            : Event::mouse_up(x, y, glfw_to_button(button));
        box_->dispatch_event(e);
    }

    void on_cursor_pos(double x, double y) override {
        auto e = Event::mouse_move((float)x, (float)y);
        box_->dispatch_event(e);
    }

    void on_key(int key, int action, int mods) override {
        if (action == GLFW_PRESS) {
            if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
                switch_view(key - GLFW_KEY_1);
            }
        }
        auto e = (action == GLFW_PRESS || action == GLFW_REPEAT)
            ? Event::key_down(glfw_to_keycode(key), glfw_to_mods(mods))
            : Event::key_up(glfw_to_keycode(key), glfw_to_mods(mods));
        box_->dispatch_event(e);
    }

    void on_scroll(double dx, double dy) override {
        float x = 0.0f;
        float y = 0.0f;
        cursor_position(x, y);
        auto e = flexUI::Event::mouse_wheel(x, y, (float)dx, (float)dy);
        box_->dispatch_event(e);
    }

    void on_char(unsigned int codepoint) override {
        std::string utf8;
        if (codepoint < 0x80) utf8 += (char)codepoint;
        else if (codepoint < 0x800) {
            utf8 += (char)(0xc0 | (codepoint >> 6));
            utf8 += (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint < 0x10000) {
            utf8 += (char)(0xe0 | (codepoint >> 12));
            utf8 += (char)(0x80 | ((codepoint >> 6) & 0x3f));
            utf8 += (char)(0x80 | (codepoint & 0x3f));
        }
        flexui_examples::dispatch_text_input_if_focused(box_.get(), utf8);
    }

private:
    struct TaskItem {
        std::string key;
        std::string label;
        bool complete = false;
    };

    flexUI::Box* ime_box() override { return box_.get(); }
    bool should_render_frame() const override { return box_ && box_->is_dirty(); }
    bool remove_canvas_before_render() const override { return false; }

    Element* create_view_pane() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        add_utilities(view, {"flex", "flex-col", "w-[860px]", "h-[800px]",
                           "gap-6"});
        return view;
    }

    Element* create_gallery_view_pane() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        view->add_class("gallery-grid");
        add_utilities(view, {"flex", "flex-row", "w-[860px]", "h-[800px]",
                           "gap-5"});
        return view;
    }

    Element* create_gallery_column() {
        auto* col = box_->create("div");
        col->add_class("col");
        add_utilities(col, {"flex", "flex-col", "w-[420px]", "gap-5"});
        return col;
    }

    Element* create_card(const char* title) {
        auto* card = box_->create("div");
        card->add_class("card");
        add_utilities(card, {"flex", "flex-col", "w-[420px]", "p-5", "gap-4",
                           "rounded-xl", "bg-card", "shadow-md"});

        auto* heading = box_->create("div");
        heading->add_class("card-title");
        add_utilities(heading, {"text-base", "w-[380px]", "h-[20px]",
                              "text-muted-foreground"});
        heading->append(box_->create_widget<LabelWidget>("span", "", title));
        card->append(heading);
        return card;
    }

    Element* create_card_row() {
        auto* row = box_->create("div");
        row->add_class("card-row");
        add_utilities(row, {"flex", "flex-row", "items-center", "gap-3",
                          "h-[40px]"});
        return row;
    }

    Element* create_avatar_row() {
        auto* row = box_->create("div");
        row->add_class("avatar-row");
        add_utilities(row, {"flex", "flex-row", "items-center", "gap-3",
                          "h-[64px]"});
        return row;
    }

    Element* create_tab_page(const char* id, const char* title) {
        auto* page = box_->create("div", id);
        page->add_class("tab-page");
        add_utilities(page, {"absolute", "top-0", "left-0", "w-full", "p-5",
                           "rounded-xl", "gap-4", "flex", "flex-col",
                           "bg-card", "shadow-md"});
        page->append(box_->create_widget<LabelWidget>("h2", "", title));
        return page;
    }

    Element* create_section_heading(const char* text) {
        auto* heading = box_->create_widget<LabelWidget>("h2", "", text);
        add_utilities(heading, {"text-2xl", "w-[400px]", "h-[40px]",
                              "text-foreground"});
        return heading;
    }

    Element* build_gallery_view() {
        auto* view = create_gallery_view_pane();

        auto* col1 = create_gallery_column();
        view->append(col1);

        auto* card1 = create_card("UI Components");

        auto* r1 = create_card_row();
        r1->append(box_->create_widget<ButtonWidget>("button", "", "Primary"));
        auto* g = box_->create_widget<ButtonWidget>("button", "", "Secondary");
        g->add_class("ghost");
        r1->append(g);
        card1->append(r1);

        auto* r2 = create_card_row();
        r2->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Push Status", true));
        r2->append(box_->create_widget<SwitchWidget>("switch", "", "Power", true));
        card1->append(r2);
        
        auto* r3 = create_card_row();
        auto* text_input = box_->create_widget<TextAreaWidget>("textarea", "");
        auto* taw = static_cast<TextAreaWidget*>(text_input->widget);
        taw->set_placeholder("Type with IME here...");
        add_utilities(text_input, {"w-[360px]", "h-[80px]"});
        r3->append(text_input);
        card1->append(r3);

        auto* r4 = create_avatar_row();
        auto* a1 = box_->create_widget<AvatarWidget>("avatar", "",
                                                     "Alice Smith",
                                                     demo_asset_path("assets/undraw/svgs/profile-image.svg"));
        static_cast<AvatarWidget*>(a1->widget)->set_status(AvatarWidget::Status::Online);
        r4->append(a1);

        auto* a2 = box_->create_widget<AvatarWidget>("avatar", "", "Bob Jones");
        static_cast<AvatarWidget*>(a2->widget)->set_status(AvatarWidget::Status::Away);
        r4->append(a2);

        auto* a3 = box_->create_widget<AvatarWidget>("avatar", "",
                                                     "Charlie Brown",
                                                     demo_asset_path("assets/undraw/svgs/account.svg"));
        static_cast<AvatarWidget*>(a3->widget)->set_status(AvatarWidget::Status::Busy);
        r4->append(a3);
        card1->append(r4);

        col1->append(card1);

        auto* col2 = create_gallery_column();
        view->append(col2);

        auto* card2 = create_card("System Metrics");

        progress_elem_ = box_->create_widget<ProgressBarWidget>("progressbar", "");
        static_cast<ProgressBarWidget*>(progress_elem_->widget)->set_value(65.0f);
        card2->append(progress_elem_);

        auto* badges = create_card_row();
        auto* b1 = box_->create_widget<BadgeWidget>("badge", "", "System OK"); b1->add_class("success");
        badges->append(b1);
        auto* b2 = box_->create_widget<BadgeWidget>("badge", "", "High Load"); b2->add_class("danger");
        badges->append(b2);
        card2->append(badges);
        col2->append(card2);

        return view;
    }

    Element* build_calendar_view() {
        auto* view = create_view_pane();
        view->append(create_section_heading("Monthly Schedule"));
        view->append(box_->create_widget<CalendarWidget>("calendar", ""));
        return view;
    }

    Element* build_data_view() {
        auto* view = create_view_pane();
        view->append(create_section_heading("User Database"));
        auto* table = box_->create_widget<TableWidget>("table", "");
        auto* tw = static_cast<TableWidget*>(table->widget);
        tw->add_column("ID", 60.0f);
        tw->add_column("Name", 200.0f);
        tw->add_column("Department", 150.0f);
        tw->add_column("Status", 120.0f);
        
        tw->add_row({"001", "Alice Smith", "Engineering", "Active"});
        tw->add_row({"002", "Bob Jones", "Design", "On Leave"});
        tw->add_row({"003", "Charlie Brown", "Product", "Active"});
        tw->add_row({"004", "Diana Prince", "Security", "Active"});
        tw->add_row({"005", "Edward Norton", "HR", "Active"});
        
        view->append(table);

        auto* task_header = box_->create("div");
        add_utilities(task_header, {"flex", "flex-row", "items-center",
                                  "justify-between", "w-[860px]", "h-[40px]"});
        task_header->append(
            box_->create_widget<LabelWidget>("h2", "", "Keyed Live Tasks"));

        auto* task_actions = box_->create("div");
        add_utilities(task_actions, {"flex", "flex-row", "items-center", "gap-2"});
        auto* rotate = box_->create_widget<ButtonWidget>("button", "", "Rotate");
        auto* clear = box_->create_widget<ButtonWidget>("button", "", "Clear");
        auto* restore = box_->create_widget<ButtonWidget>("button", "", "Restore");
        rotate->on_click([this] { rotate_tasks(); });
        clear->on_click([this] {
            tasks_.clear();
            reconcile_tasks();
        });
        restore->on_click([this] {
            tasks_ = initial_tasks_;
            reconcile_tasks();
        });
        task_actions->append(rotate);
        task_actions->append(clear);
        task_actions->append(restore);
        task_header->append(task_actions);
        view->append(task_header);

        task_list_ = box_->create("div", "live-task-list");
        add_utilities(task_list_, {"flex", "flex-col", "w-[860px]", "gap-2"});
        view->append(task_list_);
        task_repeater_ = std::make_unique<UiKeyedRepeater>(*box_, *task_list_);
        tasks_ = initial_tasks_;
        reconcile_tasks();
        return view;
    }

    void reconcile_tasks() {
        if (!task_repeater_) return;
        task_repeater_->reconcile(
            tasks_, [](const TaskItem& item) { return item.key; },
            [this](Box& box, const TaskItem& item) {
                auto* row = box.create_widget<ButtonWidget>(
                    "button", "task-" + item.key, item.label);
                row->add_class("live-task-row");
                add_utilities(row, {"flex", "flex-row", "items-center",
                                  "w-[860px]", "h-[40px]", "px-3", "py-2",
                                  "rounded-md", "bg-card", "text-sm"});
                row->on_click([this, key = item.key] { toggle_task(key); });
                return row;
            },
            [](Element& row, const TaskItem& item) {
                row.set_text(item.label);
                row.set_attribute("data-state", item.complete ? "complete" : "open");
                row.toggle_class("opacity-50", item.complete);
            });
        box_->invalidate();
    }

    void rotate_tasks() {
        if (tasks_.size() > 1) {
            std::rotate(tasks_.begin(), tasks_.begin() + 1, tasks_.end());
            reconcile_tasks();
        }
    }

    void toggle_task(const std::string& key) {
        const auto task = std::find_if(
            tasks_.begin(), tasks_.end(),
            [&](const TaskItem& item) { return item.key == key; });
        if (task == tasks_.end()) return;
        task->complete = !task->complete;
        reconcile_tasks();
    }

    Element* build_tabs_view() {
        auto* view = box_->create("div");
        view->add_class("view-pane");
        view->add_class("tabs-view");
        add_utilities(view, {"flex", "flex-col", "w-[860px]", "h-[800px]",
                           "gap-0"});
        
        auto* tabs_elem = box_->create_widget<TabsWidget>("tabs", "");
        auto* tw = static_cast<TabsWidget*>(tabs_elem->widget);
        
        auto* page1 = create_tab_page("tab-page-1", "Selectable Content");
        
        auto* selectable_text = box_->create_widget<TextAreaWidget>("textarea", "");
        auto* taw = static_cast<TextAreaWidget*>(selectable_text->widget);
        taw->set_text("This text is rendered using a TextAreaWidget set to read-only mode.\n\n"
                      "Unlike static labels, you can click and drag to SELECT this text.\n"
                      "This demonstrates how to achieve selectable content in flexUI.");
        taw->set_readonly(true);
        add_utilities(selectable_text, {"w-[820px]", "h-[300px]", "bg-transparent"});
        page1->append(selectable_text);
        
        auto* page2 = create_tab_page("tab-page-2", "Advanced Components");
        auto* page3 = create_tab_page("tab-page-3", "Network Config");
        page3->append(box_->create_widget<LabelWidget>("p", "", "Manage connections and proxy settings."));

        tw->add_tab("Selectable Text", "gen", page1);
        tw->add_tab("Advanced", "adv", page2);
        tw->add_tab("Network", "net", page3);

        view->append(tabs_elem);
        
        auto* content = box_->create("div", "tab-content");
        content->add_class("tab-content");
        add_utilities(content, {"relative", "w-full", "h-[700px]"});
        content->append(page1);
        content->append(page2);
        content->append(page3);
        view->append(content);

        return view;
    }

    std::unique_ptr<Box> box_;
    int active_view_ = 0;
    Element* progress_elem_ = nullptr;
    Element* title_label_ = nullptr;
    Element* backend_label_ = nullptr;
    Element* task_list_ = nullptr;
    Element* nav_buttons_[4] = {nullptr};
    Element* views_[4] = {nullptr};
    const std::vector<TaskItem> initial_tasks_ = {
        {"layout", "Rectangle tree owns layout", true},
        {"style", "CSS and Tailwind own visual style", false},
        {"state", "C++ and MIR own dynamic state", false},
    };
    std::vector<TaskItem> tasks_;
    std::unique_ptr<UiKeyedRepeater> task_repeater_;
};

int main() {
    VisualDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
