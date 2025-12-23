/*
 * tvgbox2 - Visual Demo with ThorVG Software Rendering
 *
 * A real window demo showcasing widgets with ThorVG + SDL2
 */

#include <SDL2/SDL.h>
#include <thorvg.h>
#include <tvgbox2/box.h>
#include <tvgbox2/element.h>
#include <tvgbox2/widgets/button_widget.h>
#include <tvgbox2/widgets/label_widget.h>
#include <tvgbox2/widgets/input_widget.h>
#include <tvgbox2/widgets/checkbox_widget.h>
#include <tvgbox2/widgets/slider_widget.h>
#include <tvgbox2/widgets/progressbar_widget.h>
#include <tvgbox2/widgets/switch_widget.h>
// New widgets
#include <tvgbox2/widgets/badge_widget.h>
#include <tvgbox2/widgets/spinner_widget.h>
#include <tvgbox2/widgets/divider_widget.h>
#include <tvgbox2/widgets/toast_widget.h>
#include <tvgbox2/widgets/dropdown_widget.h>
#include <tvgbox2/widgets/tabs_widget.h>
#include <tvgbox2/widgets/accordion_widget.h>
#include <tvgbox2/widgets/modal_widget.h>
#include <tvgbox2/widgets/calendar_widget.h>
#include <tvgbox2/widgets/table_widget.h>
#include <tvgbox2/widgets/tree_widget.h>
#include <tvgbox2/widgets/colorpicker_widget.h>
#include <tvgbox2/widgets/tooltip_widget.h>
#include <tvgbox2/widgets/image_widget.h>
#include <tvgbox2/widgets/select_widget.h> // Added missing header
#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include "demo_styles.h"

using namespace tvgbox2;

constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 900;

class VisualDemo {
public:
    bool init() {
        std::cout << "=== tvgbox2 Visual Demo ===" << std::endl;
        std::cout << "Initializing SDL2 + ThorVG Software Renderer..." << std::endl;

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
            return false;
        }

        window_ = SDL_CreateWindow(
            "tvgbox2 Visual Demo - ThorVG Software Rendering",
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

        // Load fonts
        std::cout << "Loading fonts..." << std::endl;

        // Load Arial font with custom name (ThorVG's load(path) uses path as font name)
        // We need to load from memory to specify "Arial" as the font name
        {
            std::ifstream file("C:/Windows/Fonts/arial.ttf", std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                std::cerr << "Failed to open arial.ttf" << std::endl;
                return false;
            }

            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size);
            if (!file.read(buffer.data(), size)) {
                std::cerr << "Failed to read arial.ttf" << std::endl;
                return false;
            }

            if (tvg::Text::load("Arial", buffer.data(), static_cast<uint32_t>(size), "ttf", true) != tvg::Result::Success) {
                std::cerr << "Failed to load Arial font" << std::endl;
                return false;
            }
            std::cout << "  Arial: OK" << std::endl;
        }

        std::cout << "Fonts loaded" << std::endl;

        canvas_.reset(tvg::SwCanvas::gen());
        canvas_->target(
            static_cast<uint32_t*>(surface_->pixels),
            surface_->w,
            surface_->pitch / 4,
            surface_->h,
            tvg::ColorSpace::ARGB8888
        );

        std::cout << "✓ SDL2 window created (" << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << ")" << std::endl;
        std::cout << "✓ ThorVG SwCanvas initialized" << std::endl;

        box_ = std::make_unique<Box>(canvas_.get());
        box_->set_viewport(WINDOW_WIDTH, WINDOW_HEIGHT);

        build_ui();

        // 第一次渲染
        std::cout << "\n>>> Performing initial render..." << std::endl;
        box_->update();
        SDL_UpdateWindowSurface(window_);
        std::cout << ">>> Initial render complete!" << std::endl;

        // Enable text input for InputWidget
        SDL_StartTextInput();

        std::cout << "\nDemo ready! Try:" << std::endl;
        std::cout << "  - Click buttons" << std::endl;
        std::cout << "  - Drag slider" << std::endl;
        std::cout << "  - Toggle switch and checkbox" << std::endl;
        std::cout << "  - Click input field and type" << std::endl;

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
            update(delta_ms);
            render();

            SDL_Delay(16);  // ~60 FPS
        }
    }

    ~VisualDemo() {
        SDL_StopTextInput();
        box_.reset();
        canvas_.reset();
        tvg::Initializer::term();
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();

        std::cout << "\n✓ Demo closed successfully" << std::endl;
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::unique_ptr<Box> box_;
    bool running_ = false;

    Element* progress_elem_ = nullptr;
    Element* toast_elem_ = nullptr;
    Element* modal_elem_ = nullptr;
    float progress_value_ = 0.0f;

    // View containers
    Element* components_view_ = nullptr;
    Element* calendar_view_ = nullptr;
    Element* data_view_ = nullptr;
    Element* title_label_ = nullptr;
    
    // Navigation buttons for click handling
    Element* nav_components_ = nullptr;
    Element* nav_calendar_ = nullptr;
    Element* nav_data_ = nullptr;
    Element* nav_tabs_ = nullptr;
    
    int current_view_ = 0;  // 0=Components, 1=Calendar, 2=Data, 3=Tabs

    // Tabs content pages
    Element* tabs_view_ = nullptr;
    Element* tab_page1_ = nullptr;
    Element* tab_page2_ = nullptr;
    Element* tab_page3_ = nullptr;
    Element* nest_page_a_ = nullptr;
    Element* nest_page_b_ = nullptr;

    void switch_view(int view_index) {
        current_view_ = view_index;
        
        // Hide all views using CSS class
        if (components_view_) components_view_->add_class("hidden");
        if (calendar_view_) calendar_view_->add_class("hidden");
        if (data_view_) data_view_->add_class("hidden");
        if (tabs_view_) tabs_view_->add_class("hidden");
        
        // Update nav button styles
        if (nav_components_) { nav_components_->remove_class("active"); }
        if (nav_calendar_) { nav_calendar_->remove_class("active"); }
        if (nav_data_) { nav_data_->remove_class("active"); }
        if (nav_tabs_) { nav_tabs_->remove_class("active"); }
        
        // Show selected view
        switch (view_index) {
            case 0:
                if (components_view_) components_view_->remove_class("hidden");
                if (nav_components_) nav_components_->add_class("active");
                if (title_label_) static_cast<LabelWidget*>(title_label_->widget)->set_text("Component Gallery");
                break;
            case 1:
                if (calendar_view_) calendar_view_->remove_class("hidden");
                if (nav_calendar_) nav_calendar_->add_class("active");
                if (title_label_) static_cast<LabelWidget*>(title_label_->widget)->set_text("Calendar & Date");
                break;
            case 2:
                if (data_view_) data_view_->remove_class("hidden");
                if (nav_data_) nav_data_->add_class("active");
                if (title_label_) static_cast<LabelWidget*>(title_label_->widget)->set_text("Data Display");
                break;
            case 3:
                if (tabs_view_) tabs_view_->remove_class("hidden");
                if (nav_tabs_) nav_tabs_->add_class("active");
                if (title_label_) static_cast<LabelWidget*>(title_label_->widget)->set_text("Tab Navigation");
                break;
        }
        
        box_->invalidate();
    }

    void build_ui() {
        std::cout << "\nBuilding UI..." << std::endl;

        try {
            build_ui_internal();
        } catch (const std::exception& e) {
            std::cerr << "ERROR building UI: " << e.what() << std::endl;
            throw;
        }
    }

    void build_ui_internal() {
        // Load CSS from header
        box_->load_css(examples::DEMO_CSS);

        // Root container (Flex Row)
        auto* root = box_->create("div", "root");
        box_->set_root(root);

        // ==========================================
        // LEFT: Sidebar
        // ==========================================
        auto* sidebar = box_->create("div", "sidebar");
        root->append(sidebar);

        // Brand
        auto* brand = box_->create_widget<LabelWidget>("label", "brand", "tvgbox2");
        brand->add_class("brand");
        sidebar->append(brand);

        // Nav Items with click handlers
        nav_components_ = box_->create_widget<ButtonWidget>("button", "", "Components");
        nav_components_->add_class("nav-item");
        nav_components_->add_class("active");
        sidebar->append(nav_components_);
        
        nav_calendar_ = box_->create_widget<ButtonWidget>("button", "", "Calendar");
        nav_calendar_->add_class("nav-item");
        sidebar->append(nav_calendar_);
        
        nav_data_ = box_->create_widget<ButtonWidget>("button", "", "Data Table");
        nav_data_->add_class("nav-item");
        sidebar->append(nav_data_);

        nav_tabs_ = box_->create_widget<ButtonWidget>("button", "", "Tabs");
        nav_tabs_->add_class("nav-item");
        sidebar->append(nav_tabs_);

        // Spacer / Divider
        auto* div_nav = box_->create_widget<DividerWidget>("divider", "", DividerWidget::Orientation::Horizontal);
        sidebar->append(div_nav);

        auto* ver = box_->create_widget<LabelWidget>("label", "", "v2.0.0-alpha");
        ver->add_class("card-title"); // reuse muted text style
        sidebar->append(ver);


        // ==========================================
        // RIGHT: Main Content
        // ==========================================
        auto* main = box_->create("div", "main-content");
        root->append(main);

        // Header Title (stored for dynamic update)
        title_label_ = box_->create_widget<LabelWidget>("label", "", "Component Gallery");
        title_label_->add_class("brand");
        main->append(title_label_);

        // ==========================================
        // VIEW 1: Components (default visible)
        // ==========================================
        components_view_ = box_->create("div", "components-view");
        components_view_->add_class("gallery-grid");
        main->append(components_view_);

        // --- COL 1 ---
        auto* col1 = box_->create("div", "col1");
        col1->add_class("col");
        components_view_->append(col1);

        // CARD: Buttons
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

        // CARD: Inputs
        {
            auto* card = box_->create("div", "");
            card->add_class("card");
            col1->append(card);

            auto* head = box_->create_widget<LabelWidget>("label", "", "Form Controls");
            head->add_class("card-title");
            card->append(head);

            // Input
            auto* r1 = box_->create("div", "");
            r1->add_class("card-row");
            card->append(r1);
            r1->append(box_->create_widget<InputWidget>("input", "", "Email address...", false));

            // Checkbox & Switch
            auto* r2 = box_->create("div", "");
            r2->add_class("card-row");
            card->append(r2);
            r2->append(box_->create_widget<CheckboxWidget>("checkbox", "", "Remember me", true));
            r2->append(box_->create_widget<SwitchWidget>("switch", "", "Wifi", true));
        }
        
        // CARD: Progress & Sliders
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

            // Badge & Spinner
            auto* r3 = box_->create("div", "");
            r3->add_class("card-row");
            card->append(r3);
            
            auto* spin = box_->create_widget<SpinnerWidget>("spinner", "", SpinnerWidget::Variant::Ring);
            r3->append(spin);
            
            auto* badge = box_->create_widget<BadgeWidget>("badge", "", "");
            static_cast<BadgeWidget*>(badge->widget)->set_count(99);
            r3->append(badge);
        }

        // --- COL 2 ---
        auto* col2 = box_->create("div", "col2");
        col2->add_class("col");
        components_view_->append(col2);

        // CARD: Data Display
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
            
            // Dropdown
             auto* dropdown = box_->create_widget<DropdownWidget>("dropdown", "", "Select Branch...");
             auto* dw = static_cast<DropdownWidget*>(dropdown->widget);
             dw->add_option("main", "main");
             dw->add_option("develop", "develop");
             dw->add_option("feature/layout", "feature/layout");
             dw->add_option("bugfix/styling", "bugfix/styling");
             card->append(dropdown);
        }
        


        // ==========================================
        // VIEW 2: Calendar (initially hidden)
        // ==========================================
        calendar_view_ = box_->create("div", "calendar-view");
        calendar_view_->add_class("gallery-grid");
        calendar_view_->add_class("hidden");  // Hidden initially
        main->append(calendar_view_);
        
        {
            // Calendar Card
            auto* card = box_->create("div", "");
            card->add_class("card");
            calendar_view_->append(card);
            
            auto* head = box_->create_widget<LabelWidget>("label", "", "Date Picker");
            head->add_class("card-title");
            card->append(head);
            
            auto* cal = box_->create_widget<CalendarWidget>("calendar", "cal1");
            card->append(cal);
        }

        // ==========================================
        // VIEW 3: Data Table (initially hidden)
        // ==========================================
        data_view_ = box_->create("div", "data-view");
        data_view_->add_class("gallery-grid");
        data_view_->add_class("hidden");  // Hidden initially
        main->append(data_view_);
        
        {
            // Table Card
            auto* card = box_->create("div", "");
            card->add_class("card");
            card->computed_style->width = 800.0f;  // Wider for table
            data_view_->append(card);
            
            auto* head = box_->create_widget<LabelWidget>("label", "", "Employee Data");
            head->add_class("card-title");
            card->append(head);
            
            auto* table = box_->create_widget<TableWidget>("table", "table1");
            auto* tw = static_cast<TableWidget*>(table->widget);
            
            // Add columns
            tw->add_column("ID", 60);
            tw->add_column("Name", 150);
            tw->add_column("Role", 150);
            tw->add_column("Status", 100);
            
            // Add sample data
            tw->add_row({"1", "Alice Chen", "Engineer", "Active"});
            tw->add_row({"2", "Bob Smith", "Designer", "Active"});
            tw->add_row({"3", "Carol White", "Manager", "Away"});
            tw->add_row({"4", "David Lee", "Developer", "Active"});
            tw->add_row({"5", "Eve Johnson", "Analyst", "Offline"});
            
            card->append(table);
        }

        // ==========================================
        // VIEW 4: Tabs (initially hidden)
        // ==========================================
        tabs_view_ = box_->create("div", "tabs-view");
        tabs_view_->add_class("hidden"); 
        tabs_view_->add_class("tabs-view");
        // tabs_view_ needs to be column layout for this
        // styles handled by .tabs-view class now
        main->append(tabs_view_);

        // Tabs Widget
        auto* tabs_widget = box_->create_widget<TabsWidget>("tabs", "main-tabs");
        auto* tw = static_cast<TabsWidget*>(tabs_widget->widget);
        tw->add_tab("Profile", "profile");
        tw->add_tab("Settings", "settings");
        tw->add_tab("Color & Controls", "demos");
        
        // Add callback to switch content pages
        tw->set_change_callback([this](int index, const std::string& id) {
            if (tab_page1_) tab_page1_->add_class("hidden");
            if (tab_page2_) tab_page2_->add_class("hidden");
            if (tab_page3_) tab_page3_->add_class("hidden");
            
            if (index == 0 && tab_page1_) tab_page1_->remove_class("hidden");
            if (index == 1 && tab_page2_) tab_page2_->remove_class("hidden");
            if (index == 2 && tab_page3_) tab_page3_->remove_class("hidden");
            
            box_->invalidate();
        });
        
        tabs_view_->append(tabs_widget);

        // --- PAGE 1: Profile ---
        tab_page1_ = box_->create("div", "page-profile");
        tab_page1_->add_class("card");
        tabs_view_->append(tab_page1_);
        
        {
            auto* h = box_->create_widget<LabelWidget>("label", "", "User Profile");
            h->add_class("card-title");
            tab_page1_->append(h);
            
            auto* row1 = box_->create("div", ""); row1->add_class("card-row");
            row1->append(box_->create_widget<LabelWidget>("label", "", "Username:"));
            row1->append(box_->create_widget<InputWidget>("input", "u1", "admin"));
            tab_page1_->append(row1);
            
            auto* row2 = box_->create("div", ""); row2->add_class("card-row");
            row2->append(box_->create_widget<LabelWidget>("label", "", "Email:"));
            row2->append(box_->create_widget<InputWidget>("input", "e1", "admin@example.com"));
            tab_page1_->append(row2);
            
            auto* btn = box_->create_widget<ButtonWidget>("button", "save", "Save Changes");
            tab_page1_->append(btn);
        }

        // --- PAGE 2: Settings ---
        tab_page2_ = box_->create("div", "page-settings");
        tab_page2_->add_class("card");
        tab_page2_->add_class("hidden"); // hidden by default
        tabs_view_->append(tab_page2_);
        
        {
            auto* h = box_->create_widget<LabelWidget>("label", "", " Application Settings");
            h->add_class("card-title");
            tab_page2_->append(h);
            
            auto* row1 = box_->create("div", ""); row1->add_class("card-row");
            row1->append(box_->create_widget<LabelWidget>("label", "", "Notifications"));
            row1->append(box_->create_widget<SwitchWidget>("switch", "s1", "", true));
            tab_page2_->append(row1);
            
            auto* row2 = box_->create("div", ""); row2->add_class("card-row");
            row2->append(box_->create_widget<LabelWidget>("label", "", "Dark Mode"));
            row2->append(box_->create_widget<SwitchWidget>("switch", "s2", "", true));
            tab_page2_->append(row2);
            
            auto* row3 = box_->create("div", ""); row3->add_class("card-row");
            row3->append(box_->create_widget<LabelWidget>("label", "", "Volume"));
            auto* slid = box_->create_widget<SliderWidget>("slider", "vol", 0.7f);
            row3->append(slid);
            tab_page2_->append(row3);
        }

        // --- PAGE 3: Demos ---
        tab_page3_ = box_->create("div", "page-demos");
        tab_page3_->add_class("card");
        tab_page3_->add_class("hidden"); // hidden by default
        tabs_view_->append(tab_page3_);
        
        {
             auto* h = box_->create_widget<LabelWidget>("label", "", "Interactive Pages Control");
             h->add_class("card-title");
             tab_page3_->append(h);
             
             // Nested Tabs
             auto* tabs = box_->create_widget<TabsWidget>("tabs", "nested-tabs");
             auto* t_w = static_cast<TabsWidget*>(tabs->widget);
             t_w->add_tab("Page A", "sa");
             t_w->add_tab("Page B", "sb");
             tab_page3_->append(tabs);
             
             nest_page_a_ = box_->create("div", "");
             nest_page_a_->add_class("card-row");
             nest_page_a_->append(box_->create_widget<LabelWidget>("label", "", "This is content for Page A."));
             nest_page_a_->append(box_->create_widget<ButtonWidget>("button", "", "Action A"));
             tab_page3_->append(nest_page_a_);

             nest_page_b_ = box_->create("div", "");
             nest_page_b_->add_class("card-row");
             nest_page_b_->add_class("hidden");
             nest_page_b_->append(box_->create_widget<LabelWidget>("label", "", "This is content for Page B."));
             nest_page_b_->append(box_->create_widget<ButtonWidget>("button", "", "Action B"));
             tab_page3_->append(nest_page_b_);

             t_w->set_change_callback([this](int idx, const std::string& id){
                if (nest_page_a_) nest_page_a_->add_class("hidden");
                if (nest_page_b_) nest_page_b_->add_class("hidden");
                
                if (idx == 0 && nest_page_a_) nest_page_a_->remove_class("hidden");
                if (idx == 1 && nest_page_b_) nest_page_b_->remove_class("hidden");
                box_->invalidate();
             });
             
             // Color Palette
             auto* h2 = box_->create_widget<LabelWidget>("label", "", "Color Palette");
             h2->add_class("card-title");
             tab_page3_->append(h2);

             auto* cp = box_->create_widget<ColorPickerWidget>("colorpicker", "cp-demo");
             tab_page3_->append(cp);
        }
        toast_elem_ = box_->create_widget<ToastWidget>("toast", "toast1", "Welcome to tvgbox2!", ToastWidget::Type::Success);
        root->append(toast_elem_);
        
        modal_elem_ = box_->create_widget<ModalWidget>("modal", "modal1", "System Status");
        root->append(modal_elem_);

        std::cout << "UI built with new Gallery Layout" << std::endl;
        
        // Set up event callback for nav button clicks
        box_->set_event_callback([this](Element& elem, const Event& event) {
            if (event.type == EventType::MouseUp) {
                // Check which nav button was clicked
                if (&elem == nav_components_) {
                    switch_view(0);
                } else if (&elem == nav_calendar_) {
                    switch_view(1);
                } else if (&elem == nav_data_) {
                    switch_view(2);
                } else if (&elem == nav_tabs_) {
                    switch_view(3);
                }
            }
        });
        
        // Initial update
        box_->update();
    }


    // Convert SDL keycode to tvgbox2 KeyCode
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

    // Convert SDL modifier keys to tvgbox2 KeyMod
    int sdl_to_mods(Uint16 sdl_mods) {
        int mods = 0;
        if (sdl_mods & KMOD_SHIFT) mods |= static_cast<int>(KeyMod::Shift);
        if (sdl_mods & KMOD_CTRL)  mods |= static_cast<int>(KeyMod::Control);
        if (sdl_mods & KMOD_ALT)   mods |= static_cast<int>(KeyMod::Alt);
        if (sdl_mods & KMOD_GUI)   mods |= static_cast<int>(KeyMod::Super);
        return mods;
    }

    // Convert SDL mouse button to tvgbox2 MouseButton
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

    void update(float delta_ms) {
        // Update widget animations (cursor blink, ripples, etc.)
        box_->update_time(delta_ms);

        // Render
        box_->update();

        // Animate progress bar demo
        progress_value_ += 0.0005f * delta_ms;
        if (progress_value_ > 1.0f) progress_value_ = 0.0f;

        if (progress_elem_) {
            auto* progress = static_cast<ProgressBarWidget*>(progress_elem_->widget);
            progress->set_value(progress_value_ * 100.0f);  // 0-100 范围
            progress_elem_->mark_paint_dirty();  // 通知 Box 需要重新渲染
        }
    }

    void render() {
        SDL_UpdateWindowSurface(window_);
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
