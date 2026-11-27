#pragma once

#include <stdint.h>

#include "species.h"  // UI_* colors (generated, read only)

// PvP UI constants: one source for geometry, timing and colors (spec C.0-C.4).
// All coordinates are proposals until checked on the board (spec G.2).

#ifndef C565
#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#endif

namespace pvpui {

// --- Round screen (same center as CX/CY in TamaPoke.ino) ---
constexpr int SCREEN_CX = 233;
constexpr int SCREEN_CY = 233;
constexpr int SCREEN_R = 233;
constexpr int EDGE_MARGIN = 8;
constexpr int CHAR_PX = 6;  // font cell width at text size 1
constexpr int CHAR_PX_H = 8;  // font cell height at text size 1
constexpr int HIT_SLOP = 7;

// --- Stage switch ---
// false (T4.2): the battle UI does not exist yet, so the UI leaves the match as soon as the rival's
// READY is validated (no BATTLE_START flag, no penalty). Flip to true together with pvp_ui_battle.cpp (T4.3).
constexpr bool BATTLE_UI_READY = false;

// --- Timing ---
constexpr uint32_t RENDER_INTERVAL_MS = 200;
constexpr uint32_t NOTICE_MS = 2000;
constexpr uint32_t NOTICE_VERSION_MS = 3000;
constexpr uint32_t WAKE_TOUCH_LOCK_MS = 300;
constexpr uint32_t DOTS_PERIOD_MS = 400;
constexpr uint8_t DOTS_STEPS = 4;
constexpr uint32_t PULSE_PERIOD_MS = 20;
constexpr int PULSE_R_MIN = 20;
constexpr int PULSE_R_RANGE = 40;

// --- Colors (aliases on the generated UI_*) ---
constexpr uint16_t COL_BG = UI_BG_DAY;
constexpr uint16_t COL_INK = UI_INK;
constexpr uint16_t COL_MUTED = UI_TRACK;
constexpr uint16_t COL_OK = UI_BAR_OK;
constexpr uint16_t COL_WARN = UI_BAR_WARN;
constexpr uint16_t COL_BAD = UI_BAR_BAD;
constexpr uint16_t COL_WHITE = UI_WHITE;
constexpr uint16_t COL_GOLD = UI_BAR_WARN;

// --- Generic widgets ---
constexpr int BTN_R = 12;
constexpr int PANEL_R = 18;

// --- Card page 10 (C.1) ---
constexpr int CARD_NICK_Y = 34;
constexpr int CARD_BADGES_Y = 62;
constexpr int CARD_TRAINER_Y = 78;
constexpr int CARD_BAR_Y = 106;
constexpr int CARD_BAR_X0 = 93;
constexpr int CARD_BAR_X1 = 373;
constexpr int CARD_BAR_H = 12;
constexpr int CARD_POINTS_Y = 134;
constexpr int CARD_TILE_Y = 166;
constexpr int CARD_TILE_W = 104;
constexpr int CARD_TILE_H = 52;
constexpr int CARD_TILE_X[3] = {50, 181, 312};
constexpr int CARD_TILE_NUM_DY = 8;
constexpr int CARD_TILE_LABEL_DY = 36;
constexpr int CARD_DOTS_Y = 244;
constexpr int CARD_DOT_R = 8;
constexpr int CARD_DOT_STEP = 28;
constexpr int CARD_STEPS_Y = 270;
constexpr int CARD_BTN_X0 = 76;
constexpr int CARD_BTN_X1 = 390;
constexpr int CARD_BTN_Y0 = 312;
constexpr int CARD_BTN_Y1 = 364;
constexpr int CARD_HINT_Y = 374;
constexpr int CARD_BACK_Y = 400;  // taps at or below go to the generic "back"

// --- Search list (C.2) ---
constexpr int SEARCH_TITLE_Y = 44;
constexpr int SEARCH_DOTS_Y = 64;
constexpr int SEARCH_HINT_Y = 84;
constexpr int SEARCH_ROW_X = 60;
constexpr int SEARCH_ROW_W = 346;
constexpr int SEARCH_ROW_H = 50;
constexpr int SEARCH_ROW_Y0 = 100;
constexpr int SEARCH_ROW_STEP = 58;
constexpr uint8_t SEARCH_ROWS_PER_PAGE = 4;
constexpr int SEARCH_PAGER_Y = 336;
constexpr int SEARCH_ROW_PAD = 14;
constexpr int SEARCH_PULSE_Y = 200;
constexpr int SEARCH_NONE_Y = 290;

// --- Shared buttons (CANCEL / OK / ACCEPT ...) ---
constexpr int WIDE_BTN_X0 = 93;   // 280 wide, as the wild prompt buttons
constexpr int WIDE_BTN_X1 = 373;
constexpr int WIDE_BTN_H = 44;
constexpr int CANCEL_BTN_X0 = 133;
constexpr int CANCEL_BTN_X1 = 333;
constexpr int CANCEL_BTN_Y0 = 372;  // search list
constexpr int CANCEL_BTN_Y1 = 416;
constexpr int CANCEL_BTN_WAIT_Y0 = 300;  // challenge / preparation
constexpr int CANCEL_BTN_WAIT_Y1 = 344;

// --- Outgoing challenge / preparation (C.3, C.4) ---
constexpr int OUT_TITLE_Y = 140;
constexpr int OUT_LEVEL_Y = 172;
constexpr int OUT_STATUS_Y = 206;
constexpr int OUT_BAR_Y = 236;
constexpr int OUT_BAR_H = 10;
constexpr int PREP_TITLE_Y = 190;
constexpr int PREP_DOTS_Y = 214;
constexpr int PREP_RIVAL_Y = 250;

// --- Incoming challenge modal (C.3) ---
constexpr int MODAL_X0 = 70;
constexpr int MODAL_X1 = 396;
constexpr int MODAL_Y0 = 130;
constexpr int MODAL_Y1 = 360;
constexpr int MODAL_TITLE_Y = 146;
constexpr int MODAL_NICK_Y = 172;
constexpr int MODAL_LEVEL_Y = 204;
constexpr int MODAL_BAR_Y = 230;
constexpr int MODAL_BAR_H = 8;
constexpr int MODAL_ACCEPT_Y0 = 250;
constexpr int MODAL_DECLINE_Y0 = 302;

// --- End / refusal screen (C.8) ---
constexpr int END_TITLE_Y = 70;
constexpr int END_REASON_Y = 130;
constexpr int END_REASON2_Y = 154;
constexpr int END_POINTS_Y = 184;
constexpr int END_PENALTY_Y = 208;
constexpr int END_CODE_Y = 48;
constexpr int END_OK_Y0 = 300;
constexpr int END_OK_Y1 = 350;
constexpr int END_OK_X0 = 118;
constexpr int END_OK_X1 = 348;
constexpr int SOON_Y = 200;

}  // namespace pvpui
