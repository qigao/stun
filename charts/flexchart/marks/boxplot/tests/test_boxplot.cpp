#include "tinytest.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_boxplot_force_link(void);
static auto _fl = (flexchart_boxplot_force_link(), 0);

spec("flexchart_boxplot") {
    describe("registration") {
        it("should be registered in the factory") {
            auto r = flex::chart::MarkRendererRegistry::instance().create("boxplot");
            check(r != nullptr);
        }
    }
}
