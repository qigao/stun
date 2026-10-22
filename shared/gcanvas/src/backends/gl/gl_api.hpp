#ifndef GCANVAS_GL_API_HPP
#define GCANVAS_GL_API_HPP

#if defined(GCANVAS_GL_DESKTOP) && defined(GCANVAS_GL_GLES)
#error "Exactly one gCanvas GL profile must be selected"
#elif defined(GCANVAS_GL_DESKTOP)
#define GCANVAS_GL_PROFILE_NAMESPACE desktop_gl
#define GCANVAS_GL_SHADER_BATCH_CAPACITY 128
#include <glad/glad.h>
#elif defined(GCANVAS_GL_GLES)
#define GCANVAS_GL_PROFILE_NAMESPACE opengles_gl
#define GCANVAS_GL_SHADER_BATCH_CAPACITY 16
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#error "A gCanvas GL profile must define GCANVAS_GL_DESKTOP or GCANVAS_GL_GLES"
#endif

#endif // GCANVAS_GL_API_HPP
