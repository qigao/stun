#include "font_impl_opengl.hpp"

#include <iostream>
#include <vector>
#include <map>

#include "image_impl_opengl.hpp"

namespace gcanvas
{
    FontImplOpengl::FontImplOpengl(std::string file_path)
    {
        load_from_file(file_path);

        _loaded = true;
    }

    FontImplOpengl::FontImplOpengl(const unsigned char* buffer, size_t size)
    {
        load_from_memory(buffer, size);

        _loaded = true;
    }

    void FontImplOpengl::upload()
    {
        if (!_loaded)
            return;

        auto* iiv = static_cast<ImageImplOpengl*>(_texture_atlas.get());
        iiv->upload();

        _uploaded = true;
    }

    std::unique_ptr<Image> FontImplOpengl::create_atlas(int width, int height, int components,
                                                       const unsigned char* data)
    {
        const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                          static_cast<std::size_t>(components);
        return std::make_unique<ImageImplOpengl>(width, height, components, data, size,
                                                 ImageConfig{});
    }

} // namespace gcanvas
