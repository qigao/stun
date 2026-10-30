#ifndef GCANVAS_ANDROID_TEST_STB_IMAGE_H
#define GCANVAS_ANDROID_TEST_STB_IMAGE_H

#define STBI_rgb_alpha 4

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char stbi_uc;

/* Android lifecycle compile fixture only: expose the same STB metadata
 * symbol used by the production decoder; this intentionally does NOT parse
 * files or simulate an accepted image. The full STB decoder belongs to the
 * actual gCanvas package and real Mesa qualification.
 */
static inline int stbi_info(const char*, int*, int*, int*)
{
    return 0;
}

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
