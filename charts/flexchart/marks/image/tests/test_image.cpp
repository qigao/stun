#include "tinytest.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_image_force_link(void);
static auto _fl = (flexchart_image_force_link(), 0);

spec("flexchart_image") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("image");
            check(r != nullptr);
        }
    }

    describe("rendering") {
        it("should emit positioned image nodes") {
            flex::ArenaAllocator arena(64 * 1024);
            auto plot = flex::Group::create(arena);
            auto overlay = flex::Group::create(arena);
            std::vector<std::string> labels{"A"};
            flex::chart::MarkRenderContext ctx{arena, plot, overlay, 200, 100, 1, labels};
            auto mark = std::make_shared<flex::chart::AstMark>();
            for (const auto& channel : {"x", "y", "url"}) {
                auto enc = std::make_shared<flex::chart::AstEncoding>();
                enc->channel = channel;
                enc->field = channel;
                mark->encodings.push_back(enc);
            }
            mark->styles["img-width"] = 48.0;
            mark->styles["img-height"] = 24.0;
            std::vector<flex::chart::Record> records{
                {{{"x", std::string("A")}, {"y", 25.0}, {"url", std::string("icon.png")}}}};

            auto renderer = flex::chart::MarkRendererRegistry::instance().create("image");
            renderer->render(mark, records, ctx);

            check_int_eq(overlay->child_count(), 1);
            auto image = dynamic_cast<flex::Image*>(overlay->child_at(0));
            check(image != nullptr);
            check_string_eq(image->src(), "icon.png");
            check_float_eq(image->width(), 48.0f, 0.001f);
            check_float_eq(image->height(), 24.0f, 0.001f);
        }
    }
}
