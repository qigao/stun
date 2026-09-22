/**
 * stb_sprintf implementation
 * Single compilation unit for stb_sprintf library
 */

// Path formatting appends at arbitrary byte offsets in caller-owned buffers.
#define STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
