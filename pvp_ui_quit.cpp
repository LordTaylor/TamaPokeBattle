// Escape confirmation of the PvP battle (spec C.6): warns about the penalty, STAY is the bigger target.
#include <stdio.h>

#include "pvp_app.h"
#include "pvp_ui_battle_parts.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

bool modalOpen = false;

bool inRunningBattle(PvpPhase p) {
  return p == PVP_UI_CHOOSE || p == PVP_UI_WAIT_OPP || p == PVP_UI_WAIT_CONFIRM;
}

}  // namespace

// The modal never stops the battle: it closes by itself when the match ends (spec C.6).
bool pvpQuitOpen(const PvpApp &app) {
  if (modalOpen && !inRunningBattle(app.model().phase)) modalOpen = false;
  return modalOpen;
}

void pvpQuitShow() { modalOpen = true; }

void pvpQuitClose() { modalOpen = false; }

void pvpQuitDraw() {
  gfx->fillRoundRect(QUIT_X0, QUIT_Y0, QUIT_X1 - QUIT_X0, QUIT_Y1 - QUIT_Y0, PANEL_R, COL_WHITE);
  gfx->drawRoundRect(QUIT_X0, QUIT_Y0, QUIT_X1 - QUIT_X0, QUIT_Y1 - QUIT_Y0, PANEL_R, COL_INK);
  pvpDrawCentered(TP(PVP_S_QUIT_Q), QUIT_TITLE_Y, 2, COL_INK);
  char line[40];
  snprintf(line, sizeof(line), TP(PVP_S_QUIT_PENALTY_FMT), (unsigned)PVP_QUIT_PENALTY);
  pvpDrawCentered(line, QUIT_PENALTY_Y, 2, COL_BAD);
  pvpDrawButton(WIDE_BTN_X0, QUIT_RUN_Y0, WIDE_BTN_X1, QUIT_RUN_Y1, COL_BAD, COL_WHITE, T(S_RUN_BATTLE));
  pvpDrawButton(WIDE_BTN_X0, QUIT_STAY_Y0, WIDE_BTN_X1, QUIT_STAY_Y1, COL_OK, COL_BG, TP(PVP_S_QUIT_STAY));
}

void pvpQuitTap(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  // STAY is checked first: where the two hit boxes overlap the safe choice wins.
  if (pvpHit(x, y, WIDE_BTN_X0, QUIT_STAY_Y0, WIDE_BTN_X1, QUIT_STAY_Y1)) {
    sfxPlay(SFX_TAP);
    modalOpen = false;
  } else if (pvpHit(x, y, WIDE_BTN_X0, QUIT_RUN_Y0, WIDE_BTN_X1, QUIT_RUN_Y1)) {
    sfxPlay(SFX_TAP);
    modalOpen = false;
    app.quit(now);  // abort first, then recordQuit (PvpApp keeps that order)
  }
}
