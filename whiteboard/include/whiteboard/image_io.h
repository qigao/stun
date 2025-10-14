#pragma once

extern "C" {
unsigned char *stbi_load(char const *filename, int *x, int *y, int *channels_in_file,
                         int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);
}

