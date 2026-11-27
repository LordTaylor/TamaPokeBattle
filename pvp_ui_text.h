#pragma once

#include <stddef.h>

// Pure text / geometry helpers for the round 466x466 screen (no Arduino, host-testable).

// Half of the chord of the screen circle at height y (floor; 0 outside the circle).
int pvpChordHalfWidth(int y);

// Usable text width at height y: the full chord minus the margin on both sides (0 when none).
int pvpUsableWidth(int y);

// Width of `chars` characters at the given text size (6 px cell x size).
int pvpTextWidth(int chars, int textSize);

// How many characters of the given size fit between yTop and yBottom (the row farthest from the
// center decides).
int pvpMaxChars(int yTop, int yBottom, int textSize);

// X of a text of `textPx` pixels centered on the screen.
int pvpCenterX(int textPx);

// Copies src to dst, cutting it to `maxChars` visible characters (the last two become ".." when cut).
// Always NUL-terminated (when dstSize > 0); never writes past dstSize.
void pvpFitText(const char *src, char *dst, size_t dstSize, int maxChars);
