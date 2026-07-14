#include "tinytest.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_point_force_link(void);
static auto _fl = (flexchart_point_force_link(), 0);

spec("flexchart_point") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("point");
            check(r != nullptr);
        }
    }
}
