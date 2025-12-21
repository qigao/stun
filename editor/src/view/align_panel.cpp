/*
 * Align Panel Implementation
 */

#include <editor/view/align_panel.h>
#include <editor/viewmodel/editor_vm.h>
#include <algorithm>
#include <limits>

namespace editor {

AlignPanel::AlignPanel() {}

bool AlignPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + style_.width &&
           my >= y_ && my < y_ + style_.title_height;
}

void AlignPanel::render(flex::Renderer& renderer) {
    bool hasSelection = vm_ && !vm_->selection().isEmpty();
    bool multiSelection = vm_ && vm_->selection().count() > 1;

    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.height - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(style_.width - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(style_.height - 8)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    // Title bar
    std::string titleBg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(style_.width - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.title_height - 4) +
        " h " + std::to_string(-style_.width) +
        " v " + std::to_string(-(style_.title_height - 4)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(titleBg, flex::Paint::solid({style_.title_bg.r, style_.title_bg.g,
        style_.title_bg.b, style_.title_bg.a}));

    // Drag grip
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + style_.title_height / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 8";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    // Title
    renderer.draw_text("Align", x_ + 22, y_ + 16, "Arial", 11, true,
        {style_.title_text.r, style_.title_text.g, style_.title_text.b, 1});

    float bx = x_ + style_.padding;
    float by = y_ + style_.title_height + style_.padding;

    // Align label
    renderer.draw_text("Align", bx, by + 12, "Arial", 10, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    // Align buttons (6 buttons)
    float btnX = bx + 35;
    const char* alignIcons[] = {"[|", "|·|", "|]", "T", "⊥", "_T_"};
    for (int i = 0; i < 6; ++i) {
        renderButton(renderer, btnX, by, alignIcons[i], hasSelection, hovered_button_ == i);
        btnX += style_.button_size + style_.spacing;
    }

    // Distribute label
    by += style_.button_size + style_.spacing + 4;
    renderer.draw_text("Distribute", bx, by + 12, "Arial", 10, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    // Distribute buttons (2 buttons)
    btnX = bx + 55;
    const char* distIcons[] = {"|||", "≡"};
    for (int i = 0; i < 2; ++i) {
        renderButton(renderer, btnX, by, distIcons[i], multiSelection, hovered_button_ == (6 + i));
        btnX += style_.button_size + style_.spacing;
    }
}

void AlignPanel::renderButton(flex::Renderer& renderer, float x, float y,
                              const char* icon, bool enabled, bool hovered) {
    float s = style_.button_size;

    Color bgColor = !enabled ? style_.button_disabled :
                    (hovered ? style_.button_hover : style_.button_normal);

    std::string btn = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(s) + " v " + std::to_string(s) +
        " h " + std::to_string(-s) + " Z";
    renderer.fill_path(btn, flex::Paint::solid({bgColor.r, bgColor.g, bgColor.b, bgColor.a}));

    Color iconColor = enabled ? style_.icon_normal : style_.icon_disabled;
    renderer.draw_text(icon, x + 4, y + s - 8, "Arial", 10, false,
        {iconColor.r, iconColor.g, iconColor.b, 1});
}

int AlignPanel::buttonAt(float mx, float my) const {
    float bx = x_ + style_.padding + 35;
    float by = y_ + style_.title_height + style_.padding;

    // Check align buttons
    for (int i = 0; i < 6; ++i) {
        Rect r = {bx + i * (style_.button_size + style_.spacing), by,
                  style_.button_size, style_.button_size};
        if (r.contains({mx, my})) return i;
    }

    // Check distribute buttons
    by += style_.button_size + style_.spacing + 4;
    bx = x_ + style_.padding + 55;
    for (int i = 0; i < 2; ++i) {
        Rect r = {bx + i * (style_.button_size + style_.spacing), by,
                  style_.button_size, style_.button_size};
        if (r.contains({mx, my})) return 6 + i;
    }

    return -1;
}

void AlignPanel::alignSelection(AlignType type) {
    if (!vm_ || vm_->selection().isEmpty()) return;

    auto& sel = vm_->selection().selection();

    // Calculate bounds of all selected objects
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (auto& node : sel) {
        auto bounds = node->bounds();
        minX = std::min(minX, bounds.x);
        minY = std::min(minY, bounds.y);
        maxX = std::max(maxX, bounds.x + bounds.width);
        maxY = std::max(maxY, bounds.y + bounds.height);
    }

    float centerX = (minX + maxX) / 2;
    float centerY = (minY + maxY) / 2;

    for (auto& node : sel) {
        auto bounds = node->bounds();
        float dx = 0, dy = 0;

        switch (type) {
            case AlignType::Left:
                dx = minX - bounds.x;
                break;
            case AlignType::CenterH:
                dx = centerX - (bounds.x + bounds.width / 2);
                break;
            case AlignType::Right:
                dx = maxX - (bounds.x + bounds.width);
                break;
            case AlignType::Top:
                dy = minY - bounds.y;
                break;
            case AlignType::CenterV:
                dy = centerY - (bounds.y + bounds.height / 2);
                break;
            case AlignType::Bottom:
                dy = maxY - (bounds.y + bounds.height);
                break;
        }

        node->setPosition(node->x() + dx, node->y() + dy);
    }
}

void AlignPanel::distributeSelection(DistributeType type) {
    if (!vm_ || vm_->selection().count() < 3) return;

    auto sel = vm_->selection().selection();

    // Sort by position
    if (type == DistributeType::Horizontal) {
        std::sort(sel.begin(), sel.end(), [](auto& a, auto& b) {
            return a->bounds().x < b->bounds().x;
        });
    } else {
        std::sort(sel.begin(), sel.end(), [](auto& a, auto& b) {
            return a->bounds().y < b->bounds().y;
        });
    }

    // Get total range
    auto firstBounds = sel.front()->bounds();
    auto lastBounds = sel.back()->bounds();

    if (type == DistributeType::Horizontal) {
        float totalWidth = (lastBounds.x + lastBounds.width / 2) -
                          (firstBounds.x + firstBounds.width / 2);
        float spacing = totalWidth / (sel.size() - 1);

        for (size_t i = 1; i < sel.size() - 1; ++i) {
            auto& node = sel[i];
            auto bounds = node->bounds();
            float targetCenterX = firstBounds.x + firstBounds.width / 2 + spacing * i;
            float dx = targetCenterX - (bounds.x + bounds.width / 2);
            node->setPosition(node->x() + dx, node->y());
        }
    } else {
        float totalHeight = (lastBounds.y + lastBounds.height / 2) -
                           (firstBounds.y + firstBounds.height / 2);
        float spacing = totalHeight / (sel.size() - 1);

        for (size_t i = 1; i < sel.size() - 1; ++i) {
            auto& node = sel[i];
            auto bounds = node->bounds();
            float targetCenterY = firstBounds.y + firstBounds.height / 2 + spacing * i;
            float dy = targetCenterY - (bounds.y + bounds.height / 2);
            node->setPosition(node->x(), node->y() + dy);
        }
    }
}

bool AlignPanel::onMouseDown(float mx, float my, int button) {
    if (button != 0) return false;

    // Check for panel drag start on title bar
    if (draggable_ && isInTitleBar(mx, my)) {
        dragging_ = true;
        drag_offset_x_ = mx - x_;
        drag_offset_y_ = my - y_;
        return true;
    }

    int btn = buttonAt(mx, my);
    if (btn < 0) return false;

    bool hasSelection = vm_ && !vm_->selection().isEmpty();
    bool multiSelection = vm_ && vm_->selection().count() > 1;

    if (btn < 6 && hasSelection) {
        alignSelection(static_cast<AlignType>(btn));
        return true;
    } else if (btn >= 6 && multiSelection) {
        distributeSelection(static_cast<DistributeType>(btn - 6));
        return true;
    }

    return false;
}

bool AlignPanel::onMouseMove(float mx, float my) {
    // Handle panel dragging
    if (dragging_) {
        x_ = mx - drag_offset_x_;
        y_ = my - drag_offset_y_;
        return true;
    }

    hovered_button_ = buttonAt(mx, my);
    return hovered_button_ >= 0 || isInTitleBar(mx, my);
}

bool AlignPanel::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    if (dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

} // namespace editor
