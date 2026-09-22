#include <salts_unicode.h>

#include <cstddef>
#include <cstdint>

int main() {
  const char cluster_bytes[] = "e\xCC\x81X";
  const vstr input = vstr_from_buf(cluster_bytes, sizeof(cluster_bytes) - 1u);
  size_t cursor = 0u;
  vstr cluster{};
  if (salts_unicode_grapheme_next(input, &cursor, &cluster) != SALTS_UNICODE_OK)
    return 1;
  if (cursor != 3u || cluster.len != 3u)
    return 2;

  const char rtl[] = "\xD7\x90";
  uint8_t level = 9u;
  if (salts_unicode_bidi_paragraph_level(
          vstr_from_buf(rtl, sizeof(rtl) - 1u), &level) != SALTS_UNICODE_OK)
    return 3;
  return level == 1u ? 0 : 4;
}
