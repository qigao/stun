#ifndef GCANVAS_GCANVAS_HPP
#define GCANVAS_GCANVAS_HPP

#define GCANVAS_LIBRARY_NAME "gCanvas"
#define GCANVAS_VERSION_MAJOR 0
#define GCANVAS_VERSION_MINOR 4
#define GCANVAS_VERSION_PATCH 0

#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)
#define GCANVAS_VERSION                                                                              \
    (GCANVAS_LIBRARY_NAME " " TO_STR(GCANVAS_VERSION_MAJOR) "." TO_STR(                                \
        GCANVAS_VERSION_MINOR) "." TO_STR(GCANVAS_VERSION_PATCH))

#ifdef GCANVAS_BUILD_SHARED
#ifdef _MSC_VER
#ifdef gcanvas_EXPORTS
#define GCANVAS_API __declspec(dllexport)
#else
#define GCANVAS_API __declspec(dllimport)
#endif
#else
#define GCANVAS_API __attribute__((visibility("default")))
#endif
#else
#define GCANVAS_API
#endif

#endif // GCANVAS_GCANVAS_HPP
