/*
 * flexUI - Visual Demo with MVC Architecture (GLFW version)
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h>
#include <flex/bridge/renderer.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
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
#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include "demo_styles.h"

using namespace flexUI;

constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 900;

// ============================================================================
// Model
// ============================================================================

struct DemoModel {
    int current_view = 0;
    float progress_value = 0.0f;
    const char* view_titles[4] = {"Component Gallery", "Calendar & Date", "Data Display", "Tab Navigation"};
    const char* current_title() const { return view_titles[current_view]; }
};


// ============================================================================
// View
// ============================================================================

class DemoView {
public:
    DemoView(Box* box) : box_(box) {}

    void build() {
        box_->load_css(examples::DEMO_CSS);
        root_ = box_->create("div", "root");
        box_->set_root(root_);
        build_sidebar();
        build_main_content();
        build_overlays();
    }

    void update(const DemoModel& model) {
        if (progress_elem_) {
            auto* pw = static_cast<ProgressBarWidget*>(progress_elem_->widget);
            float new_value = model.progress_value * 100.0f;
            if (std::abs(pw->value() - new_value) > 0.1f) {
                pw->set_value(new_value);
                progress_elem_->mark_paint_dirty();
            }
        }
        if (title_label_) {
            static_cast<LabelWidget*>(title_label_->widget)->set_text(model.current_title());
        }
    }

    void switch_view(int index) {
        for (auto* view : views_) if (view) view->add_class("hidden");
        for (auto* nav : nav_buttons_) if (nav) nav->remove_class("active");
        if (index >= 0 && index < 4) {
            if (views_[index]) views_[index]->remove_class("hidden");
            if (nav_buttons_[index]) nav_buttons_[index]->add_class("active");
        }
        box_->invalidate();
    }

    Element* nav_button(int i) const { return nav_buttons_[i]; }
    Box* box() const { return box_; }

private:
    Box* box_;
    Element* root_ = nullptr;
    Element* nav_buttons_[4] = {};
    Element* views_[4] = {};
    Element* title_label_ = nullptr;
    Element* progress_elem_ = nullptr;
    Element* tab_pages_[3] = {};

    void build_sidebar() {
        auto* sidebar = box_->create("div", "sidebar");
        root_->append(sidebar);

        auto* brand = box_->create_widget<LabelWidget>("label", "brand", "flexUI");
        brand->add_class("brand");
        sidebar->append(brand);

        const char* nav_labels[] = {"Components", "Calendar", "Data Table", "Tabs"};
        for (int i = 0; i < 4; ++i) {
            nav_buttons_[i] = box_->create_widget<ButtonWidget>("button", "", nav_labels[i]);
            nav_buttons_[i]->add_class("nav-item");
            if (i == 0) nav_buttons_[i]->add_class("active");
            sidebar->append(nav_buttons_[i]);
        }

        sidebar->append(box_->create_widget<DividerWidget>("divider", "", DividerWidget::Orientation::Horizontal));
        auto* ver = box_->create_widget<LabelWidget>("label", "", "v2.0.0-alpha");
        ver->add_class("card-title");
        sidebar->append(ver);
    }

    void build_main_content() {
        auto* main = box_->create("div", "main-content");
        root_->append(main);

        title_label_ = box_->create_widget<LabelWidget>("label", "", "Component Gallery");
        title_label_->add_class("brand");
        main->append(title_label_);

        views_[0] = build_components_view();
        views_[1] = build_calendar_view();
        views_[2] = build_data_view();
        views_[3] = build_tabs_view();

        for (int i = 0; i < 4; ++i) {
            if (i > 0) views_[i]->add_class("hidden");
            main->append(views_[i]);
        }
    }

    Element* build_components_view() {
        auto* view = box_->create("div", "components-view");
        view->add_class("gallery-grid");

        auto* col1 = box_->create("div", "col1");
        col1->add_class("col");
        view->append(col1);

        // Buttons Card
        auto* card1 = box_->create("div", ""); card1->add_class("card"); col1->append(card1);
        auto* h1 = box_->create_widget<LabelWidget>("label", "", "Buttons"); h1->add_class("card-title"); card1->append(h1);
        auto* row1 = box_->create("div", ""); row1->add_class("card-row"); card1->append(row1);
        row1->append(box_->create_widget<ButtonWidget>("button", "", "Primary"));
        auto* b2 = box_->create_widget<ButtonWidget>("button", "", "Success"); b2->add_class("success"); row1->append(b2);
        auto* b3 = box_->create_widget<ButtonWidget>("button", "", "Danger"); b3->add_class("danger"); row1->append(b3);

        // Form Controls Card
        auto* card2 = box_->create("div", ""); card2->add_class("card"); col1->append(card2);
        auto* h2 = box_->create_widget<LabelWidget>("label", "", "Form Controls"); h2->add_class("card-title"); card2->append(h2);
        auto* r1 = box_->create("div", ""); r1->add_class("card-row"); card2->append(r1);
        r1->append(box_->create_widget<InputWidget>("input", "", "Email address...", false));
        auto* r2 = box_->create("div", ""); r2->add_class("card-row"); card2->append(r2);
        r2->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Remember me", true));
        r2->append(box_->create_widget<SwitchWidget>("switch", "", "Wifi", true));
        auto* r3 = box_->create("div", ""); r3->add_class("card-row"); card2->append(r3);
        r3->append(box_->create_widget<LabelWidget>("label", "", "Quantity:"));
        r3->append(box_->create_widget<StepperWidget>("stepper", "", 1, 0, 99, 1));

        // Progress Card
        auto* card3 = box_->create("div", ""); card3->add_class("card"); col1->append(card3);
        auto* h3 = box_->create_widget<LabelWidget>("label", "", "Progress & Valuators"); h3->add_class("card-title"); card3->append(h3);
        card3->append(box_->create_widget<SliderWidget>("slider", "", 0.0f, 100.0f, 60.0f));
        progress_elem_ = box_->create_widget<ProgressBarWidget>("progressbar", "prog1", 0.0f, false);
        card3->append(progress_elem_);
        auto* r4 = box_->create("div", ""); r4->add_class("card-row"); card3->append(r4);
        auto* badge = box_->create_widget<BadgeWidget>("badge", "", "");
        static_cast<BadgeWidget*>(badge->widget)->set_count(99);
        r4->append(badge);

        auto* col2 = box_->create("div", "col2");
        col2->add_class("col");
        view->append(col2);

        // Data & Trees Card
        auto* card4 = box_->create("div", ""); card4->add_class("card"); col2->append(card4);
        auto* h4 = box_->create_widget<LabelWidget>("label", "", "Data & Trees"); h4->add_class("card-title"); card4->append(h4);
        auto* tree = box_->create_widget<TreeWidget>("tree", "");
        auto* tw = static_cast<TreeWidget*>(tree->widget);
        auto r = tw->add_node("r", "Project Root");
        tw->add_node("src", "Source", r.get());
        tw->add_node("inc", "Include", r.get());
        tw->expand("r");
        card4->append(tree);
        auto* dropdown = box_->create_widget<DropdownWidget>("dropdown", "", "Select Branch...");
        auto* dw = static_cast<DropdownWidget*>(dropdown->widget);
        dw->add_option("main", "main");
        dw->add_option("develop", "develop");
        card4->append(dropdown);

        // Toggle & Avatar Card
        auto* card5 = box_->create("div", ""); card5->add_class("card"); col2->append(card5);
        auto* h5 = box_->create_widget<LabelWidget>("label", "", "Toggle & Avatar"); h5->add_class("card-title"); card5->append(h5);
        auto* toggle = box_->create_widget<ToggleGroupWidget>("toggle-group", "");
        auto* tg = static_cast<ToggleGroupWidget*>(toggle->widget);
        tg->set_options({{"day", "Day"}, {"week", "Week"}, {"month", "Month"}});
        card5->append(toggle);
        auto* avatar_row = box_->create("div", ""); avatar_row->add_class("card-row"); card5->append(avatar_row);
        auto* av1 = box_->create_widget<AvatarWidget>("avatar", "", "Alice Chen", "");
        static_cast<AvatarWidget*>(av1->widget)->set_status(AvatarWidget::Status::Online);
        avatar_row->append(av1);
        auto* av2 = box_->create_widget<AvatarWidget>("avatar", "", "Bob Smith", "");
        static_cast<AvatarWidget*>(av2->widget)->set_status(AvatarWidget::Status::Away);
        avatar_row->append(av2);

        return view;
    }

    Element* build_calendar_view() {
        auto* view = box_->create("div", "calendar-view");
        view->add_class("gallery-grid");
        auto* card = box_->create("div", ""); card->add_class("card"); view->append(card);
        auto* head = box_->create_widget<LabelWidget>("label", "", "Date Picker"); head->add_class("card-title"); card->append(head);
        card->append(box_->create_widget<CalendarWidget>("calendar", "cal1"));
        return view;
    }

    Element* build_data_view() {
        auto* view = box_->create("div", "data-view");
        view->add_class("gallery-grid");
        auto* card = box_->create("div", ""); card->add_class("card"); card->add_class("wide-card"); view->append(card);
        auto* head = box_->create_widget<LabelWidget>("label", "", "Employee Data"); head->add_class("card-title"); card->append(head);
        auto* table = box_->create_widget<TableWidget>("table", "table1");
        auto* tw = static_cast<TableWidget*>(table->widget);
        tw->add_column("ID", 60); tw->add_column("Name", 150); tw->add_column("Role", 150); tw->add_column("Status", 100);
        tw->add_row({"1", "Alice Chen", "Engineer", "Active"});
        tw->add_row({"2", "Bob Smith", "Designer", "Active"});
        tw->add_row({"3", "Carol White", "Manager", "Away"});
        card->append(table);
        return view;
    }

    Element* build_tabs_view() {
        auto* view = box_->create("div", "tabs-view");
        view->add_class("tabs-view");

        auto* tab_content = box_->create("div", "tab-content");
        tab_content->add_class("tab-content");

        // Page 1: Profile
        tab_pages_[0] = box_->create("div", "page-profile"); tab_pages_[0]->add_class("tab-page"); tab_content->append(tab_pages_[0]);
        auto* h1 = box_->create_widget<LabelWidget>("label", "", "User Profile"); h1->add_class("card-title"); tab_pages_[0]->append(h1);
        auto* row1 = box_->create("div", ""); row1->add_class("card-row");
        row1->append(box_->create_widget<LabelWidget>("label", "", "Username:"));
        row1->append(box_->create_widget<InputWidget>("input", "u1", "admin"));
        tab_pages_[0]->append(row1);
        tab_pages_[0]->append(box_->create_widget<ButtonWidget>("button", "save", "Save Changes"));

        // Page 2: Settings
        tab_pages_[1] = box_->create("div", "page-settings"); tab_pages_[1]->add_class("tab-page"); tab_content->append(tab_pages_[1]);
        auto* h2 = box_->create_widget<LabelWidget>("label", "", "Application Settings"); h2->add_class("card-title"); tab_pages_[1]->append(h2);
        auto* row2 = box_->create("div", ""); row2->add_class("card-row");
        row2->append(box_->create_widget<LabelWidget>("label", "", "Notifications"));
        row2->append(box_->create_widget<SwitchWidget>("switch", "s1", "", true));
        tab_pages_[1]->append(row2);

        // Page 3: Navigation
        tab_pages_[2] = box_->create("div", "page-navigation"); tab_pages_[2]->add_class("tab-page"); tab_content->append(tab_pages_[2]);
        auto* h3 = box_->create_widget<LabelWidget>("label", "", "Navigation Components"); h3->add_class("card-title"); tab_pages_[2]->append(h3);
        auto* bc = box_->create_widget<BreadcrumbWidget>("breadcrumb", "");
        auto* bw = static_cast<BreadcrumbWidget*>(bc->widget);
        bw->set_items({{"home", "Home"}, {"products", "Products"}});
        tab_pages_[2]->append(bc);
        tab_pages_[2]->append(box_->create_widget<PaginationWidget>("pagination", "", 10, 1));

        auto* tabs_elem = box_->create_widget<TabsWidget>("tabs", "main-tabs");
        auto* tabs_w = static_cast<TabsWidget*>(tabs_elem->widget);
        tabs_w->add_tab("Profile", "profile", tab_pages_[0]);
        tabs_w->add_tab("Settings", "settings", tab_pages_[1]);
        tabs_w->add_tab("Navigation", "navigation", tab_pages_[2]);
        view->append(tabs_elem);
        view->append(tab_content);

        return view;
    }

    void build_overlays() {
        root_->append(box_->create_widget<ToastWidget>("toast", "toast1", "Welcome to flexUI!", ToastWidget::Type::Success));
        root_->append(box_->create_widget<ModalWidget>("modal", "modal1", "System Status"));
    }
};


// ============================================================================
// Controller
// ============================================================================

class DemoController {
public:
    DemoController(DemoModel& model, DemoView& view) : model_(model), view_(view) {}

    void setup_bindings() {
        view_.box()->set_event_callback([this](Element& elem, const Event& event) {
            if (event.type != EventType::MouseUp) return;
            for (int i = 0; i < 4; ++i) {
                if (&elem == view_.nav_button(i)) {
                    model_.current_view = i;
                    view_.switch_view(i);
                    return;
                }
            }
        });
    }

    void update(float delta_ms) {
        float old = model_.progress_value;
        model_.progress_value += 0.0005f * delta_ms;
        if (model_.progress_value > 1.0f) model_.progress_value = 0.0f;
        if (std::abs(model_.progress_value - old) > 0.01f || model_.progress_value == 0.0f) {
            view_.update(model_);
        }
    }

    void handle_key(int key) {
        if (key == GLFW_KEY_1) { model_.current_view = 0; view_.switch_view(0); }
        else if (key == GLFW_KEY_2) { model_.current_view = 1; view_.switch_view(1); }
        else if (key == GLFW_KEY_3) { model_.current_view = 2; view_.switch_view(2); }
        else if (key == GLFW_KEY_4) { model_.current_view = 3; view_.switch_view(3); }
    }

private:
    DemoModel& model_;
    DemoView& view_;
};

// ============================================================================
// Application
// ============================================================================

class VisualDemo {
public:
    bool init() {
        std::cout << "=== flexUI Visual Demo (GLFW) ===" << std::endl;

        if (!glfwInit()) {
            std::cerr << "GLFW init failed" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window_ = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "flexUI Visual Demo - GLFW", nullptr, nullptr);
        if (!window_) {
            std::cerr << "Window creation failed" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window_);
        glfwSetWindowUserPointer(window_, this);
        glfwSetKeyCallback(window_, key_callback);
        glfwSetMouseButtonCallback(window_, mouse_button_callback);
        glfwSetCursorPosCallback(window_, cursor_pos_callback);
        glfwSetScrollCallback(window_, scroll_callback);
        glfwSetCharCallback(window_, char_callback);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "GLAD init failed" << std::endl;
            return false;
        }

        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
        glfwSwapInterval(1);

        if (tvg::Initializer::init(4) != tvg::Result::Success) {
            std::cerr << "ThorVG init failed" << std::endl;
            return false;
        }

        if (!load_font("Arial", "C:/Windows/Fonts/arial.ttf")) return false;

        canvas_.reset(tvg::GlCanvas::gen());
        if (!canvas_) {
            std::cerr << "GlCanvas creation failed" << std::endl;
            return false;
        }

        canvas_->target(glfwGetCurrentContext(), 0, WINDOW_WIDTH, WINDOW_HEIGHT, tvg::ColorSpace::ABGR8888S);
        flex_renderer_ = flex::create_thorvg_renderer(canvas_.get());
        if (!flex_renderer_) {
            std::cerr << "Renderer creation failed" << std::endl;
            return false;
        }

        box_ = std::make_unique<Box>(flex_renderer_.get());
        box_->set_viewport(WINDOW_WIDTH, WINDOW_HEIGHT);

        model_ = std::make_unique<DemoModel>();
        view_ = std::make_unique<DemoView>(box_.get());
        view_->build();

        controller_ = std::make_unique<DemoController>(*model_, *view_);
        controller_->setup_bindings();

        box_->update();
        glfwSwapBuffers(window_);

        std::cout << "\nDemo ready! Keys: 1-4 to switch views, ESC to quit" << std::endl;
        return true;
    }

    void run() {
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(window_)) {
            double now = glfwGetTime();
            float delta_ms = static_cast<float>((now - last) * 1000.0);
            last = now;

            glfwPollEvents();
            controller_->update(delta_ms);
            box_->update_time(delta_ms);

            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            box_->invalidate();  // Force repaint every frame for GlCanvas
            box_->update();
            glfwSwapBuffers(window_);
        }
    }

    ~VisualDemo() {
        controller_.reset();
        view_.reset();
        model_.reset();
        box_.reset();
        flex_renderer_.reset();
        canvas_.reset();
        tvg::Initializer::term();
        if (window_) glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
    std::unique_ptr<tvg::GlCanvas> canvas_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
    std::unique_ptr<Box> box_;
    std::unique_ptr<DemoModel> model_;
    std::unique_ptr<DemoView> view_;
    std::unique_ptr<DemoController> controller_;

    bool load_font(const char* name, const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) { std::cerr << "Failed to open " << path << std::endl; return false; }
        auto size = file.tellg();
        file.seekg(0);
        std::vector<char> buf(size);
        file.read(buf.data(), size);
        if (tvg::Text::load(name, buf.data(), (uint32_t)size, "ttf", true) != tvg::Result::Success) {
            std::cerr << "Failed to load font" << std::endl;
            return false;
        }
        std::cout << "Font loaded: " << name << std::endl;
        return true;
    }

    static void key_callback(GLFWwindow* w, int key, int, int action, int mods) {
        auto* app = static_cast<VisualDemo*>(glfwGetWindowUserPointer(w));
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(w, GLFW_TRUE);
            return;
        }
        if (action == GLFW_PRESS) app->controller_->handle_key(key);
        auto e = (action == GLFW_PRESS || action == GLFW_REPEAT)
            ? Event::key_down(glfw_to_keycode(key), glfw_to_mods(mods))
            : Event::key_up(glfw_to_keycode(key), glfw_to_mods(mods));
        app->box_->dispatch_event(e);
    }

    static void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
        auto* app = static_cast<VisualDemo*>(glfwGetWindowUserPointer(w));
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        auto e = (action == GLFW_PRESS)
            ? Event::mouse_down((float)x, (float)y, glfw_to_button(button))
            : Event::mouse_up((float)x, (float)y, glfw_to_button(button));
        app->box_->dispatch_event(e);
    }

    static void cursor_pos_callback(GLFWwindow* w, double x, double y) {
        auto* app = static_cast<VisualDemo*>(glfwGetWindowUserPointer(w));
        auto e = Event::mouse_move((float)x, (float)y);
        app->box_->dispatch_event(e);
    }

    static void scroll_callback(GLFWwindow* w, double dx, double dy) {
        auto* app = static_cast<VisualDemo*>(glfwGetWindowUserPointer(w));
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        auto e = Event::mouse_wheel((float)x, (float)y, (float)dx, (float)dy);
        app->box_->dispatch_event(e);
    }

    static void char_callback(GLFWwindow* w, unsigned int codepoint) {
        auto* app = static_cast<VisualDemo*>(glfwGetWindowUserPointer(w));
        char buf[5] = {};
        if (codepoint < 0x80) buf[0] = (char)codepoint;
        else if (codepoint < 0x800) { buf[0] = 0xC0 | (codepoint >> 6); buf[1] = 0x80 | (codepoint & 0x3F); }
        else { buf[0] = 0xE0 | (codepoint >> 12); buf[1] = 0x80 | ((codepoint >> 6) & 0x3F); buf[2] = 0x80 | (codepoint & 0x3F); }
        auto e = Event::text_input(buf);
        app->box_->dispatch_event(e);
    }

    static KeyCode glfw_to_keycode(int key) {
        switch (key) {
            case GLFW_KEY_LEFT: return KeyCode::Left;
            case GLFW_KEY_RIGHT: return KeyCode::Right;
            case GLFW_KEY_UP: return KeyCode::Up;
            case GLFW_KEY_DOWN: return KeyCode::Down;
            case GLFW_KEY_HOME: return KeyCode::Home;
            case GLFW_KEY_END: return KeyCode::End;
            case GLFW_KEY_BACKSPACE: return KeyCode::Backspace;
            case GLFW_KEY_DELETE: return KeyCode::Delete;
            case GLFW_KEY_ENTER: return KeyCode::Enter;
            case GLFW_KEY_TAB: return KeyCode::Tab;
            case GLFW_KEY_ESCAPE: return KeyCode::Escape;
            case GLFW_KEY_A: return KeyCode::A;
            case GLFW_KEY_C: return KeyCode::C;
            case GLFW_KEY_V: return KeyCode::V;
            case GLFW_KEY_X: return KeyCode::X;
            case GLFW_KEY_Z: return KeyCode::Z;
            default: return KeyCode::Unknown;
        }
    }

    static int glfw_to_mods(int mods) {
        int r = 0;
        if (mods & GLFW_MOD_SHIFT) r |= (int)KeyMod::Shift;
        if (mods & GLFW_MOD_CONTROL) r |= (int)KeyMod::Control;
        if (mods & GLFW_MOD_ALT) r |= (int)KeyMod::Alt;
        if (mods & GLFW_MOD_SUPER) r |= (int)KeyMod::Super;
        return r;
    }

    static MouseButton glfw_to_button(int button) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::Left;
            case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::Right;
            case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
            default: return MouseButton::Left;
        }
    }
};

int main() {
    VisualDemo demo;
    if (!demo.init()) return 1;
    demo.run();
    return 0;
}
