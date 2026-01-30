/*
 * flexUI - Visual Demo with MVC Architecture
 *
 * Architecture:
 *   Model      = DemoModel (application state)
 *   View       = DemoView (UI elements and bindings)
 *   Controller = DemoController (input handling and logic)
 */

#include <SDL2/SDL.h>
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
// Model - Application State
// ============================================================================

struct DemoModel {
    // Navigation state
    int current_view = 0;  // 0=Components, 1=Calendar, 2=Data, 3=Tabs
    int current_tab = 0;   // For tabs view
    int nested_tab = 0;    // For nested tabs

    // Progress animation
    float progress_value = 0.0f;

    // Toast state
    bool show_toast = true;
    float toast_timer = 0.0f;

    // Table data
    struct Employee {
        std::string id;
        std::string name;
        std::string role;
        std::string status;
    };
    std::vector<Employee> employees = {
        {"1", "Alice Chen", "Engineer", "Active"},
        {"2", "Bob Smith", "Designer", "Active"},
        {"3", "Carol White", "Manager", "Away"},
        {"4", "David Lee", "Developer", "Active"},
        {"5", "Eve Johnson", "Analyst", "Offline"}
    };

    // View titles
    const char* view_titles[4] = {
        "Component Gallery",
        "Calendar & Date",
        "Data Display",
        "Tab Navigation"
    };

    const char* current_title() const {
        return view_titles[current_view];
    }
};

// ============================================================================
// View - UI Creation and Binding
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

    // Update view from model
    void update(const DemoModel& model) {
        // Update progress bar
        if (progress_elem_) {
            auto* pw = static_cast<ProgressBarWidget*>(progress_elem_->widget);
            pw->set_value(model.progress_value * 100.0f);
            progress_elem_->mark_paint_dirty();
        }

        // Update title
        if (title_label_) {
            static_cast<LabelWidget*>(title_label_->widget)->set_text(model.current_title());
        }
    }

    void switch_view(int index) {
        // Hide all views
        for (auto* view : views_) {
            if (view) view->add_class("hidden");
        }
        // Hide all nav active states
        for (auto* nav : nav_buttons_) {
            if (nav) nav->remove_class("active");
        }

        // Show selected
        if (index >= 0 && index < 4) {
            if (views_[index]) views_[index]->remove_class("hidden");
            if (nav_buttons_[index]) nav_buttons_[index]->add_class("active");
        }

        box_->invalidate();
    }


    // Accessors for controller
    Element* nav_button(int i) const { return nav_buttons_[i]; }
    Box* box() const { return box_; }

private:
    Box* box_;
    Element* root_ = nullptr;

    // Navigation
    Element* nav_buttons_[4] = {};
    Element* views_[4] = {};
    Element* title_label_ = nullptr;

    // Widgets
    Element* progress_elem_ = nullptr;
    Element* toast_elem_ = nullptr;
    Element* modal_elem_ = nullptr;

    // Tab pages
    Element* tab_pages_[3] = {};
    Element* tabs_widget_elem_ = nullptr;

    void build_sidebar() {
        auto* sidebar = box_->create("div", "sidebar");
        root_->append(sidebar);

        // Brand
        auto* brand = box_->create_widget<LabelWidget>("label", "brand", "flexUI");
        brand->add_class("brand");
        sidebar->append(brand);

        // Nav Items
        const char* nav_labels[] = {"Components", "Calendar", "Data Table", "Tabs"};
        for (int i = 0; i < 4; ++i) {
            nav_buttons_[i] = box_->create_widget<ButtonWidget>("button", "", nav_labels[i]);
            nav_buttons_[i]->add_class("nav-item");
            if (i == 0) nav_buttons_[i]->add_class("active");
            sidebar->append(nav_buttons_[i]);
        }

        // Divider
        auto* div_nav = box_->create_widget<DividerWidget>("divider", "", DividerWidget::Orientation::Horizontal);
        sidebar->append(div_nav);

        // Version
        auto* ver = box_->create_widget<LabelWidget>("label", "", "v2.0.0-alpha");
        ver->add_class("card-title");
        sidebar->append(ver);
    }

    void build_main_content() {
        auto* main = box_->create("div", "main-content");
        root_->append(main);

        // Title
        title_label_ = box_->create_widget<LabelWidget>("label", "", "Component Gallery");
        title_label_->add_class("brand");
        main->append(title_label_);

        // Build views
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

        // Column 1
        auto* col1 = box_->create("div", "col1");
        col1->add_class("col");
        view->append(col1);

        // Buttons Card
        {
            auto* card = box_->create("div", "");
            card->add_class("card");
            col1->append(card);

            auto* head = box_->create_widget<LabelWidget>("label", "", "Buttons");
            head->add_class("card-title");
            card->append(head);

            auto* row = box_->create("div", "");
            row->add_class("card-row");
            card->append(row);

            row->append(box_->create_widget<ButtonWidget>("button", "", "Primary"));

            auto* b2 = box_->create_widget<ButtonWidget>("button", "", "Success");
            b2->add_class("success");
            row->append(b2);

            auto* b3 = box_->create_widget<ButtonWidget>("button", "", "Danger");
            b3->add_class("danger");
            row->append(b3);
        }

        // Form Controls Card
        {
            auto* card = box_->create("div", "");
            card->add_class("card");
            col1->append(card);

            auto* head = box_->create_widget<LabelWidget>("label", "", "Form Controls");
            head->add_class("card-title");
            card->append(head);

            auto* r1 = box_->create("div", "");
            r1->add_class("card-row");
            card->append(r1);
            r1->append(box_->create_widget<InputWidget>("input", "", "Email address...", false));

            auto* r2 = box_->create("div", "");
            r2->add_class("card-row");
            card->append(r2);
            r2->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Remember me", true));
            r2->append(box_->create_widget<SwitchWidget>("switch", "", "Wifi", true));

            // Stepper
            auto* r3 = box_->create("div", "");
            r3->add_class("card-row");
            card->append(r3);
            r3->append(box_->create_widget<LabelWidget>("label", "", "Quantity:"));
            auto* stepper = box_->create_widget<StepperWidget>("stepper", "", 1, 0, 99, 1);
            r3->append(stepper);
        }

        // Progress Card
        {
            auto* card = box_->create("div", "");
            card->add_class("card");
            col1->append(card);

            auto* head = box_->create_widget<LabelWidget>("label", "", "Progress & Valuators");
            head->add_class("card-title");
            card->append(head);

            card->append(box_->create_widget<SliderWidget>("slider", "", 0.0f, 100.0f, 60.0f));

            progress_elem_ = box_->create_widget<ProgressBarWidget>("progressbar", "prog1", 0.0f, false);
            card->append(progress_elem_);

            auto* r3 = box_->create("div", "");
            r3->add_class("card-row");
            card->append(r3);

            r3->append(box_->create_widget<SpinnerWidget>("spinner", "", SpinnerWidget::Variant::Ring));

            auto* badge = box_->create_widget<BadgeWidget>("badge", "", "");
            static_cast<BadgeWidget*>(badge->widget)->set_count(99);
            r3->append(badge);
        }

        // Column 2
        auto* col2 = box_->create("div", "col2");
        col2->add_class("col");
        view->append(col2);

        // Data & Trees Card
        {
            auto* card = box_->create("div", "");
            card->add_class("card");
            col2->append(card);

            auto* head = box_->create_widget<LabelWidget>("label", "", "Data & Trees");
            head->add_class("card-title");
            card->append(head);

            auto* tree = box_->create_widget<TreeWidget>("tree", "");
            auto* tw = static_cast<TreeWidget*>(tree->widget);
            auto r = tw->add_node("r", "Project Root");
            tw->add_node("src", "Source", r.get());
            tw->add_node("inc", "Include", r.get());
            tw->expand("r");
            card->append(tree);

            auto* dropdown = box_->create_widget<DropdownWidget>("dropdown", "", "Select Branch...");
            auto* dw = static_cast<DropdownWidget*>(dropdown->widget);
            dw->add_option("main", "main");
            dw->add_option("develop", "develop");
            dw->add_option("feature/layout", "feature/layout");
            dw->add_option("bugfix/styling", "bugfix/styling");
            card->append(dropdown);
        }

        // Toggle Group & Avatar Card
        {
            auto* card = box_->create("div", "");
            card->add_class("card");
            col2->append(card);

            auto* head = box_->create_widget<LabelWidget>("label", "", "Toggle & Avatar");
            head->add_class("card-title");
            card->append(head);

            // Toggle Group
            auto* toggle = box_->create_widget<ToggleGroupWidget>("toggle-group", "");
            auto* tg = static_cast<ToggleGroupWidget*>(toggle->widget);
            tg->set_options({
                {"day", "Day"},
                {"week", "Week"},
                {"month", "Month"}
            });
            card->append(toggle);

            // Avatars row
            auto* avatar_row = box_->create("div", "");
            avatar_row->add_class("card-row");
            card->append(avatar_row);

            auto* av1 = box_->create_widget<AvatarWidget>("avatar", "", "Alice Chen", "");
            static_cast<AvatarWidget*>(av1->widget)->set_status(AvatarWidget::Status::Online);
            avatar_row->append(av1);

            auto* av2 = box_->create_widget<AvatarWidget>("avatar", "", "Bob Smith", "");
            static_cast<AvatarWidget*>(av2->widget)->set_status(AvatarWidget::Status::Away);
            avatar_row->append(av2);

            auto* av3 = box_->create_widget<AvatarWidget>("avatar", "", "Carol W", "");
            static_cast<AvatarWidget*>(av3->widget)->set_status(AvatarWidget::Status::Busy);
            avatar_row->append(av3);
        }

        return view;
    }

    Element* build_calendar_view() {
        auto* view = box_->create("div", "calendar-view");
        view->add_class("gallery-grid");

        auto* card = box_->create("div", "");
        card->add_class("card");
        view->append(card);

        auto* head = box_->create_widget<LabelWidget>("label", "", "Date Picker");
        head->add_class("card-title");
        card->append(head);

        card->append(box_->create_widget<CalendarWidget>("calendar", "cal1"));

        return view;
    }

    Element* build_data_view() {
        auto* view = box_->create("div", "data-view");
        view->add_class("gallery-grid");

        auto* card = box_->create("div", "");
        card->add_class("card");
        card->add_class("wide-card");
        // card->computed_style->width = 800.0f; // Removed manual override
        view->append(card);

        auto* head = box_->create_widget<LabelWidget>("label", "", "Employee Data");
        head->add_class("card-title");
        card->append(head);

        auto* table = box_->create_widget<TableWidget>("table", "table1");
        auto* tw = static_cast<TableWidget*>(table->widget);

        tw->add_column("ID", 60);
        tw->add_column("Name", 150);
        tw->add_column("Role", 150);
        tw->add_column("Status", 100);

        tw->add_row({"1", "Alice Chen", "Engineer", "Active"});
        tw->add_row({"2", "Bob Smith", "Designer", "Active"});
        tw->add_row({"3", "Carol White", "Manager", "Away"});
        tw->add_row({"4", "David Lee", "Developer", "Active"});
        tw->add_row({"5", "Eve Johnson", "Analyst", "Offline"});

        card->append(table);

        return view;
    }

    Element* build_tabs_view() {
        auto* view = box_->create("div", "tabs-view");
        view->add_class("tabs-view");

        // Tab content container - all pages overlap here
        auto* tab_content = box_->create("div", "tab-content");
        tab_content->add_class("tab-content");

        // Page 1: Profile
        tab_pages_[0] = box_->create("div", "page-profile");
        tab_pages_[0]->add_class("tab-page");
        tab_content->append(tab_pages_[0]);
        {
            auto* h = box_->create_widget<LabelWidget>("label", "", "User Profile");
            h->add_class("card-title");
            tab_pages_[0]->append(h);

            auto* row1 = box_->create("div", "");
            row1->add_class("card-row");
            row1->append(box_->create_widget<LabelWidget>("label", "", "Username:"));
            row1->append(box_->create_widget<InputWidget>("input", "u1", "admin"));
            tab_pages_[0]->append(row1);

            auto* row2 = box_->create("div", "");
            row2->add_class("card-row");
            row2->append(box_->create_widget<LabelWidget>("label", "", "Email:"));
            row2->append(box_->create_widget<InputWidget>("input", "e1", "admin@example.com"));
            tab_pages_[0]->append(row2);

            tab_pages_[0]->append(box_->create_widget<ButtonWidget>("button", "save", "Save Changes"));
        }

        // Page 2: Settings
        tab_pages_[1] = box_->create("div", "page-settings");
        tab_pages_[1]->add_class("tab-page");
        tab_content->append(tab_pages_[1]);
        {
            auto* h = box_->create_widget<LabelWidget>("label", "", "Application Settings");
            h->add_class("card-title");
            tab_pages_[1]->append(h);

            auto* row1 = box_->create("div", "");
            row1->add_class("card-row");
            row1->append(box_->create_widget<LabelWidget>("label", "", "Notifications"));
            row1->append(box_->create_widget<SwitchWidget>("switch", "s1", "", true));
            tab_pages_[1]->append(row1);

            auto* row2 = box_->create("div", "");
            row2->add_class("card-row");
            row2->append(box_->create_widget<LabelWidget>("label", "", "Dark Mode"));
            row2->append(box_->create_widget<SwitchWidget>("switch", "s2", "", true));
            tab_pages_[1]->append(row2);

            auto* row3 = box_->create("div", "");
            row3->add_class("card-row");
            row3->append(box_->create_widget<LabelWidget>("label", "", "Volume"));
            row3->append(box_->create_widget<SliderWidget>("slider", "vol", 0.7f));
            tab_pages_[1]->append(row3);
        }

        // Page 3: Navigation
        tab_pages_[2] = box_->create("div", "page-navigation");
        tab_pages_[2]->add_class("tab-page");
        tab_content->append(tab_pages_[2]);
        {
            auto* h = box_->create_widget<LabelWidget>("label", "", "Navigation Components");
            h->add_class("card-title");
            tab_pages_[2]->append(h);

            // Breadcrumb
            auto* bc = box_->create_widget<BreadcrumbWidget>("breadcrumb", "");
            auto* bw = static_cast<BreadcrumbWidget*>(bc->widget);
            bw->set_items({
                {"home", "Home"},
                {"products", "Products"},
                {"electronics", "Electronics"},
                {"phones", "Phones"}
            });
            tab_pages_[2]->append(bc);

            // Pagination
            auto* pg = box_->create_widget<PaginationWidget>("pagination", "", 10, 1);
            tab_pages_[2]->append(pg);

            // Stepper
            auto* stepper_row = box_->create("div", "");
            stepper_row->add_class("card-row");
            stepper_row->append(box_->create_widget<LabelWidget>("label", "", "Page Size:"));
            stepper_row->append(box_->create_widget<StepperWidget>("stepper", "", 10, 5, 50, 5));
            tab_pages_[2]->append(stepper_row);
        }

        // Main tabs - add with page references
        tabs_widget_elem_ = box_->create_widget<TabsWidget>("tabs", "main-tabs");
        auto* tw = static_cast<TabsWidget*>(tabs_widget_elem_->widget);
        tw->add_tab("Profile", "profile", tab_pages_[0]);
        tw->add_tab("Settings", "settings", tab_pages_[1]);
        tw->add_tab("Navigation", "navigation", tab_pages_[2]);
        view->append(tabs_widget_elem_);

        view->append(tab_content);

        return view;
    }

    void build_overlays() {
        toast_elem_ = box_->create_widget<ToastWidget>("toast", "toast1", "Welcome to flexUI!", ToastWidget::Type::Success);
        root_->append(toast_elem_);

        modal_elem_ = box_->create_widget<ModalWidget>("modal", "modal1", "System Status");
        root_->append(modal_elem_);
    }
};

// ============================================================================
// Controller - Input Handling and Logic
// ============================================================================

class DemoController {
public:
    DemoController(DemoModel& model, DemoView& view)
        : model_(model), view_(view) {}

    void setup_bindings() {
        auto* box = view_.box();

        // Nav button clicks
        box->set_event_callback([this](Element& elem, const Event& event) {
            if (event.type != EventType::MouseUp) return;

            for (int i = 0; i < 4; ++i) {
                if (&elem == view_.nav_button(i)) {
                    model_.current_view = i;
                    view_.switch_view(i);
                    return;
                }
            }
        });

        // Tab widgets callbacks need to be set up via widget API
        // This is done in view build since widgets handle their own events
    }

    void update(float delta_ms) {
        // Animate progress
        model_.progress_value += 0.0005f * delta_ms;
        if (model_.progress_value > 1.0f) {
            model_.progress_value = 0.0f;
        }

        // Update view from model
        view_.update(model_);
    }

    void handle_key(SDL_Keycode key) {
        switch (key) {
            case SDLK_1: model_.current_view = 0; view_.switch_view(0); break;
            case SDLK_2: model_.current_view = 1; view_.switch_view(1); break;
            case SDLK_3: model_.current_view = 2; view_.switch_view(2); break;
            case SDLK_4: model_.current_view = 3; view_.switch_view(3); break;
        }
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
        std::cout << "=== flexUI Visual Demo (MVC) ===" << std::endl;

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
            return false;
        }

        window_ = SDL_CreateWindow(
            "flexUI Visual Demo - MVC Architecture",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH, WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN
        );

        if (!window_) {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            return false;
        }

        surface_ = SDL_GetWindowSurface(window_);

        if (tvg::Initializer::init(0) != tvg::Result::Success) {
            std::cerr << "ThorVG init failed" << std::endl;
            return false;
        }

        if (!load_font("Arial", "C:/Windows/Fonts/arial.ttf")) {
            return false;
        }

        canvas_.reset(tvg::SwCanvas::gen());
        canvas_->target(
            static_cast<uint32_t*>(surface_->pixels),
            surface_->w,
            surface_->pitch / 4,
            surface_->h,
            tvg::ColorSpace::ARGB8888
        );

        flex_renderer_ = flex::create_thorvg_renderer(canvas_.get());
        if (!flex_renderer_) {
            std::cerr << "Failed to create flex::Renderer" << std::endl;
            return false;
        }

        box_ = std::make_unique<Box>(flex_renderer_.get());
        box_->set_viewport(WINDOW_WIDTH, WINDOW_HEIGHT);

        // Initialize MVC
        model_ = std::make_unique<DemoModel>();
        view_ = std::make_unique<DemoView>(box_.get());
        view_->build();

        controller_ = std::make_unique<DemoController>(*model_, *view_);
        controller_->setup_bindings();

        // Initial render
        box_->update();
        SDL_UpdateWindowSurface(window_);

        SDL_StartTextInput();

        std::cout << "\nDemo ready! Keys: 1-4 to switch views, ESC to quit" << std::endl;

        return true;
    }

    void run() {
        running_ = true;
        Uint32 last_time = SDL_GetTicks();

        while (running_) {
            Uint32 current_time = SDL_GetTicks();
            float delta_ms = static_cast<float>(current_time - last_time);
            last_time = current_time;

            handle_events();
            controller_->update(delta_ms);
            box_->update_time(delta_ms);
            box_->update();
            SDL_UpdateWindowSurface(window_);

            SDL_Delay(16);
        }
    }

    ~VisualDemo() {
        SDL_StopTextInput();
        controller_.reset();
        view_.reset();
        model_.reset();
        box_.reset();
        flex_renderer_.reset();
        canvas_.reset();
        tvg::Initializer::term();
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::unique_ptr<flex::Renderer> flex_renderer_;
    std::unique_ptr<Box> box_;
    bool running_ = false;

    // MVC components
    std::unique_ptr<DemoModel> model_;
    std::unique_ptr<DemoView> view_;
    std::unique_ptr<DemoController> controller_;

    bool load_font(const char* name, const char* path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open " << path << std::endl;
            return false;
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            std::cerr << "Failed to read " << path << std::endl;
            return false;
        }

        if (tvg::Text::load(name, buffer.data(), static_cast<uint32_t>(size), "ttf", true) != tvg::Result::Success) {
            std::cerr << "Failed to load " << name << " font" << std::endl;
            return false;
        }

        std::cout << "Font loaded: " << name << std::endl;
        return true;
    }

    // SDL event conversion helpers
    KeyCode sdl_to_keycode(SDL_Keycode sdl_key) {
        switch (sdl_key) {
            case SDLK_LEFT:      return KeyCode::Left;
            case SDLK_RIGHT:     return KeyCode::Right;
            case SDLK_UP:        return KeyCode::Up;
            case SDLK_DOWN:      return KeyCode::Down;
            case SDLK_HOME:      return KeyCode::Home;
            case SDLK_END:       return KeyCode::End;
            case SDLK_BACKSPACE: return KeyCode::Backspace;
            case SDLK_DELETE:    return KeyCode::Delete;
            case SDLK_RETURN:    return KeyCode::Enter;
            case SDLK_TAB:       return KeyCode::Tab;
            case SDLK_ESCAPE:    return KeyCode::Escape;
            case SDLK_a:         return KeyCode::A;
            case SDLK_c:         return KeyCode::C;
            case SDLK_v:         return KeyCode::V;
            case SDLK_x:         return KeyCode::X;
            case SDLK_z:         return KeyCode::Z;
            default:             return KeyCode::Unknown;
        }
    }

    int sdl_to_mods(Uint16 sdl_mods) {
        int mods = 0;
        if (sdl_mods & KMOD_SHIFT) mods |= static_cast<int>(KeyMod::Shift);
        if (sdl_mods & KMOD_CTRL)  mods |= static_cast<int>(KeyMod::Control);
        if (sdl_mods & KMOD_ALT)   mods |= static_cast<int>(KeyMod::Alt);
        if (sdl_mods & KMOD_GUI)   mods |= static_cast<int>(KeyMod::Super);
        return mods;
    }

    MouseButton sdl_to_button(Uint8 sdl_button) {
        switch (sdl_button) {
            case SDL_BUTTON_LEFT:   return MouseButton::Left;
            case SDL_BUTTON_RIGHT:  return MouseButton::Right;
            case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
            default:                return MouseButton::Left;
        }
    }

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_MOUSEMOTION: {
                    auto e = Event::mouse_move(
                        static_cast<float>(event.motion.x),
                        static_cast<float>(event.motion.y)
                    );
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_MOUSEBUTTONDOWN: {
                    auto e = Event::mouse_down(
                        static_cast<float>(event.button.x),
                        static_cast<float>(event.button.y),
                        sdl_to_button(event.button.button)
                    );
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_MOUSEBUTTONUP: {
                    auto e = Event::mouse_up(
                        static_cast<float>(event.button.x),
                        static_cast<float>(event.button.y),
                        sdl_to_button(event.button.button)
                    );
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_MOUSEWHEEL: {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    auto e = Event::mouse_wheel(
                        static_cast<float>(mx),
                        static_cast<float>(my),
                        static_cast<float>(event.wheel.x),
                        static_cast<float>(event.wheel.y)
                    );
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_KEYDOWN: {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running_ = false;
                    } else {
                        controller_->handle_key(event.key.keysym.sym);
                    }
                    auto e = Event::key_down(
                        sdl_to_keycode(event.key.keysym.sym),
                        sdl_to_mods(event.key.keysym.mod)
                    );
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_KEYUP: {
                    auto e = Event::key_up(
                        sdl_to_keycode(event.key.keysym.sym),
                        sdl_to_mods(event.key.keysym.mod)
                    );
                    box_->dispatch_event(e);
                    break;
                }

                case SDL_TEXTINPUT: {
                    auto e = Event::text_input(event.text.text);
                    box_->dispatch_event(e);
                    break;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    VisualDemo demo;

    if (!demo.init()) {
        std::cerr << "Failed to initialize demo" << std::endl;
        return 1;
    }

    demo.run();

    return 0;
}
