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
// true (T4.3): the battle, result and badge screens exist; the T4.2 "PvP coming soon" guard is off.
constexpr bool BATTLE_UI_READY = true;

// --- Timing ---
constexpr uint32_t RENDER_INTERVAL_MS = 200;
constexpr uint32_t NOTICE_MS = 2000;
constexpr uint32_t NOTICE_VERSION_MS = 3000;
constexpr uint32_t WAKE_TOUCH_LOCK_MS = 300;
constexpr uint32_t DOTS_PERIOD_MS = 400;
constexpr uint8_t DOTS_STEPS = 4;
constexpr uint32_t BATTLE_WARN_LEFT_MS = 5000;  // one SFX_MENU this long before the round clock runs out
constexpr uint32_t BATTLE_DMG_FLASH_MS = 1000;  // "-N" next to the HP bar after a round
constexpr uint32_t BATTLE_NOTE_MS = 1500;       // "no rest left" line
constexpr uint32_t RESULT_BADGE_DELAY_MS = 400;  // SFX_MEDAL after the result jingle
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
constexpr uint16_t COL_INK_NIGHT = UI_INK_NIGHT;
constexpr uint16_t COL_ACT_ATTACK = UI_BAR_BAD;
constexpr uint16_t COL_ACT_QUICK = UI_BAR_WARN;
constexpr uint16_t COL_ACT_HEAVY = C565(0xB8, 0x3A, 0x28);
constexpr uint16_t COL_ACT_DODGE = 0x4C98;
constexpr uint16_t COL_ACT_REST = UI_BAR_OK;

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

// --- Battle screen (C.5) ---
constexpr int BATTLE_ROUND_Y = 26;
constexpr int BATTLE_TIMEBAR_X0 = 133;
constexpr int BATTLE_TIMEBAR_X1 = 333;
constexpr int BATTLE_TIMEBAR_Y = 54;
constexpr int BATTLE_TIMEBAR_H = 6;
constexpr int BATTLE_NAME_Y = 72;
constexpr int BATTLE_NAME_X_L = 76;
constexpr int BATTLE_NAME_X_R = 390;  // right edge
constexpr int BATTLE_LV_Y = 92;
constexpr int BATTLE_HPBAR_Y = 112;
constexpr int BATTLE_HPBAR_H = 14;
constexpr int BATTLE_HPBAR_W = 116;
constexpr int BATTLE_HPBAR_L_X = 68;
constexpr int BATTLE_HPBAR_R_X = 282;
constexpr int BATTLE_HPNUM_Y = 130;
constexpr int BATTLE_CHIPS_Y = 140;
constexpr int BATTLE_SPRITE_L_X = 142;
constexpr int BATTLE_SPRITE_R_X = 328;
constexpr int BATTLE_SPRITE_GROUND_Y = 256;
constexpr int BATTLE_SPRITE_SCALE = 3;
constexpr int BATTLE_THUMB_L_X = 94;
constexpr int BATTLE_THUMB_R_X = 280;
constexpr int BATTLE_THUMB_Y = 160;
constexpr int BATTLE_LOG1_Y = 262;
constexpr int BATTLE_LOG2_Y = 280;
constexpr int BATTLE_QUIT_X0 = 190;
constexpr int BATTLE_QUIT_X1 = 276;
constexpr int BATTLE_QUIT_Y0 = 106;
constexpr int BATTLE_QUIT_Y1 = 134;
constexpr int BATTLE_ALT_X[2] = {74, 242};  // QUICK / HEAVY, always visible
constexpr int BATTLE_ALT_W = 150;
constexpr int BATTLE_ALT_Y0 = 300;
constexpr int BATTLE_ALT_H = 46;
constexpr int BATTLE_MAIN_X[3] = {58, 179, 300};  // ATTACK / DODGE / REST, same as PvE
constexpr int BATTLE_MAIN_W = 108;
constexpr int BATTLE_MAIN_Y0 = 358;
constexpr int BATTLE_MAIN_H = 58;
constexpr int BATTLE_PICK_OUTLINE = 3;
constexpr int BATTLE_REST_PILL_R = 11;
constexpr int BATTLE_NIGHT_FROM_HOUR = 20;
constexpr int BATTLE_NIGHT_TO_HOUR = 6;

// --- Quit modal (C.6) ---
constexpr int QUIT_X0 = 70;
constexpr int QUIT_X1 = 396;
constexpr int QUIT_Y0 = 150;
constexpr int QUIT_Y1 = 340;
constexpr int QUIT_TITLE_Y = 166;
constexpr int QUIT_PENALTY_Y = 200;
constexpr int QUIT_RUN_Y0 = 226;
constexpr int QUIT_RUN_Y1 = 270;
constexpr int QUIT_STAY_Y0 = 280;
constexpr int QUIT_STAY_Y1 = 328;

// --- Result screen (C.7) ---
constexpr int RESULT_TITLE_Y = 40;
constexpr int RESULT_VS_Y = 84;
constexpr int RESULT_POINTS_Y = 118;
constexpr int RESULT_BLOCKED_Y = 150;
constexpr int RESULT_LEVELUP_Y = 176;
constexpr int RESULT_BAR_Y = 204;
constexpr int RESULT_BAR_H = 12;
constexpr int RESULT_BADGE_Y = 240;
constexpr int RESULT_STATS_Y = 290;
constexpr int RESULT_OK_Y0 = 396;
constexpr int RESULT_OK_Y1 = 448;

// --- Badges sub-view (C.9) ---
constexpr int BADGES_TITLE_Y = 44;
constexpr int BADGES_ROW_X0 = 68;
constexpr int BADGES_ROW_X1 = 398;
constexpr int BADGES_ROW_Y0 = 84;
constexpr int BADGES_ROW_STEP = 52;
constexpr int BADGES_ROW_H = 44;
constexpr uint8_t BADGES_PER_PAGE = 5;
constexpr int BADGES_ROW_PAD = 10;
constexpr int BADGES_STAR_R = 9;
constexpr int BADGES_NAV_Y0 = 348;
constexpr int BADGES_NAV_Y1 = 386;
constexpr int BADGES_NAV_L_X0 = 76;
constexpr int BADGES_NAV_L_X1 = 170;
constexpr int BADGES_NAV_R_X0 = 296;
constexpr int BADGES_NAV_R_X1 = 390;
constexpr int BADGES_EMPTY_Y = 200;
constexpr int CARD_BADGES_X0 = 150;  // badge counter hit box on card page 10
constexpr int CARD_BADGES_X1 = 316;
constexpr int CARD_BADGES_Y0 = 56;
constexpr int CARD_BADGES_Y1 = 76;
constexpr int CARD_TILES_X0 = 50;
constexpr int CARD_TILES_X1 = 416;
constexpr int CARD_TILES_Y0 = 166;
constexpr int CARD_TILES_Y1 = 218;

}  // namespace pvpui
