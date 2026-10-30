#include "font_impl_gl.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <limits>
#include <stdexcept>

#include "image_impl_gl.hpp"

namespace gcanvas
{
namespace GCANVAS_GL_PROFILE_NAMESPACE
{
    FontImplGl::FontImplGl(std::string file_path, Context& context)
        : creation_context_(&context)
    {
        load_from_file(file_path);

        _loaded = true;
    }

    FontImplGl::FontImplGl(const unsigned char* buffer, size_t size, Context& context)
        : creation_context_(&context)
    {
        load_from_memory(buffer, size);

        _loaded = true;
    }

    void FontImplGl::admit_atlas_creation(std::size_t atlas_bytes) const
    {
        if (!creation_context_)
            throw std::logic_error(
                "gCanvas GL font atlas has no creation budget owner");
        if (atlas_bytes >
            (std::numeric_limits<std::size_t>::max)() - font_file_staging_bytes_)
            throw std::overflow_error(
                "gCanvas font atlas/source-file transient bytes overflow");
        // Atlas staging + retained font-file bytes, a persistent Image copy
        // and the newly allocated GL base texture payload are distinct.
        (void)creation_context_->image_creation_peak_usage(
            atlas_bytes, atlas_bytes + font_file_staging_bytes_, atlas_bytes);
    }

    void FontImplGl::upload()
    {
        if (!_loaded)
            return;

        auto* iiv = static_cast<ImageImplGl*>(_texture_atlas.get());
        iiv->upload();

        _uploaded = true;
    }

    std::unique_ptr<Image> FontImplGl::create_atlas(int width, int height, int components,
                                                       const unsigned char* data)
    {
        const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                          static_cast<std::size_t>(components);
        return std::make_unique<ImageImplGl>(width, height, components, data, size,
                                                 ImageConfig{});
    }

} // namespace GCANVAS_GL_PROFILE_NAMESPACE
} // namespace gcanvas
