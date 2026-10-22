#include "tinytest.h"
#include <string>
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_trail_force_link(void);
static auto _fl = (flexchart_trail_force_link(), 0);

spec("flexchart_trail") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("trail");
            check(r != nullptr);
        }
    }

    describe("rendering") {
        it("should emit one shape for each trail segment") {
            flex::ArenaAllocator arena(64 * 1024);
            auto plot = flex::Group::create(arena);
            auto overlay = flex::Group::create(arena);
            std::vector<std::string> labels{"A", "B", "C"};
            flex::chart::MarkRenderContext ctx{arena, plot, overlay, 300, 100, 1, labels};
            auto mark = std::make_shared<flex::chart::AstMark>();
            for (const auto& channel : {"x", "y", "size"}) {
                auto enc = std::make_shared<flex::chart::AstEncoding>();
                enc->channel = channel;
                enc->field = channel;
                mark->encodings.push_back(enc);
            }
            std::vector<flex::chart::Record> records{
                {{{"x", std::string("A")}, {"y", 20.0}, {"size", 4.0}}},
                {{{"x", std::string("B")}, {"y", 40.0}, {"size", 8.0}}},
                {{{"x", std::string("C")}, {"y", 30.0}, {"size", 6.0}}}};

            auto renderer = flex::chart::MarkRendererRegistry::instance().create("trail");
            renderer->render(mark, records, ctx);

            check((overlay->child_count()) == (2));
            check(std::string(overlay->child_at(0)->type_name()) == std::string("Shape"));
        }
    }
}
