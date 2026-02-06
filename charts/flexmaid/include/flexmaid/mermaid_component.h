#pragma once

#include "ir/unified_diagram.h"
#include "flexmaid.h"
#include <flex/runtime/group.h>
#include <string>

namespace flex {
class Instance;
}

namespace flex {
namespace modules {
namespace flexmaid {

class MermaidComponent {
public:
    static void register_component();
    
    static flex::Group* build(const UnifiedDiagram& diagram, flex::Instance& instance);
    static flex::Group* build(const UnifiedDiagram& diagram, const LayoutData& layout, 
                              flex::Instance& instance, const Theme& theme = Theme::light());
};

} // namespace flexmaid
} // namespace modules
} // namespace flex
