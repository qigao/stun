#pragma once

#include <flex/modules/flexmaid/ir/unified_diagram.h>
#include <flex/modules/flexmaid/flexmaid.h>
#include <flex/runtime/group.h>
#include <string>

namespace flex {
class Instance;
}

namespace flex::modules::flexmaid {

class MermaidComponent {
public:
    static void register_component();
    
    static flex::Group* build(const UnifiedDiagram& diagram, flex::Instance& instance);
    static flex::Group* build(const UnifiedDiagram& diagram, const LayoutData& layout, 
                              flex::Instance& instance, const Theme& theme = Theme::light());
};

} // namespace flex::modules::flexmaid
