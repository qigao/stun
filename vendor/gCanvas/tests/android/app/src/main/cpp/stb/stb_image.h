#ifndef GCANVAS_ANDROID_TEST_STB_IMAGE_H
#define GCANVAS_ANDROID_TEST_STB_IMAGE_H

#define STBI_rgb_alpha 4

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char stbi_uc;

static inline stbi_uc* stbi_load(const char*, int*, int*, int*, int)
{
    return 0;
}

static inline void stbi_image_free(void*)
{
}

#ifdef __cplusplus
}
#endif

#endif
