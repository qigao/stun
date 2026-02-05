#pragma once

#include <ir/unified_infographic.h>
#include <flex/runtime/group.h>
#include <string>

namespace flex {
class Instance;
}

namespace flex::modules::infographic {

class InfographicComponent {
public:
    static void register_component();
    static flex::Group* build(const UnifiedInfographic& infographic, flex::Instance& instance);
};

} // namespace flex::modules::infographic
