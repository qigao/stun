#pragma once

#include <ir/unified_infographic.h>
#include <flex/runtime/group.h>
#include <string>

namespace flex {
class Instance;
}

namespace flex::modules::infographic {

struct InfographicComponentBuildOptions {
    bool show_background = true;
    bool show_title = true;
};

class InfographicComponent {
public:
    static void register_component();
    /** @deprecated Use create_flexui_infographic for interactive UI. */
    static flex::Group* build(const UnifiedInfographic& infographic, flex::Instance& instance);
    /** @deprecated Use create_flexui_infographic for interactive UI. */
    static flex::Group* build(const UnifiedInfographic& infographic, flex::Instance& instance,
                              float width, float height);
    static flex::Group* build(const UnifiedInfographic& infographic,
                              flex::Instance& instance, float width,
                              float height,
                              const InfographicComponentBuildOptions& options);
};

} // namespace flex::modules::infographic
