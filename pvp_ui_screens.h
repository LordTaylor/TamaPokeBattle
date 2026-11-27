#pragma once

#include <stdint.h>

// Per-phase PvP screens (firmware only). Each draw function paints one frame body into gfx
// (the caller clears the screen and flushes); each tap function reacts to a touch in its phase.

class PvpApp;

void pvpUiDrawSearch(const PvpApp &app, uint32_t now);
void pvpUiTapSearch(PvpApp &app, int16_t x, int16_t y, uint32_t now);
void pvpUiSwipeSearch(PvpApp &app, int dir, uint32_t now);  // dir > 0 = right

void pvpUiDrawChallenging(const PvpApp &app, uint32_t now);
void pvpUiTapChallenging(PvpApp &app, int16_t x, int16_t y, uint32_t now);
void pvpUiDrawChallenged(const PvpApp &app, uint32_t now);
void pvpUiTapChallenged(PvpApp &app, int16_t x, int16_t y, uint32_t now);
void pvpUiDrawPreparing(const PvpApp &app, uint32_t now);
void pvpUiTapPreparing(PvpApp &app, int16_t x, int16_t y, uint32_t now);

void pvpUiDrawEnded(const PvpApp &app, uint32_t now);
void pvpUiTapEnded(PvpApp &app, int16_t x, int16_t y, uint32_t now);

// Battle (CHOOSE .. WAIT_FINAL, incl. the quit modal) and result.
void pvpUiDrawBattle(const PvpApp &app, uint32_t now);
void pvpUiTapBattle(PvpApp &app, int16_t x, int16_t y, uint32_t now);
void pvpUiDrawResult(const PvpApp &app, uint32_t now);
void pvpUiTapResult(PvpApp &app, int16_t x, int16_t y, uint32_t now);

// Badge list: sub-view of card page 10 (pvp_ui_badges.cpp).
bool pvpBadgesViewOpen();
void pvpBadgesOpen();
void pvpBadgesClose();
void pvpBadgesDraw();
bool pvpBadgesTap(int16_t x, int16_t y);  // always true while the view is open
