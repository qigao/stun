/*
    example_m3_dialog.cpp -- M3 Dialog showcase

    Demonstrates all M3Dialog features:
    - Basic dialogs
    - Full-screen dialogs
    - Action buttons (horizontal and vertical layouts)
    - Dismissible and non-dismissible variants
    - Callbacks (onShow and onDismiss)
*/

#include <nanogui/screen.h>
#include <nanogui/window.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/button.h>
#include <nanogui/textbox.h>
#include <nanogui/checkbox.h>
#include <nanogui/slider.h>
#include <nanogui/combobox.h>
#include <nanogui/m3_theme.h>
#include <nanogui/m3_dialog.h>
#include <iostream>

using namespace nanogui;

int main(int /* argc */, char ** /* argv */) {
    nanogui::init();

    {
        // Create main screen
        Screen *screen = new Screen(Vector2i(1000, 700), "M3 Dialog Examples");

        // Create M3 theme with purple seed
        auto *theme = new M3Theme(
            screen->nvg_context(),
            Color(0.4f, 0.2f, 0.8f, 1.0f),  // Purple seed
            M3Theme::Scheme::Light
        );
        screen->set_theme(theme);

        // Main window
        Window *window = new Window(screen, "M3 Dialog Showcase");
        window->set_position(Vector2i(15, 15));
        window->set_layout(new GroupLayout());

        // Theme controls
        new Label(window, "Theme Controls", "sans-bold");
        Widget *theme_controls = new Widget(window);
        theme_controls->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 5, 5));

        Button *light_btn = new Button(theme_controls, "Light");
        light_btn->set_callback([theme, screen]() {
            theme->apply_scheme(M3Theme::Scheme::Light);
            screen->perform_layout();
        });

        Button *dark_btn = new Button(theme_controls, "Dark");
        dark_btn->set_callback([theme, screen]() {
            theme->apply_scheme(M3Theme::Scheme::Dark);
            screen->perform_layout();
        });

        // Section 1: Basic Dialogs
        new Label(window, "1. Basic Dialogs", "sans-bold");
        new Label(window, "Standard centered dialogs with title and content", "sans");

        Widget *basic_panel = new Widget(window);
        basic_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        // Simple dialog with one action
        Button *simple_dialog_btn = new Button(basic_panel, "Simple Dialog");
        simple_dialog_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Welcome!");
            dialog->set_fixed_size(Vector2i(400, 200));
            dialog->set_content_text("This is a simple dialog with just one action button. Click OK to close.");
            dialog->add_action("OK", [dialog]() {
                std::cout << "✓ Simple dialog - OK clicked" << std::endl;
                dialog->hide();
            });
            dialog->show();
        });

        // Dialog with icon
        Button *icon_dialog_btn = new Button(basic_panel, "With Icon");
        icon_dialog_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Delete File?");
            dialog->set_fixed_size(Vector2i(400, 220));
            dialog->set_icon(0xf1f8);  // Trash icon
            dialog->set_content_text("Are you sure you want to delete 'document.txt'? This action cannot be undone.");
            dialog->add_action("Cancel", [dialog]() {
                std::cout << "✓ Icon dialog - Cancel clicked (file NOT deleted)" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Delete", [dialog]() {
                std::cout << "✓ Icon dialog - Delete clicked (file deleted!)" << std::endl;
                dialog->hide();
            });
            dialog->show();
        });

        // Section 2: Action Button Layouts
        new Label(window, "2. Action Button Layouts", "sans-bold");
        new Label(window, "Horizontal (≤2 actions) vs Vertical (>2 actions)", "sans");

        Widget *actions_panel = new Widget(window);
        actions_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        // Horizontal layout (2 actions)
        Button *horizontal_btn = new Button(actions_panel, "2 Actions (Horizontal)");
        horizontal_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Save Changes?");
            dialog->set_fixed_size(Vector2i(400, 220));
            dialog->set_content_text("You have unsaved changes. This dialog has 2 actions arranged HORIZONTALLY at the bottom.");
            dialog->add_action("Discard", [dialog]() {
                std::cout << "✓ Horizontal layout - Discard clicked" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Save", [dialog]() {
                std::cout << "✓ Horizontal layout - Save clicked" << std::endl;
                dialog->hide();
            });
            dialog->show();
        });

        // Vertical layout (3+ actions)
        Button *vertical_btn = new Button(actions_panel, "3+ Actions (Vertical)");
        vertical_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Export Format");
            dialog->set_fixed_size(Vector2i(400, 320));
            dialog->set_content_text("Choose export format. This dialog has 4 actions arranged VERTICALLY (stacked) on the right side.");
            dialog->add_action("Export as PDF", [dialog]() {
                std::cout << "✓ Vertical layout - PDF selected" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Export as PNG", [dialog]() {
                std::cout << "✓ Vertical layout - PNG selected" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Export as SVG", [dialog]() {
                std::cout << "✓ Vertical layout - SVG selected" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Cancel", [dialog]() {
                std::cout << "✓ Vertical layout - Cancelled" << std::endl;
                dialog->hide();
            });
            dialog->show();
        });

        // Section 3: Dismissible vs Non-Dismissible
        new Label(window, "3. Dismissible Variants", "sans-bold");
        new Label(window, "Control whether users can dismiss by clicking scrim or pressing Escape", "sans");

        Widget *dismissible_panel = new Widget(window);
        dismissible_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        // Dismissible dialog (default)
        Button *dismissible_btn = new Button(dismissible_panel, "Dismissible (Try ESC)");
        dismissible_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Info");
            dialog->set_fixed_size(Vector2i(400, 200));
            dialog->set_content_text("This is DISMISSIBLE. Try clicking outside the dialog or pressing Escape to close it!");
            dialog->set_dismissible(true);  // This is the default
            dialog->add_action("Close", [dialog]() {
                std::cout << "✓ Dismissible - Close button clicked" << std::endl;
                dialog->hide();
            });
            dialog->set_on_dismiss([]() {
                std::cout << "✓ Dismissible - Dialog dismissed (ESC or scrim click)" << std::endl;
            });
            dialog->show();
        });

        // Non-dismissible dialog
        Button *non_dismissible_btn = new Button(dismissible_panel, "Non-Dismissible (Forced)");
        non_dismissible_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "⚠ Important Action");
            dialog->set_fixed_size(Vector2i(400, 220));
            dialog->set_content_text("This is NON-DISMISSIBLE. You MUST click a button. Try pressing Escape or clicking outside - it won't work!");
            dialog->set_dismissible(false);
            dialog->add_action("Cancel", [dialog]() {
                std::cout << "✓ Non-dismissible - Cancel clicked" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Proceed", [dialog]() {
                std::cout << "✓ Non-dismissible - Proceed clicked" << std::endl;
                dialog->hide();
            });
            dialog->show();
        });

        // Section 4: Callbacks
        new Label(window, "4. Dialog Callbacks", "sans-bold");
        new Label(window, "onShow and onDismiss callbacks for lifecycle events", "sans");

        Widget *callbacks_panel = new Widget(window);
        callbacks_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *callbacks_btn = new Button(callbacks_panel, "Callbacks (Check Console)");
        callbacks_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Lifecycle Callbacks");
            dialog->set_fixed_size(Vector2i(400, 220));
            dialog->set_content_text("This dialog has onShow and onDismiss callbacks. Check the console output to see when they fire!");
            
            dialog->set_on_show([]() {
                std::cout << "\n=== CALLBACK: onShow fired ===" << std::endl;
                std::cout << "Dialog is now visible" << std::endl;
            });
            
            dialog->set_on_dismiss([]() {
                std::cout << "\n=== CALLBACK: onDismiss fired ===" << std::endl;
                std::cout << "Dialog is being closed" << std::endl;
            });
            
            dialog->add_action("Close", [dialog]() {
                std::cout << "✓ Callbacks - Close button clicked" << std::endl;
                dialog->hide();
            });
            
            dialog->show();
        });

        // Section 5: Full-Screen Dialogs
        new Label(window, "5. Full-Screen Dialogs", "sans-bold");
        new Label(window, "Dialogs that fill the entire screen with slide animations", "sans");

        Widget *fullscreen_panel = new Widget(window);
        fullscreen_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        // Basic full-screen dialog
        Button *fullscreen_btn = new Button(fullscreen_panel, "Full-Screen (Slides In)");
        fullscreen_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Document Viewer", M3Dialog::DialogType::FULLSCREEN);
            
            // Create content with multiple sections
            Widget *content = new Widget(dialog);
            content->set_layout(new GroupLayout());
            
            new Label(content, "📄 Full-Screen Dialog", "sans-bold");
            new Label(content, "This dialog FILLS THE ENTIRE SCREEN and slides in from the right with animation!", "sans");
            new Label(content, "", "sans");  // Spacer
            
            new Label(content, "Features:", "sans-bold");
            new Label(content, "• Slides in from right (300ms animation)", "sans");
            new Label(content, "• Close button (X) in top-left corner", "sans");
            new Label(content, "• Action buttons at bottom", "sans");
            new Label(content, "• Perfect for complex forms", "sans");
            new Label(content, "", "sans");
            
            new Label(content, "Try This:", "sans-bold");
            new Label(content, "1. Click the X button in the top-left", "sans");
            new Label(content, "2. Or click Save/Cancel below", "sans");
            new Label(content, "3. Watch the slide-out animation!", "sans");
            
            dialog->set_content(content);
            
            dialog->add_action("Save", [dialog]() {
                std::cout << "✓ Full-screen - Save clicked" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Cancel", [dialog]() {
                std::cout << "✓ Full-screen - Cancel clicked" << std::endl;
                dialog->hide();
            });
            
            dialog->show();
        });

        // Full-screen with callbacks
        Button *fullscreen_callbacks_btn = new Button(fullscreen_panel, "Full-Screen with Callbacks");
        fullscreen_callbacks_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Edit Profile", M3Dialog::DialogType::FULLSCREEN);
            
            Widget *content = new Widget(dialog);
            content->set_layout(new GroupLayout());
            
            new Label(content, "Profile Settings", "sans-bold");
            new Label(content, "Edit your profile information below.", "sans");
            new Label(content, "", "sans");
            
            // Add some form fields
            new Label(content, "Name:", "sans");
            TextBox *name_field = new TextBox(content, "John Doe");
            name_field->set_fixed_width(300);
            
            new Label(content, "Email:", "sans");
            TextBox *email_field = new TextBox(content, "john@example.com");
            email_field->set_fixed_width(300);
            
            new Label(content, "Bio:", "sans");
            TextBox *bio_field = new TextBox(content, "Software developer");
            bio_field->set_fixed_width(300);
            
            dialog->set_content(content);
            
            dialog->set_on_show([]() {
                std::cout << "✓ Full-screen dialog opened" << std::endl;
            });
            
            dialog->set_on_dismiss([]() {
                std::cout << "✓ Full-screen dialog closed" << std::endl;
            });
            
            dialog->add_action("Save Changes", [dialog]() {
                std::cout << "Profile saved" << std::endl;
                dialog->hide();
            });
            
            dialog->show();
        });

        // Section 6: Complex Content
        new Label(window, "6. Complex Content", "sans-bold");
        new Label(window, "Dialogs with custom widgets and scrollable content", "sans");

        Widget *complex_panel = new Widget(window);
        complex_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *complex_btn = new Button(complex_panel, "Dialog with Widgets");
        complex_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Settings");
            dialog->set_fixed_width(450);
            
            // Create content with various widgets
            Widget *content = new Widget(dialog);
            content->set_layout(new GroupLayout());
            
            new Label(content, "Notification Settings", "sans-bold");
            
            CheckBox *email_check = new CheckBox(content, "Email notifications");
            email_check->set_checked(true);
            
            CheckBox *push_check = new CheckBox(content, "Push notifications");
            push_check->set_checked(false);
            
            new Label(content, "", "sans");  // Spacer
            new Label(content, "Volume", "sans-bold");
            
            Slider *volume_slider = new Slider(content);
            volume_slider->set_value(0.7f);
            volume_slider->set_fixed_width(250);
            
            new Label(content, "", "sans");
            new Label(content, "Theme", "sans-bold");
            
            ComboBox *theme_combo = new ComboBox(content, {"Light", "Dark", "Auto"});
            theme_combo->set_fixed_width(200);
            
            dialog->set_content(content);
            
            dialog->add_action("Cancel", [dialog]() {
                dialog->hide();
            });
            dialog->add_action("Apply", [dialog]() {
                std::cout << "Settings applied" << std::endl;
                dialog->hide();
            });
            
            dialog->center();
            dialog->show();
        });

        // Section 7: All Features Combined
        new Label(window, "7. All Features Combined", "sans-bold");
        new Label(window, "Demonstration of multiple features together", "sans");

        Button *combined_btn = new Button(window, "Show All Features");
        combined_btn->set_callback([screen]() {
            M3Dialog *dialog = new M3Dialog(screen, "Confirm Purchase");
            dialog->set_fixed_width(450);
            dialog->set_icon(0xf07a);  // Shopping cart icon
            
            Widget *content = new Widget(dialog);
            content->set_layout(new GroupLayout());
            
            new Label(content, "Order Summary", "sans-bold");
            new Label(content, "Item: Premium Subscription", "sans");
            new Label(content, "Price: $9.99/month", "sans");
            new Label(content, "", "sans");
            new Label(content, "This is a non-dismissible dialog with callbacks.", "sans");
            
            dialog->set_content(content);
            dialog->set_dismissible(false);
            
            dialog->set_on_show([]() {
                std::cout << "✓ Purchase dialog shown" << std::endl;
            });
            
            dialog->set_on_dismiss([]() {
                std::cout << "✓ Purchase dialog dismissed" << std::endl;
            });
            
            dialog->add_action("Cancel", [dialog]() {
                std::cout << "Purchase cancelled" << std::endl;
                dialog->hide();
            });
            dialog->add_action("Confirm Purchase", [dialog]() {
                std::cout << "Purchase confirmed!" << std::endl;
                dialog->hide();
            });
            
            dialog->center();
            dialog->show();
        });

        // Info section
        new Label(window, "", "sans");  // Spacer
        new Label(window, "Dialog Features:", "sans-bold");
        new Label(window, "• Basic and full-screen variants", "sans");
        new Label(window, "• Horizontal/vertical action layouts", "sans");
        new Label(window, "• Dismissible and non-dismissible modes", "sans");
        new Label(window, "• onShow and onDismiss callbacks", "sans");
        new Label(window, "• Custom content widgets", "sans");
        new Label(window, "• Smooth slide animations", "sans");
        new Label(window, "• Keyboard navigation (Tab, Escape)", "sans");

        screen->set_visible(true);
        screen->perform_layout();
        screen->draw_all();

        nanogui::run();
    }

    nanogui::shutdown();
    return 0;
}
