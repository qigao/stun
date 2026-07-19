#include <infographic_component.h>
#include <layout_renderer.h>
#include <layout/layout_engine.h>
#include <render_helpers.h>
#include <flex/runtime/group.h>
#include <flex/runtime/shape.h>
#include <flex/runtime/text.h>
#include <flex/runtime/component.h>
#include <flex.h>
#include <cmath>
#include <parser/unified_parser.h>
#include <tlog.h>

namespace flex::modules::infographic {

void InfographicComponent::register_component() {
    flex::ComponentRegistry::instance().register_component("Infographic", 
        [](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            const std::string source = flex::get_prop_string(props, "source", "");
            const float width = flex::get_prop_float(props, "width", 600.0f);
            const float height = flex::get_prop_float(props, "height", 500.0f);
            if (source.empty() || !std::isfinite(width) || !std::isfinite(height) ||
                width <= 0.0f || height <= 0.0f) {
                TLOG_ERROR("Infographic component requires non-empty source and positive finite dimensions");
                return nullptr;
            }

            UnifiedParser parser;
            auto result = parser.parse(source);
            if (!result.success) {
                TLOG_ERROR("Infographic parse error at {}:{}: {}", result.error_line,
                           result.error_column, result.get_error());
                return nullptr;
            }

            auto instance = flex::Instance::create(width, height);
            flex::Group* root = InfographicComponent::build(*result.infographic, *instance,
                                                             width, height);
            if (!root) return nullptr;
            return std::shared_ptr<flex::Node>(root, [instance](flex::Node*) mutable {
                instance.reset();
            });
        });
}

flex::Group* InfographicComponent::build(const UnifiedInfographic& info, flex::Instance& instance) {
    return build(info, instance, 600.0f, 500.0f);
}

flex::Group* InfographicComponent::build(const UnifiedInfographic& info, flex::Instance& instance,
                                         float width, float height) {
    return build(info, instance, width, height, {});
}

flex::Group* InfographicComponent::build(
    const UnifiedInfographic& info, flex::Instance& instance, float width,
    float height, const InfographicComponentBuildOptions& options) {
    flex::ArenaAllocator& arena = *instance.object_allocator();
    
    auto root = flex::Group::create(arena);
    root->set_id("infographic-root");
    root->set_layout_size(width, height);
    
    // Background
    if (options.show_background) {
        auto bg = arena.create<flex::Shape>();
        bg->set_rect(width, height);
        bg->set_fill(flex::Color(0.98f, 0.98f, 0.98f));
        root->add_child(bg);
    }
    
    // Title
    if (options.show_title && info.title) {
        auto title = arena.create<flex::Text>();
        title->set_content(*info.title);
        title->set_font_size(24.0f);
        title->set_font_weight(flex::FontWeight::Bold);
        title->set_color(flex::Color(0.15f, 0.15f, 0.15f));
        title->set_position(width * 0.5f, 30);
        title->set_anchor(flex::Anchor::Top);
        root->add_child(title);
    }
    
    // Get layout type and create renderer
    LayoutType layout = get_layout_type(info.template_type);
    auto renderer = LayoutRendererFactory::create(layout);
    LayoutResult layout_result;
    bool has_layout = false;

    auto layout_engine = create_layout_engine(info.template_type);
    if (layout_engine) {
        const std::string template_name = template_type_to_string(info.template_type);
        const StyleConfig style = parse_style_from_template(template_name);
        const int layout_width = static_cast<int>(std::lround(width));
        const int layout_height = static_cast<int>(std::lround(height));
        layout_result = layout_engine->compute(info, layout_width, layout_height, style);
        has_layout = !layout_result.nodes.empty();
    }
    
    if (renderer) {
        LayoutRenderContext ctx{arena, root, width, height, &instance, has_layout ? &layout_result : nullptr};
        renderer->render(info, ctx);
    }
    
    return root;
}

} // namespace flex::modules::infographic
