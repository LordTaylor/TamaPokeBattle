#include "pvp_ui_draw.h"

#include <stdio.h>
#include <string.h>

#include "pvp_ui_bridge.h"
#include "pvp_ui_text.h"
#include "pvp_ui_theme.h"

namespace {

constexpr size_t LINE_BUF = 48;

// Cut `text` to the chars that fit between y and y + glyph height; returns the fitted copy.
const char *fitted(const char *text, int y, int size, char *buf) {
  int maxChars = pvpMaxChars(y, y + pvpui::CHAR_PX_H * size, size);
  pvpFitText(text, buf, LINE_BUF, maxChars);
  return buf;
}

}  // namespace

void pvpDrawCentered(const char *text, int y, int size, uint16_t color) {
  char buf[LINE_BUF];
  const char *s = fitted(text, y, size, buf);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(pvpCenterX(pvpTextWidth((int)strlen(s), size)), y);
  gfx->print(s);
}

void pvpDrawLeft(const char *text, int x, int y, int size, uint16_t color) {
  char buf[LINE_BUF];
  const char *s = fitted(text, y, size, buf);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(s);
}

void pvpDrawRight(const char *text, int xRight, int y, int size, uint16_t color) {
  char buf[LINE_BUF];
  const char *s = fitted(text, y, size, buf);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(xRight - pvpTextWidth((int)strlen(s), size), y);
  gfx->print(s);
}

void pvpDrawButton(int x0, int y0, int x1, int y1, uint16_t fill, uint16_t textColor, const char *label) {
  gfx->fillRoundRect(x0, y0, x1 - x0, y1 - y0, pvpui::BTN_R, fill);
  int width = x1 - x0;
  int len = (int)strlen(label);
  int size = pvpTextWidth(len, 2) <= width - 2 * pvpui::EDGE_MARGIN ? 2 : 1;
  int textH = pvpui::CHAR_PX_H * size;
  gfx->setTextSize(size);
  gfx->setTextColor(textColor);
  gfx->setCursor(x0 + (width - pvpTextWidth(len, size)) / 2, y0 + (y1 - y0 - textH) / 2);
  gfx->print(label);
}

void pvpDrawBar(int x0, int x1, int y, int h, int permille, uint16_t color) {
  if (permille < 0) permille = 0;
  if (permille > 1000) permille = 1000;
  int w = x1 - x0;
  int fw = w * permille / 1000;
  gfx->fillRoundRect(x0, y, w, h, h / 3, pvpui::COL_MUTED);
  if (fw > 2) gfx->fillRoundRect(x0, y, fw, h, h / 3, color);
}

const char *pvpDots(uint32_t nowMs) {
  static const char *const DOTS[pvpui::DOTS_STEPS] = {"", ".", "..", "..."};
  return DOTS[(nowMs / pvpui::DOTS_PERIOD_MS) % pvpui::DOTS_STEPS];
}

bool pvpHit(int16_t x, int16_t y, int x0, int y0, int x1, int y1) {
  const int s = pvpui::HIT_SLOP;
  return x >= x0 - s && x <= x1 + s && y >= y0 - s && y <= y1 + s;
}

void pvpFormatLevel(char *out, size_t cap, uint8_t level) {
  snprintf(out, cap, T(S_LEVEL_SHORT_FMT), (unsigned)level);
}
