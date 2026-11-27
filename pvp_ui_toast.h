#pragma once

#include <stdint.h>

#include "pvp_strings.h"

// One short message line (declined, expired, radio error ...) shown on the card page or the search list.
void pvpToastShow(PvpStrId id, uint16_t color, uint32_t nowMs, uint32_t durationMs);
// Text of the live toast or nullptr; `color` is set when a toast is live.
const char *pvpToastText(uint32_t nowMs, uint16_t &color);
// True once, right after a toast has run out (the screen needs one more redraw).
bool pvpToastJustExpired(uint32_t nowMs);
