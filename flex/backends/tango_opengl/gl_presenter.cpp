#include <backends/tango_opengl/gl_presenter.h>
#include <flex/render/engines/gcanvas.h>

#include <stdexcept>
#include <utility>

namespace flex::tango_opengl {

std::unique_ptr<CellPixelPresenter> create_external_gl_presenter(
    const ExternalGLCellCreateInfo& info) {
    if (!info.get_proc_address || info.font_family.empty()) {
        throw std::invalid_argument(
            "TangoOpenGL requires an explicit current GL context/loader and font identity");
    }
    // Use the existing gCanvas OpenGL shaders/atlas context without
    // registering the general OpenGL factory as TangoOpenGL.
    auto pixel = render::engines::gcanvas::create_external_opengl_renderer(
        info.get_proc_address, info.proc_user_data);
    if (!pixel) {
        throw std::runtime_error("TangoOpenGL failed to create gCanvas GL renderer");
    }
    if (!info.font_path.empty() &&
        !pixel->register_font(info.font_family, info.font_path)) {
        throw std::runtime_error("TangoOpenGL cannot register the exact requested font");
    }
    return std::make_unique<CellPixelPresenter>(std::move(pixel));
}

} // namespace flex::tango_opengl
