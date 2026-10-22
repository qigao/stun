#ifndef GCANVAS_ANDROID_TEST_STB_IMAGE_WRITE_H
#define GCANVAS_ANDROID_TEST_STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

static inline int stbi_write_png(const char*, int, int, int, const void*, int)
{
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
