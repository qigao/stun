/*
 * Meta Editor - Template Panel Implementation
 */

#include "meta_editor/view/template_panel.h"
#include "meta_editor/core/editor.h"

namespace meta_editor {

TemplatePanel::TemplatePanel(Editor* editor) : editor_(editor) {
    set_size(300, 350);
    
    templates_ = {
        {"Blank", "□", "Empty canvas"},
        {"Brainstorm", "💡", "Central idea with branches"},
        {"Kanban", "▦", "3-column task board"},
        {"Flowchart", "◇", "Process flow diagram"},
        {"Mind Map", "🕸", "Hierarchical idea map"},
        {"Wireframe", "▢", "UI mockup grid"},
        {"Timeline", "→", "Horizontal timeline"},
        {"SWOT", "⊞", "2x2 analysis grid"},
    };
}

void TemplatePanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    render_background(renderer);

    // Title
    renderer.draw_text("Templates", x_ + 12, y_ + 24, "Arial", 14.0f, true, 
                       flex::Color{1.0f, 1.0f, 1.0f, 1.0f});

    // Template grid (2 columns)
    float start_y = y_ + 50;
    float card_w = 130;
    float card_h = 70;
    float gap = 10;

    for (size_t i = 0; i < templates_.size(); ++i) {
        int col = i % 2;
        int row = i / 2;
        
        float cx = x_ + 12 + col * (card_w + gap);
        float cy = start_y + row * (card_h + gap);

        bool hovered = (static_cast<int>(i) == hovered_index_);
        
        flex::Paint card_bg = hovered
            ? flex::Paint::solid(flex::Color{0.3f, 0.35f, 0.4f, 1.0f})
            : flex::Paint::solid(flex::Color{0.22f, 0.22f, 0.22f, 1.0f});
        flex::Paint card_border = hovered
            ? flex::Paint::solid(flex::Color{0.4f, 0.6f, 0.9f, 1.0f})
            : flex::Paint::solid(flex::Color{0.35f, 0.35f, 0.35f, 1.0f});
        
        renderer.draw_rect(cx, cy, card_w, card_h, 6.0f, card_bg, card_border, 1.0f);

        // Icon
        renderer.draw_text(templates_[i].icon.c_str(), cx + 10, cy + 28, "Arial", 20.0f, false,
                          flex::Color{0.7f, 0.8f, 0.9f, 1.0f});

        // Name
        renderer.draw_text(templates_[i].name.c_str(), cx + 40, cy + 25, "Arial", 12.0f, true,
                          flex::Color{1.0f, 1.0f, 1.0f, 1.0f});

        // Description
        renderer.draw_text(templates_[i].description.c_str(), cx + 40, cy + 45, "Arial", 10.0f, false,
                          flex::Color{0.6f, 0.6f, 0.6f, 1.0f});
    }
}

bool TemplatePanel::handle_click(float px, float py) {
    float start_y = y_ + 50;
    float card_w = 130;
    float card_h = 70;
    float gap = 10;

    for (size_t i = 0; i < templates_.size(); ++i) {
        int col = i % 2;
        int row = i / 2;
        float cx = x_ + 12 + col * (card_w + gap);
        float cy = start_y + row * (card_h + gap);

        if (px >= cx && px <= cx + card_w && py >= cy && py <= cy + card_h) {
            apply_template(static_cast<int>(i));
            return true;
        }
    }

    return false;
}

void TemplatePanel::apply_template(int index) {
    if (index < 0 || index >= static_cast<int>(templates_.size())) return;

    auto* canvas = editor_->canvas();
    auto* allocator = canvas->instance()->object_allocator();
    auto* layer = canvas->content_root();

    // Clear existing content
    layer->clear_children();

    const auto& tmpl = templates_[index];

    if (tmpl.name == "Blank") {
        // Nothing to add
    }
    else if (tmpl.name == "Brainstorm") {
        // Central circle with 4 branches
        auto* center = flex::Shape::create(*allocator);
        center->set_circle(60);
        center->set_position(400, 300);
        center->set_fill(flex::Color{0.9f, 0.85f, 0.6f, 1.0f});
        center->set_stroke(flex::Color::Black, 2.0f);
        layer->add_child(center);

        auto* label = flex::Text::create(*allocator);
        label->set_content("Main Idea");
        label->set_position(360, 295);
        label->set_font_size(14.0f);
        layer->add_child(label);

        float angles[] = {0, 90, 180, 270};
        for (int i = 0; i < 4; ++i) {
            float rad = angles[i] * 3.14159f / 180.0f;
            float bx = 400 + std::cos(rad) * 150;
            float by = 300 + std::sin(rad) * 150;

            auto* branch = flex::Shape::create(*allocator);
            branch->set_circle(40);
            branch->set_position(bx, by);
            branch->set_fill(flex::Color{0.7f, 0.85f, 0.95f, 1.0f});
            branch->set_stroke(flex::Color::Black, 1.5f);
            layer->add_child(branch);
        }
    }
    else if (tmpl.name == "Kanban") {
        const char* cols[] = {"To Do", "In Progress", "Done"};
        for (int i = 0; i < 3; ++i) {
            auto* col = flex::Shape::create(*allocator);
            col->set_rect(200, 400, 8.0f);
            col->set_position(100 + i * 220, 50);
            col->set_fill(flex::Color{0.95f, 0.95f, 0.95f, 1.0f});
            col->set_stroke(flex::Color{0.8f, 0.8f, 0.8f, 1.0f}, 1.0f);
            layer->add_child(col);

            auto* header = flex::Text::create(*allocator);
            header->set_content(cols[i]);
            header->set_position(140 + i * 220, 75);
            header->set_font_size(14.0f);
            header->set_color(flex::Color{0.3f, 0.3f, 0.3f, 1.0f});
            layer->add_child(header);
        }
    }
    else if (tmpl.name == "Flowchart") {
        // Start -> Process -> Decision -> End
        auto* start = flex::Shape::create(*allocator);
        start->set_ellipse(50, 25);
        start->set_position(400, 80);
        start->set_fill(flex::Color{0.7f, 0.9f, 0.7f, 1.0f});
        start->set_stroke(flex::Color::Black, 1.5f);
        layer->add_child(start);

        auto* process = flex::Shape::create(*allocator);
        process->set_rect(120, 60, 0);
        process->set_position(340, 150);
        process->set_fill(flex::Color{0.9f, 0.9f, 0.95f, 1.0f});
        process->set_stroke(flex::Color::Black, 1.5f);
        layer->add_child(process);

        auto* decision = flex::Shape::create(*allocator);
        decision->set_polygon(4, 50);
        decision->set_position(400, 300);
        decision->set_fill(flex::Color{1.0f, 0.95f, 0.8f, 1.0f});
        decision->set_stroke(flex::Color::Black, 1.5f);
        layer->add_child(decision);

        auto* end = flex::Shape::create(*allocator);
        end->set_ellipse(50, 25);
        end->set_position(400, 420);
        end->set_fill(flex::Color{0.95f, 0.8f, 0.8f, 1.0f});
        end->set_stroke(flex::Color::Black, 1.5f);
        layer->add_child(end);
    }
    else if (tmpl.name == "SWOT") {
        const char* labels[] = {"Strengths", "Weaknesses", "Opportunities", "Threats"};
        flex::Color colors[] = {
            {0.7f, 0.9f, 0.7f, 1.0f},
            {0.9f, 0.8f, 0.8f, 1.0f},
            {0.8f, 0.85f, 0.95f, 1.0f},
            {1.0f, 0.9f, 0.7f, 1.0f}
        };
        for (int i = 0; i < 4; ++i) {
            int col = i % 2;
            int row = i / 2;
            auto* quad = flex::Shape::create(*allocator);
            quad->set_rect(200, 180, 0);
            quad->set_position(200 + col * 210, 100 + row * 190);
            quad->set_fill(colors[i]);
            quad->set_stroke(flex::Color{0.5f, 0.5f, 0.5f, 1.0f}, 1.0f);
            layer->add_child(quad);

            auto* label = flex::Text::create(*allocator);
            label->set_content(labels[i]);
            label->set_position(220 + col * 210, 125 + row * 190);
            label->set_font_size(13.0f);
            label->set_color(flex::Color{0.2f, 0.2f, 0.2f, 1.0f});
            layer->add_child(label);
        }
    }
    else if (tmpl.name == "Timeline") {
        // Horizontal line with markers
        auto* line = flex::Shape::create(*allocator);
        line->set_path("M 50 250 L 750 250", 700, 4, 50, 248);
        line->set_stroke(flex::Color{0.4f, 0.4f, 0.4f, 1.0f}, 3.0f);
        layer->add_child(line);

        for (int i = 0; i < 5; ++i) {
            float mx = 100 + i * 150;
            auto* marker = flex::Shape::create(*allocator);
            marker->set_circle(8);
            marker->set_position(mx, 250);
            marker->set_fill(flex::Color{0.3f, 0.5f, 0.8f, 1.0f});
            layer->add_child(marker);

            auto* label = flex::Text::create(*allocator);
            char buf[16];
            snprintf(buf, sizeof(buf), "Event %d", i + 1);
            label->set_content(buf);
            label->set_position(mx - 25, 275);
            label->set_font_size(11.0f);
            layer->add_child(label);
        }
    }

    if (apply_callback_) {
        apply_callback_(tmpl.name);
    }
}

} // namespace meta_editor
