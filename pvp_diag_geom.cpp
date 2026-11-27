#ifdef TAMAPOKE_LOCAL_TEST

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "pvp_diag.h"
#include "pvp_diag_config.h"

extern Arduino_Canvas *gfx;  // framebuffer owned by TamaPoke.ino

// GEOM: calibration screen for the round 466x466 AMOLED. Three circles around the centre
// (red r=225, green r=233, yellow r=240), horizontal rulers at three heights with ticks
// every 20 px (long tick + x label every 100 px) and digit strips in text sizes 1/2/3
// running across the ruler row. Read off which circle/ruler/last digit is still fully
// visible and write it into docs/pvp-t40-measurements.md. Any serial input (or the
// timeout) closes the screen; the main UI is repainted by the caller.

namespace {
const uint16_t COLORS[3] = {RGB565_RED, RGB565_GREEN, RGB565_YELLOW};
const char *const COLOR_NAMES[3] = {"r225 red", "r233 green", "r240 yellow"};

void digitStrip(int x, int y, uint8_t size, int chars, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  for (int i = 0; i < chars; i++) gfx->print((char)('0' + (i % 10)));
}

void ruler(int y, uint16_t color) {
  gfx->drawFastHLine(0, y, PVP_DIAG_GEOM_SIZE, color);
  gfx->setTextSize(1);
  gfx->setTextColor(color);
  for (int x = 0; x < PVP_DIAG_GEOM_SIZE; x += PVP_DIAG_GEOM_TICK_STEP) {
    bool major = (x % 100) == 0;
    int len = major ? 10 : 4;
    gfx->drawFastVLine(x, y - len, len * 2 + 1, color);
    if (major) {
      gfx->setCursor(x + 2, y + 12);
      gfx->print(x);
    }
  }
  gfx->setCursor(PVP_DIAG_GEOM_SIZE / 2 - 12, y - 16);
  gfx->print("y=");
  gfx->print(y);
}
}  // namespace

void pvpDiagGeom() {
  const int c = PVP_DIAG_GEOM_CENTER;
  gfx->fillScreen(RGB565_BLACK);
  for (int i = 0; i < 3; i++) gfx->drawCircle(c, c, PVP_DIAG_GEOM_RADII[i], COLORS[i]);

  // Rulers: size 1 strip under y=30, size 2 under y=60, size 3 above y=430.
  ruler(PVP_DIAG_GEOM_RULER_Y[0], RGB565_CYAN);
  digitStrip(0, PVP_DIAG_GEOM_RULER_Y[0] + 22, 1, PVP_DIAG_GEOM_SIZE / 6 + 1, RGB565_WHITE);
  ruler(PVP_DIAG_GEOM_RULER_Y[1], RGB565_CYAN);
  digitStrip(0, PVP_DIAG_GEOM_RULER_Y[1] + 22, 2, PVP_DIAG_GEOM_SIZE / 12 + 1, RGB565_WHITE);
  ruler(PVP_DIAG_GEOM_RULER_Y[2], RGB565_CYAN);
  digitStrip(0, PVP_DIAG_GEOM_RULER_Y[2] - 34, 3, PVP_DIAG_GEOM_SIZE / 18 + 1, RGB565_WHITE);

  gfx->setTextSize(1);
  for (int i = 0; i < 3; i++) {
    gfx->setTextColor(COLORS[i]);
    gfx->setCursor(c - 30, c - 12 + i * 12);
    gfx->print(COLOR_NAMES[i]);
  }
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(c - 48, c + 30);
  gfx->print("serial input = exit");
  gfx->flush();

  Serial.println("GEOM on screen (press Enter in the serial monitor to close)");
  uint32_t t0 = millis();
  while (!Serial.available() && millis() - t0 < PVP_DIAG_GEOM_TIMEOUT_MS) delay(50);
  while (Serial.available()) Serial.read();
}

#endif
