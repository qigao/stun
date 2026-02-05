#include <infographic_component.h>
#include <layout_renderer.h>
#include <render_helpers.h>
#include <flex/runtime/group.h>
#include <flex/runtime/shape.h>
#include <flex/runtime/text.h>
#include <flex/runtime/component.h>
#include <flex.h>

namespace flex::modules::infographic {

void InfographicComponent::register_component() {
    flex::ComponentRegistry::instance().register_component("Infographic", 
        [](const flex::Props& props) -> std::shared_ptr<flex::Node> {
            return nullptr;
        });
}

flex::Group* InfographicComponent::build(const UnifiedInfographic& info, flex::Instance& instance) {
    flex::ArenaAllocator& arena = *instance.object_allocator();
    
    auto root = flex::Group::create(arena);
    root->set_id("infographic-root");
    root->set_layout_size(600, 500);
    
    // Background
    auto bg = arena.create<flex::Shape>();
    bg->set_rect(600, 500);
    bg->set_fill(flex::Color(0.98f, 0.98f, 0.98f));
    root->add_child(bg);
    
    // Title
    if (info.title) {
        auto title = arena.create<flex::Text>();
        title->set_content(*info.title);
        title->set_font_size(24.0f);
        title->set_font_weight(flex::FontWeight::Bold);
        title->set_color(flex::Color(0.15f, 0.15f, 0.15f));
        title->set_position(300, 30);
        title->set_anchor(flex::Anchor::Top);
        root->add_child(title);
    }
    
    // Get layout type and create renderer
    LayoutType layout = get_layout_type(info.template_type);
    auto renderer = LayoutRendererFactory::create(layout);
    
    if (renderer) {
        LayoutRenderContext ctx{arena, root, 600, 500, &instance};
        renderer->render(info, ctx);
    }
    
    return root;
}

} // namespace flex::modules::infographic
