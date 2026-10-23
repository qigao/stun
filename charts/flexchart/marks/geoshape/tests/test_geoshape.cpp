#include "tinytest.h"
#include <string>
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_geoshape_force_link(void);
static auto _fl = (flexchart_geoshape_force_link(), 0);

spec("flexchart_geoshape") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("geoshape");
            check(r != nullptr);
        }
    }

    describe("rendering") {
        it("should emit a shape for GeoJSON polygon geometry") {
            flex::ArenaAllocator arena(64 * 1024);
            auto plot = flex::Group::create(arena);
            auto overlay = flex::Group::create(arena);
            std::vector<std::string> labels;
            flex::chart::MarkRenderContext ctx{arena, plot, overlay, 200, 100, 1, labels};
            auto mark = std::make_shared<flex::chart::AstMark>();
            auto enc = std::make_shared<flex::chart::AstEncoding>();
            enc->channel = "shape";
            enc->field = "geometry";
            mark->encodings.push_back(enc);
            std::vector<flex::chart::Record> records{
                {{{"geometry", std::string(
                    R"({"type":"Polygon","coordinates":[[[-10,0],[10,0],[0,20],[-10,0]]]})")}}}};

            auto renderer = flex::chart::MarkRendererRegistry::instance().create("geoshape");
            renderer->render(mark, records, ctx);

            check((overlay->child_count()) == (1));
            check(std::string(overlay->child_at(0)->type_name()) == std::string("Shape"));
        }
    }
}
