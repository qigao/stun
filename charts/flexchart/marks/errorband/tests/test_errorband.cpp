#include "tinytest.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_errorband_force_link(void);
static auto _fl = (flexchart_errorband_force_link(), 0);

spec("flexchart_errorband") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("errorband");
            check(r != nullptr);
        }
    }

    describe("rendering") {
        it("should emit a closed band shape") {
            flex::ArenaAllocator arena(64 * 1024);
            auto plot = flex::Group::create(arena);
            auto overlay = flex::Group::create(arena);
            std::vector<std::string> labels{"A", "B"};
            flex::chart::MarkRenderContext ctx{arena, plot, overlay, 200, 100, 1, labels};
            auto mark = std::make_shared<flex::chart::AstMark>();
            for (const auto& channel : {"x", "y", "y-error"}) {
                auto enc = std::make_shared<flex::chart::AstEncoding>();
                enc->channel = channel;
                enc->field = channel;
                mark->encodings.push_back(enc);
            }
            std::vector<flex::chart::Record> records{
                {{{"x", std::string("A")}, {"y", 30.0}, {"y-error", 5.0}}},
                {{{"x", std::string("B")}, {"y", 50.0}, {"y-error", 8.0}}}};

            auto renderer = flex::chart::MarkRendererRegistry::instance().create("errorband");
            renderer->render(mark, records, ctx);

            check_int_eq(overlay->child_count(), 1);
            check_str_eq(overlay->child_at(0)->id().c_str(), "errorband-mark-shape");
        }
    }
}
