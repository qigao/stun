/*
 * flexUI Designer - Menu Bar Implementation
 */

#include "flexui_designer/menu_bar.h"

namespace flexui_designer {

MenuBar::MenuBar() {
    height_ = HEIGHT;
}

void MenuBar::add_menu(const std::string& label, std::vector<MenuItem> items) {
    menus_.push_back({label, std::move(items)});
}

float MenuBar::menu_x(int index) const {
    float x = x_ + 8;
    for (int i = 0; i < index; ++i) {
        x += menu_width(menus_[i].label) + MENU_PADDING;
    }
    return x;
}

float MenuBar::menu_width(const std::string& label) const {
    return label.length() * 7.0f + 16.0f;
}

void MenuBar::render(flex::Renderer& renderer) {
    if (!visible_) return;

    renderer.save();
    renderer.translate(x_, y_);

    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1});
    renderer.draw_rect(0, 0, width_, HEIGHT, 0, bg, flex::Paint::none(), 0);

    // Bottom border
    renderer.draw_rect(0, HEIGHT - 1, width_, 1, 0, 
        flex::Paint::solid(flex::Color{0.22f, 0.22f, 0.25f, 1}), flex::Paint::none(), 0);

    // Menu items
    float mx = 8;
    for (size_t i = 0; i < menus_.size(); ++i) {
        float mw = menu_width(menus_[i].label);
        
        // Highlight if open
        if ((int)i == open_menu_) {
            flex::Paint sel = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 0.3f});
            renderer.draw_rect(mx - 4, 2, mw + 8, HEIGHT - 4, 3, sel, flex::Paint::none(), 0);
        }

        flex::Color col = ((int)i == open_menu_) ? flex::Color{1, 1, 1, 1} : flex::Color{0.8f, 0.8f, 0.85f, 1};
        renderer.draw_text(menus_[i].label, mx, 9, "sans", 12, false, col);
        mx += mw + MENU_PADDING;
    }

    // Render open dropdown
    if (open_menu_ >= 0 && open_menu_ < (int)menus_.size()) {
        render_dropdown(renderer, menus_[open_menu_], menu_x(open_menu_) - x_);
    }

    renderer.restore();
}

void MenuBar::render_dropdown(flex::Renderer& renderer, const Menu& menu, float dx) {
    renderer.save();
    renderer.translate(dx, HEIGHT); // Translate to dropdown's top-left corner relative to menubar's (x_, y_)

    float dh = 8; // padding
    for (const auto& item : menu.items) {
        dh += item.separator ? 8 : ITEM_HEIGHT;
    }

    // Shadow
    flex::Paint shadow = flex::Paint::solid(flex::Color{0, 0, 0, 0.3f});
    renderer.draw_rect(2, 2, DROPDOWN_WIDTH, dh, 4, shadow, flex::Paint::none(), 0);

    // Background
    flex::Paint bg = flex::Paint::solid(flex::Color{0.16f, 0.16f, 0.18f, 0.98f});
    flex::Paint border = flex::Paint::solid(flex::Color{0.28f, 0.28f, 0.32f, 1});
    renderer.draw_rect(0, 0, DROPDOWN_WIDTH, dh, 4, bg, border, 1);

    float iy = 4; // Relative Y for items
    for (size_t i = 0; i < menu.items.size(); ++i) {
        const auto& item = menu.items[i];
        
        if (item.separator) {
            renderer.draw_rect(8, iy + 3, DROPDOWN_WIDTH - 16, 1, 0,
                flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1}), flex::Paint::none(), 0);
            iy += 8.0f;
            continue;
        }

        // Hover highlight
        if ((int)i == hovered_item_) {
            flex::Paint hover = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 0.25f});
            renderer.draw_rect(4, iy, DROPDOWN_WIDTH - 8, ITEM_HEIGHT - 2, 3, hover, flex::Paint::none(), 0);
        }

        // Checkmark
        if (item.checkable && item.checked) {
            renderer.draw_text("✓", 10, iy + 8, "sans", 11, false, {0.4f, 0.8f, 0.5f, 1});
        }

        // Label
        float label_x = item.checkable ? 28 : 12;
        renderer.draw_text(item.label, label_x, iy + 7, "sans", 11, false, {0.9f, 0.9f, 0.92f, 1});

        // Shortcut
        if (!item.shortcut.empty()) {
            renderer.draw_text(item.shortcut, DROPDOWN_WIDTH - 70, iy + 8, "sans", 10, false, {0.5f, 0.5f, 0.55f, 1});
        }

        iy += ITEM_HEIGHT;
    }
    renderer.restore();
}

bool MenuBar::handle_click(float px, float py) {
    if (!visible_) return false;

    // Click on menu bar
    if (py >= y_ && py < y_ + HEIGHT) {
        float mx = x_ + 8;
        for (size_t i = 0; i < menus_.size(); ++i) {
            float mw = menu_width(menus_[i].label);
            if (px >= mx - 4 && px < mx + mw + 4) {
                open_menu_ = (open_menu_ == (int)i) ? -1 : (int)i;
                hovered_item_ = -1;
                return true;
            }
            mx += mw + MENU_PADDING;
        }
        open_menu_ = -1;
        return true;
    }

    // Click on dropdown
    if (open_menu_ >= 0 && open_menu_ < (int)menus_.size()) {
        float dx = menu_x(open_menu_);
        float dy = y_ + HEIGHT;
        
        if (px >= dx && px < dx + DROPDOWN_WIDTH) {
            float iy = dy + 4;
            for (size_t i = 0; i < menus_[open_menu_].items.size(); ++i) {
                const auto& item = menus_[open_menu_].items[i];
                float item_h = item.separator ? 8.0f : ITEM_HEIGHT;
                
                if (!item.separator && py >= iy && py < iy + item_h) {
                    if (item.action) item.action();
                    open_menu_ = -1;
                    return true;
                }
                iy += item_h;
            }
        }
        open_menu_ = -1;
        return true;
    }

    return false;
}

} // namespace flexui_designer
