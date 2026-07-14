#ifndef FLEXUI_EXAMPLES_RENDERER_CAPABILITY_LABEL_H
#define FLEXUI_EXAMPLES_RENDERER_CAPABILITY_LABEL_H

#include <flex/runtime/renderer.h>
#include <string>

namespace flexui_examples {

inline const char* on_off(bool value) {
    return value ? "on" : "off";
}

inline std::string renderer_capability_label(const flex::RendererCapabilities& caps) {
    const bool effects = caps.shadow || caps.blur;
    const bool transforms = caps.rotation || caps.scaling;
    return std::string("IMG:") + on_off(caps.raster_images) +
           " SVG:" + on_off(caps.svg_images) +
           " FX:" + on_off(effects) +
           " XFORM:" + on_off(transforms);
}

} // namespace flexui_examples

#endif // FLEXUI_EXAMPLES_RENDERER_CAPABILITY_LABEL_H
