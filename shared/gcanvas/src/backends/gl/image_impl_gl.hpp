#ifndef GCANVAS_IMAGE_IMPL_GL_HPP
#define GCANVAS_IMAGE_IMPL_GL_HPP

#include "gcanvas/image.hpp"
#include "gcanvas/context.hpp"

#include <map>
#include <string>
#include "gl_api.hpp"


namespace gcanvas
{
namespace GCANVAS_GL_PROFILE_NAMESPACE
{
    class ImageImplGl : public Image
    {
    public:
        ImageImplGl(std::string file_path, ImageConfig imageConfig);
        ImageImplGl(int width, int height, int components, const unsigned char* data,
                        std::size_t size, ImageConfig imageConfig);
        ~ImageImplGl();

        GLuint _image = 0;
        int _sampler_index = -1;
        uint32_t _mipLevels = 1;

        GLenum _type;
        GLint _internal_format;

        bool _uploaded = false;

        // Configured by the owning GL Context before the first native
        // texture upload. The borrowed observer must outlive this Image.
        void set_mutation_guard(
            NativeGpuMutationGuard guard, void* user_data) noexcept {
            mutation_guard_ = guard;
            mutation_user_data_ = user_data;
        }

        void upload();
        void upload_update();
        void bind(GLuint texture_unit);

    private:
        NativeGpuMutationGuard mutation_guard_ = nullptr;
        void* mutation_user_data_ = nullptr;
        std::size_t uploaded_bytes_ = 0;
        void admit_mutation(
            NativeGpuMutation operation, GLuint name, std::size_t bytes) const;
        void init_format();
    };

} // namespace GCANVAS_GL_PROFILE_NAMESPACE
} // namespace gcanvas

#endif // GCANVAS_IMAGE_IMPL_GL_HPP
