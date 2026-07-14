/*
 * flexUI - LabelWidget Implementation
 */

#include <flexUI/widgets/label_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>

namespace flexUI {

LabelWidget::LabelWidget(const std::string& text)
    : text_(text) {}

void LabelWidget::set_text(const std::string& text) {
    text_ = text;
    sync_host_semantics();
    invalidate_render_cache();
}

void LabelWidget::sync_host_semantics() {
    if (auto* host = host_element()) {
        host->set_text(text_);
    }
}

void LabelWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;
    if (!style) return;
    const std::string display_text = !elem.text().empty() ? elem.text() : text_;
    if (display_text.empty()) return;

    if (render_cache_matches(elem, display_text)) {
        commands.append(render_cache_);
        dirty_ = false;
        return;
    }

    const float content_x = style->padding[3];
    const float content_y = style->padding[0];
    const float content_width =
        std::max(0.0f, elem.width() - style->padding[1] - style->padding[3]);
    const float content_height =
        std::max(0.0f, elem.height() - style->padding[0] - style->padding[2]);
    const Color text_color =
        style->get_variable_color("--text-color", style->text_color);
    const auto text_block = layout_text_block(
        style, display_text, content_x, content_y, content_width, content_height,
        text_color, resolve_text_vertical_align(style, TextVerticalAlign::Top));

    RenderCommandList rebuilt(commands.capabilities());
    emit_text_block(rebuilt, text_block);
    render_cache_ = rebuilt.commands();
    update_render_cache_key(elem, display_text);
    commands.append(render_cache_);

    dirty_ = false;
}

bool LabelWidget::render_cache_matches(const Element& elem,
                                       const std::string& display_text) const {
    const auto* style = elem.computed_style;
    if (!render_cache_valid_ || !style) {
        return false;
    }

    return cached_text_ == display_text &&
           cached_width_ == elem.width() &&
           cached_height_ == elem.height() &&
           cached_font_size_ == style->font_size &&
           cached_letter_spacing_ == style->letter_spacing &&
           cached_word_spacing_ == style->word_spacing &&
           cached_text_indent_ == style->text_indent &&
           cached_tab_size_ == style->tab_size &&
           cached_font_weight_ == static_cast<int>(style->font_weight) &&
           cached_font_style_ == static_cast<int>(style->font_style) &&
           cached_text_align_ == static_cast<int>(style->text_align) &&
           cached_text_transform_ == static_cast<int>(style->text_transform) &&
           cached_direction_ == static_cast<int>(style->direction) &&
           cached_style_signature_ == style->variables_signature() &&
           cached_font_family_ == style->font_family &&
           cached_text_color_ == style->text_color;
}

void LabelWidget::update_render_cache_key(const Element& elem,
                                          const std::string& display_text) {
    const auto* style = elem.computed_style;
    if (!style) {
        render_cache_valid_ = false;
        return;
    }

    cached_text_ = display_text;
    cached_width_ = elem.width();
    cached_height_ = elem.height();
    cached_font_size_ = style->font_size;
    cached_letter_spacing_ = style->letter_spacing;
    cached_word_spacing_ = style->word_spacing;
    cached_text_indent_ = style->text_indent;
    cached_tab_size_ = style->tab_size;
    cached_font_weight_ = static_cast<int>(style->font_weight);
    cached_font_style_ = static_cast<int>(style->font_style);
    cached_text_align_ = static_cast<int>(style->text_align);
    cached_text_transform_ = static_cast<int>(style->text_transform);
    cached_direction_ = static_cast<int>(style->direction);
    cached_style_signature_ = style->variables_signature();
    cached_font_family_ = style->font_family;
    cached_text_color_ = style->text_color;
    render_cache_valid_ = true;
}

void LabelWidget::invalidate_render_cache() {
    render_cache_valid_ = false;
    dirty_ = true;
}

bool LabelWidget::handle_event(const Event& event, Element& elem) {
    // Label has no interaction
    return false;
}

void LabelWidget::update(float delta_ms, Element& elem) {
    // Label has no animation
}

} // namespace flexUI
