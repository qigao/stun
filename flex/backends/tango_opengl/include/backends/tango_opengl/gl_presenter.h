/*
 * Dedicated Tango cell-grid OpenGL presenter host.
 * This is NOT the general Flex OpenGL renderer factory and does not
 * register RendererBackend::TangoOpenGL before real application acceptance.
 */
#pragma once
#include <backends/tango_opengl/cell_presenter.h>
#include <backends/opengl/init.h>

#include <memory>
#include <string>

namespace flex::tango_opengl {

struct ExternalGLCellCreateInfo {
    // Host-owned, compatible OpenGL context MUST be current during create,
    // present and destruction. The callback user_data must outlive this
    // presenter. The host owns window buffer swapping.
    opengl_backend::OpenGLProcLoader get_proc_address = nullptr;
    void* proc_user_data = nullptr;
    // Both regular and bold font files are mandatory. Empty/missing paths
    // must fail before a frame instead of silently falling back to gCanvas's
    // default font or pretending that regular glyphs are bold glyphs.
    std::string font_family = "tango-monospace";
    std::string font_path;
    std::string bold_font_path;
};

// Owns gCanvas's real GL renderer and shader/glyph resources. It draws
// cell-pixel frame plans; native GPU/in-flight retirement and app/window
// qualification remain explicit #342 gates.
std::unique_ptr<CellPixelPresenter> create_external_gl_presenter(
    const ExternalGLCellCreateInfo& info);

} // namespace flex::tango_opengl
