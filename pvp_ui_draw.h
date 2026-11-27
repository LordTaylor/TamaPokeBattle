#pragma once

#include <stddef.h>
#include <stdint.h>

// Small drawing helpers shared by the PvP screens (firmware only, draw into gfx).
// Every text is cut to what fits the round screen at its row (pvp_ui_text.h).

void pvpDrawCentered(const char *text, int y, int size, uint16_t color);
void pvpDrawLeft(const char *text, int x, int y, int size, uint16_t color);
void pvpDrawRight(const char *text, int xRight, int y, int size, uint16_t color);

// Filled rounded button with a centered label (size 2, falls back to size 1 when it does not fit).
void pvpDrawButton(int x0, int y0, int x1, int y1, uint16_t fill, uint16_t textColor, const char *label);

// Horizontal bar, fraction in per-mille (0..1000).
void pvpDrawBar(int x0, int x1, int y, int h, int permille, uint16_t color);

// Animated dots ("", ".", "..", "...") as text, one more every DOTS_PERIOD_MS.
const char *pvpDots(uint32_t nowMs);

bool pvpHit(int16_t x, int16_t y, int x0, int y0, int x1, int y1);  // with PVP_UI HIT_SLOP

// "Lv.12" from the shared S_LEVEL_SHORT_FMT (each language keeps its own abbreviation).
void pvpFormatLevel(char *out, size_t cap, uint8_t level);
