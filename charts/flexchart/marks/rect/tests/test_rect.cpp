#include "tinytest.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_rect_force_link(void);
static auto _fl = (flexchart_rect_force_link(), 0);

spec("flexchart_rect") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("rect");
            check(r != nullptr);
        }
    }
}
