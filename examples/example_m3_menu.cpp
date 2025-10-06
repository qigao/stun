/*
    examples/example_m3_menu.cpp -- M3 Menu Example

    This example demonstrates the M3Menu component with:
    - Basic menus with text items
    - Menus with icons
    - Trailing text (keyboard shortcuts)
    - Multi-level submenus
    - Keyboard navigation (Arrow keys, Enter, Escape)
    - Different anchor positions
    - Disabled items
    - Smart repositioning

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <iostream>
#include <nanogui/button.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/m3_theme.h>
#include <nanogui/m3_menu.h>
#include <nanogui/screen.h>
#include <nanogui/window.h>

using namespace nanogui;

int main(int /* argc */, char ** /* argv */) {
    nanogui::init();

    {
        // Create main screen
        Screen *screen = new Screen(Vector2i(1000, 700), "M3 Menu Examples");

        // Create M3 theme with blue seed
        auto *theme = new M3Theme(
            screen->nvg_context(),
            Color(0.2f, 0.4f, 0.9f, 1.0f),  // Blue seed
            M3Theme::Scheme::Light
        );
        screen->set_theme(theme);

        // Main window
        Window *window = new Window(screen, "M3 Menu Showcase");
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

        // Section 1: Basic Menu
        new Label(window, "1. Basic Menu", "sans-bold");
        new Label(window, "Simple menu with text items", "sans");

        Widget *basic_panel = new Widget(window);
        basic_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *basic_menu_btn = new Button(basic_panel, "Open Basic Menu");
        basic_menu_btn->set_callback([screen, basic_menu_btn]() {
            M3Menu *menu = new M3Menu(screen, basic_menu_btn);
            
            menu->add_item("New File", []() {
                std::cout << "✓ New File selected" << std::endl;
            });
            
            menu->add_item("Open File", []() {
                std::cout << "✓ Open File selected" << std::endl;
            });
            
            menu->add_item("Save", []() {
                std::cout << "✓ Save selected" << std::endl;
            });
            
            menu->add_divider();
            
            menu->add_item("Exit", []() {
                std::cout << "✓ Exit selected" << std::endl;
            });
            
            menu->show_at(basic_menu_btn);
        });

        // Section 2: Menu with Icons
        new Label(window, "2. Menu with Icons", "sans-bold");
        new Label(window, "Menu items with FontAwesome icons", "sans");

        Widget *icon_panel = new Widget(window);
        icon_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *icon_menu_btn = new Button(icon_panel, "Open Menu with Icons");
        icon_menu_btn->set_callback([screen, icon_menu_btn]() {
            M3Menu *menu = new M3Menu(screen, icon_menu_btn);
            
            menu->add_item("Cut", []() {
                std::cout << "✓ Cut selected" << std::endl;
            }, 0xf0c4);  // Scissors icon
            
            menu->add_item("Copy", []() {
                std::cout << "✓ Copy selected" << std::endl;
            }, 0xf0c5);  // Copy icon
            
            menu->add_item("Paste", []() {
                std::cout << "✓ Paste selected" << std::endl;
            }, 0xf0ea);  // Paste icon
            
            menu->add_divider();
            
            menu->add_item("Delete", []() {
                std::cout << "✓ Delete selected" << std::endl;
            }, 0xf1f8);  // Trash icon
            
            menu->show_at(icon_menu_btn);
        });

        // Section 3: Menu with Trailing Text (Shortcuts)
        new Label(window, "3. Menu with Shortcuts", "sans-bold");
        new Label(window, "Menu items with keyboard shortcut hints", "sans");

        Widget *shortcut_panel = new Widget(window);
        shortcut_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *shortcut_menu_btn = new Button(shortcut_panel, "Open Menu with Shortcuts");
        shortcut_menu_btn->set_callback([screen, shortcut_menu_btn]() {
            M3Menu *menu = new M3Menu(screen, shortcut_menu_btn);
            
            menu->add_item_with_trailing("Undo", "Ctrl+Z", []() {
                std::cout << "✓ Undo selected" << std::endl;
            }, 0xf0e2);  // Undo icon
            
            menu->add_item_with_trailing("Redo", "Ctrl+Y", []() {
                std::cout << "✓ Redo selected" << std::endl;
            }, 0xf01e);  // Redo icon
            
            menu->add_divider();
            
            menu->add_item_with_trailing("Find", "Ctrl+F", []() {
                std::cout << "✓ Find selected" << std::endl;
            }, 0xf002);  // Search icon
            
            menu->add_item_with_trailing("Replace", "Ctrl+H", []() {
                std::cout << "✓ Replace selected" << std::endl;
            });
            
            menu->show_at(shortcut_menu_btn);
        });

        // Section 4: Submenus (Multi-level)
        new Label(window, "4. Submenus (Multi-level)", "sans-bold");
        new Label(window, "Nested menus with hover delay (200ms)", "sans");

        Widget *submenu_panel = new Widget(window);
        submenu_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *submenu_btn = new Button(submenu_panel, "Open Menu with Submenus");
        submenu_btn->set_callback([screen, submenu_btn]() {
            M3Menu *menu = new M3Menu(screen, submenu_btn);
            
            menu->add_item("New", []() {
                std::cout << "✓ New selected" << std::endl;
            }, 0xf15b);  // File icon
            
            // Create "Open Recent" submenu
            M3Menu *recent_submenu = new M3Menu(screen);
            recent_submenu->add_item("document1.txt", []() {
                std::cout << "✓ Opened document1.txt" << std::endl;
            });
            recent_submenu->add_item("document2.txt", []() {
                std::cout << "✓ Opened document2.txt" << std::endl;
            });
            recent_submenu->add_item("document3.txt", []() {
                std::cout << "✓ Opened document3.txt" << std::endl;
            });
            
            menu->add_submenu("Open Recent", recent_submenu, 0xf1c0);  // Database icon
            
            menu->add_divider();
            
            // Create "Export" submenu with nested submenus
            M3Menu *export_submenu = new M3Menu(screen);
            
            // Create "Image Formats" nested submenu
            M3Menu *image_formats = new M3Menu(screen);
            image_formats->add_item("PNG", []() {
                std::cout << "✓ Export as PNG" << std::endl;
            });
            image_formats->add_item("JPEG", []() {
                std::cout << "✓ Export as JPEG" << std::endl;
            });
            image_formats->add_item("GIF", []() {
                std::cout << "✓ Export as GIF" << std::endl;
            });
            
            export_submenu->add_submenu("Image Formats", image_formats, 0xf1c5);  // Image icon
            
            // Create "Document Formats" nested submenu
            M3Menu *doc_formats = new M3Menu(screen);
            doc_formats->add_item("PDF", []() {
                std::cout << "✓ Export as PDF" << std::endl;
            });
            doc_formats->add_item("Word", []() {
                std::cout << "✓ Export as Word" << std::endl;
            });
            doc_formats->add_item("Text", []() {
                std::cout << "✓ Export as Text" << std::endl;
            });
            
            export_submenu->add_submenu("Document Formats", doc_formats, 0xf15c);  // File text icon
            
            menu->add_submenu("Export As", export_submenu, 0xf56e);  // Export icon
            
            menu->add_divider();
            
            menu->add_item("Print", []() {
                std::cout << "✓ Print selected" << std::endl;
            }, 0xf02f);  // Print icon
            
            menu->show_at(submenu_btn);
        });

        // Section 5: Anchor Positions
        new Label(window, "5. Anchor Positions", "sans-bold");
        new Label(window, "Control where menu appears relative to button", "sans");

        Widget *anchor_panel = new Widget(window);
        anchor_panel->set_layout(new GridLayout(Orientation::Horizontal, 2, Alignment::Fill, 10, 10));

        // TOP_START
        Button *top_start_btn = new Button(anchor_panel, "TOP_START");
        top_start_btn->set_callback([screen, top_start_btn]() {
            M3Menu *menu = new M3Menu(screen, top_start_btn);
            menu->set_anchor_position(M3Menu::AnchorPosition::TOP_START);
            
            menu->add_item("Item 1", []() { std::cout << "Item 1" << std::endl; });
            menu->add_item("Item 2", []() { std::cout << "Item 2" << std::endl; });
            menu->add_item("Item 3", []() { std::cout << "Item 3" << std::endl; });
            
            menu->show_at(top_start_btn);
        });

        // TOP_END
        Button *top_end_btn = new Button(anchor_panel, "TOP_END");
        top_end_btn->set_callback([screen, top_end_btn]() {
            M3Menu *menu = new M3Menu(screen, top_end_btn);
            menu->set_anchor_position(M3Menu::AnchorPosition::TOP_END);
            
            menu->add_item("Item 1", []() { std::cout << "Item 1" << std::endl; });
            menu->add_item("Item 2", []() { std::cout << "Item 2" << std::endl; });
            menu->add_item("Item 3", []() { std::cout << "Item 3" << std::endl; });
            
            menu->show_at(top_end_btn);
        });

        // BOTTOM_START (default)
        Button *bottom_start_btn = new Button(anchor_panel, "BOTTOM_START (default)");
        bottom_start_btn->set_callback([screen, bottom_start_btn]() {
            M3Menu *menu = new M3Menu(screen, bottom_start_btn);
            menu->set_anchor_position(M3Menu::AnchorPosition::BOTTOM_START);
            
            menu->add_item("Item 1", []() { std::cout << "Item 1" << std::endl; });
            menu->add_item("Item 2", []() { std::cout << "Item 2" << std::endl; });
            menu->add_item("Item 3", []() { std::cout << "Item 3" << std::endl; });
            
            menu->show_at(bottom_start_btn);
        });

        // BOTTOM_END
        Button *bottom_end_btn = new Button(anchor_panel, "BOTTOM_END");
        bottom_end_btn->set_callback([screen, bottom_end_btn]() {
            M3Menu *menu = new M3Menu(screen, bottom_end_btn);
            menu->set_anchor_position(M3Menu::AnchorPosition::BOTTOM_END);
            
            menu->add_item("Item 1", []() { std::cout << "Item 1" << std::endl; });
            menu->add_item("Item 2", []() { std::cout << "Item 2" << std::endl; });
            menu->add_item("Item 3", []() { std::cout << "Item 3" << std::endl; });
            
            menu->show_at(bottom_end_btn);
        });

        // Section 6: Keyboard Navigation
        new Label(window, "6. Keyboard Navigation", "sans-bold");
        new Label(window, "Use Arrow Up/Down, Enter to select, Escape to close", "sans");

        Widget *keyboard_panel = new Widget(window);
        keyboard_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *keyboard_menu_btn = new Button(keyboard_panel, "Open Menu (Try Keyboard)");
        keyboard_menu_btn->set_callback([screen, keyboard_menu_btn]() {
            M3Menu *menu = new M3Menu(screen, keyboard_menu_btn);
            
            menu->add_item("First Item", []() {
                std::cout << "✓ First Item selected" << std::endl;
            }, 0xf111);  // Circle icon
            
            menu->add_item("Second Item", []() {
                std::cout << "✓ Second Item selected" << std::endl;
            }, 0xf111);
            
            menu->add_item("Third Item", []() {
                std::cout << "✓ Third Item selected" << std::endl;
            }, 0xf111);
            
            menu->add_divider();
            
            menu->add_item("Fourth Item", []() {
                std::cout << "✓ Fourth Item selected" << std::endl;
            }, 0xf111);
            
            menu->add_item("Fifth Item (Disabled)", nullptr, 0xf111, false);
            
            menu->add_item("Sixth Item", []() {
                std::cout << "✓ Sixth Item selected" << std::endl;
            }, 0xf111);
            
            menu->show_at(keyboard_menu_btn);
        });

        // Section 7: Disabled Items
        new Label(window, "7. Disabled Items", "sans-bold");
        new Label(window, "Menu items can be disabled (grayed out)", "sans");

        Widget *disabled_panel = new Widget(window);
        disabled_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *disabled_menu_btn = new Button(disabled_panel, "Menu with Disabled Items");
        disabled_menu_btn->set_callback([screen, disabled_menu_btn]() {
            M3Menu *menu = new M3Menu(screen, disabled_menu_btn);
            
            menu->add_item("Enabled Item 1", []() {
                std::cout << "✓ Enabled Item 1" << std::endl;
            }, 0xf00c);  // Check icon
            
            menu->add_item("Disabled Item", nullptr, 0xf00d, false);  // X icon, disabled
            
            menu->add_item("Enabled Item 2", []() {
                std::cout << "✓ Enabled Item 2" << std::endl;
            }, 0xf00c);
            
            menu->add_divider();
            
            menu->add_item("Another Disabled", nullptr, 0xf00d, false);
            
            menu->add_item("Enabled Item 3", []() {
                std::cout << "✓ Enabled Item 3" << std::endl;
            }, 0xf00c);
            
            menu->show_at(disabled_menu_btn);
        });

        // Section 8: Complex Example
        new Label(window, "8. Complex Example", "sans-bold");
        new Label(window, "All features combined: icons, shortcuts, submenus, disabled items", "sans");

        Widget *complex_panel = new Widget(window);
        complex_panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

        Button *complex_menu_btn = new Button(complex_panel, "Open Complex Menu");
        complex_menu_btn->set_callback([screen, complex_menu_btn]() {
            M3Menu *menu = new M3Menu(screen, complex_menu_btn);
            
            // File operations
            menu->add_item_with_trailing("New File", "Ctrl+N", []() {
                std::cout << "✓ New File" << std::endl;
            }, 0xf15b);
            
            menu->add_item_with_trailing("Open", "Ctrl+O", []() {
                std::cout << "✓ Open" << std::endl;
            }, 0xf07c);
            
            menu->add_item_with_trailing("Save", "Ctrl+S", []() {
                std::cout << "✓ Save" << std::endl;
            }, 0xf0c7);
            
            menu->add_item_with_trailing("Save As", "Ctrl+Shift+S", nullptr, 0xf0c7, false);  // Disabled
            
            menu->add_divider();
            
            // Edit submenu
            M3Menu *edit_submenu = new M3Menu(screen);
            edit_submenu->add_item_with_trailing("Undo", "Ctrl+Z", []() {
                std::cout << "✓ Undo" << std::endl;
            }, 0xf0e2);
            edit_submenu->add_item_with_trailing("Redo", "Ctrl+Y", []() {
                std::cout << "✓ Redo" << std::endl;
            }, 0xf01e);
            edit_submenu->add_divider();
            edit_submenu->add_item_with_trailing("Cut", "Ctrl+X", []() {
                std::cout << "✓ Cut" << std::endl;
            }, 0xf0c4);
            edit_submenu->add_item_with_trailing("Copy", "Ctrl+C", []() {
                std::cout << "✓ Copy" << std::endl;
            }, 0xf0c5);
            edit_submenu->add_item_with_trailing("Paste", "Ctrl+V", []() {
                std::cout << "✓ Paste" << std::endl;
            }, 0xf0ea);
            
            menu->add_submenu("Edit", edit_submenu, 0xf044);  // Edit icon
            
            // View submenu with nested submenus
            M3Menu *view_submenu = new M3Menu(screen);
            
            M3Menu *zoom_submenu = new M3Menu(screen);
            zoom_submenu->add_item_with_trailing("Zoom In", "Ctrl++", []() {
                std::cout << "✓ Zoom In" << std::endl;
            });
            zoom_submenu->add_item_with_trailing("Zoom Out", "Ctrl+-", []() {
                std::cout << "✓ Zoom Out" << std::endl;
            });
            zoom_submenu->add_item_with_trailing("Reset Zoom", "Ctrl+0", []() {
                std::cout << "✓ Reset Zoom" << std::endl;
            });
            
            view_submenu->add_submenu("Zoom", zoom_submenu, 0xf00e);  // Search plus icon
            view_submenu->add_item("Full Screen", []() {
                std::cout << "✓ Full Screen" << std::endl;
            }, 0xf065);
            
            menu->add_submenu("View", view_submenu, 0xf06e);  // Eye icon
            
            menu->add_divider();
            
            menu->add_item_with_trailing("Exit", "Alt+F4", []() {
                std::cout << "✓ Exit" << std::endl;
            }, 0xf011);  // Power off icon
            
            menu->show_at(complex_menu_btn);
        });

        // Info section
        new Label(window, "", "sans");  // Spacer
        new Label(window, "Menu Features:", "sans-bold");
        new Label(window, "• Basic text items", "sans");
        new Label(window, "• Icons (FontAwesome)", "sans");
        new Label(window, "• Trailing text (shortcuts)", "sans");
        new Label(window, "• Multi-level submenus", "sans");
        new Label(window, "• Keyboard navigation (↑↓ Enter Esc)", "sans");
        new Label(window, "• 4 anchor positions", "sans");
        new Label(window, "• Disabled items", "sans");
        new Label(window, "• Smart repositioning (stays on screen)", "sans");
        new Label(window, "• Hover delay (200ms) for submenus", "sans");

        screen->set_visible(true);
        screen->perform_layout();
        screen->draw_all();

        nanogui::run();
    }

    nanogui::shutdown();
    return 0;
}
