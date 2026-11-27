#include "pvp_ui_text.h"

#include <string.h>

#include "pvp_ui_theme.h"

namespace {

// floor(sqrt(v)) for v >= 0
int isqrt(long v) {
  if (v <= 0) return 0;
  long r = 0;
  while ((r + 1) * (r + 1) <= v) r++;
  return (int)r;
}

long chordSquare(int y) {
  long dy = y - pvpui::SCREEN_CY;
  long r = pvpui::SCREEN_R;
  return r * r - dy * dy;
}

}  // namespace

int pvpChordHalfWidth(int y) { return isqrt(chordSquare(y)); }

int pvpUsableWidth(int y) {
  int w = isqrt(4 * chordSquare(y)) - 2 * pvpui::EDGE_MARGIN;  // 2*sqrt(.) without early rounding
  return w > 0 ? w : 0;
}

int pvpTextWidth(int chars, int textSize) { return chars * pvpui::CHAR_PX * textSize; }

int pvpMaxChars(int yTop, int yBottom, int textSize) {
  if (textSize <= 0) return 0;
  int dTop = yTop > pvpui::SCREEN_CY ? yTop - pvpui::SCREEN_CY : pvpui::SCREEN_CY - yTop;
  int dBottom = yBottom > pvpui::SCREEN_CY ? yBottom - pvpui::SCREEN_CY : pvpui::SCREEN_CY - yBottom;
  int farthest = dTop >= dBottom ? yTop : yBottom;
  return pvpUsableWidth(farthest) / (pvpui::CHAR_PX * textSize);
}

int pvpCenterX(int textPx) { return pvpui::SCREEN_CX - textPx / 2; }

void pvpFitText(const char *src, char *dst, size_t dstSize, int maxChars) {
  if (dstSize == 0) return;
  size_t cap = dstSize - 1;
  size_t limit = maxChars > 0 ? (size_t)maxChars : 0;
  if (limit > cap) limit = cap;
  size_t len = strlen(src);
  if (len <= limit) {
    memcpy(dst, src, len);
    dst[len] = 0;
    return;
  }
  size_t keep = limit > 2 ? limit - 2 : 0;
  memcpy(dst, src, keep);
  size_t dots = limit - keep;
  for (size_t i = 0; i < dots; i++) dst[keep + i] = '.';
  dst[keep + dots] = 0;
}
