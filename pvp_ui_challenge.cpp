// Outgoing challenge, incoming challenge modal and preparation (spec C.3, C.4).
#include <stdio.h>

#include "pvp_app.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_phase.h"
#include "pvp_ui_screens.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

// Share of the 30 s decision window that is left, in per-mille.
int challengeLeftPermille(uint32_t now) {
  uint32_t spent = now - pvpUiPhaseSinceMs();
  if (spent >= PVP_CHALLENGE_TIMEOUT_MS) return 0;
  return (int)((PVP_CHALLENGE_TIMEOUT_MS - spent) * 1000 / PVP_CHALLENGE_TIMEOUT_MS);
}

void rivalLevelLine(const PvpPeerInfo &opp, char *out, size_t cap) {
  char lv[16];
  pvpFormatLevel(lv, sizeof(lv), opp.trainerLevel);
  snprintf(out, cap, "%s %s", TP(PVP_S_TRAINER_LABEL), lv);
}

void drawCancelButton(int y0, int y1) {
  pvpDrawButton(CANCEL_BTN_X0, y0, CANCEL_BTN_X1, y1, COL_MUTED, COL_INK, TP(PVP_S_CANCEL));
}

}  // namespace

void pvpUiDrawChallenging(const PvpApp &app, uint32_t now) {
  const PvpPeerInfo &opp = app.model().opponent;
  char line[48];
  snprintf(line, sizeof(line), TP(PVP_S_CHALLENGING_FMT), opp.nick);
  pvpDrawCentered(line, OUT_TITLE_Y, 2, COL_INK);
  rivalLevelLine(opp, line, sizeof(line));
  pvpDrawCentered(line, OUT_LEVEL_Y, 2, COL_INK);
  pvpDrawCentered(TP(PVP_S_WAITING_REPLY), OUT_STATUS_Y, 2, COL_INK);
  pvpDrawBar(CARD_BAR_X0, CARD_BAR_X1, OUT_BAR_Y, OUT_BAR_H, challengeLeftPermille(now), COL_WARN);
  drawCancelButton(CANCEL_BTN_WAIT_Y0, CANCEL_BTN_WAIT_Y1);
}

void pvpUiTapChallenging(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (!pvpHit(x, y, CANCEL_BTN_X0, CANCEL_BTN_WAIT_Y0, CANCEL_BTN_X1, CANCEL_BTN_WAIT_Y1)) return;
  sfxPlay(SFX_TAP);
  app.leave(now);  // abort before any battle flag: no penalty
}

void pvpUiDrawChallenged(const PvpApp &app, uint32_t now) {
  const PvpPeerInfo &opp = app.model().opponent;
  gfx->fillRoundRect(MODAL_X0, MODAL_Y0, MODAL_X1 - MODAL_X0, MODAL_Y1 - MODAL_Y0, PANEL_R, COL_WHITE);
  gfx->drawRoundRect(MODAL_X0, MODAL_Y0, MODAL_X1 - MODAL_X0, MODAL_Y1 - MODAL_Y0, PANEL_R, COL_INK);
  pvpDrawCentered(TP(PVP_S_CHALLENGE_Q), MODAL_TITLE_Y, 2, COL_INK);
  pvpDrawCentered(opp.nick, MODAL_NICK_Y, 3, COL_INK);
  char line[48];
  rivalLevelLine(opp, line, sizeof(line));
  pvpDrawCentered(line, MODAL_LEVEL_Y, 2, COL_INK);
  pvpDrawBar(CARD_BAR_X0, CARD_BAR_X1, MODAL_BAR_Y, MODAL_BAR_H, challengeLeftPermille(now), COL_WARN);
  pvpDrawButton(WIDE_BTN_X0, MODAL_ACCEPT_Y0, WIDE_BTN_X1, MODAL_ACCEPT_Y0 + WIDE_BTN_H, COL_OK, COL_WHITE,
                TP(PVP_S_ACCEPT));
  pvpDrawButton(WIDE_BTN_X0, MODAL_DECLINE_Y0, WIDE_BTN_X1, MODAL_DECLINE_Y0 + WIDE_BTN_H, COL_MUTED, COL_INK,
                TP(PVP_S_DECLINE));
}

void pvpUiTapChallenged(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (pvpHit(x, y, WIDE_BTN_X0, MODAL_ACCEPT_Y0, WIDE_BTN_X1, MODAL_ACCEPT_Y0 + WIDE_BTN_H)) {
    sfxPlay(SFX_TAP);
    app.respondChallenge(true, now);
  } else if (pvpHit(x, y, WIDE_BTN_X0, MODAL_DECLINE_Y0, WIDE_BTN_X1, MODAL_DECLINE_Y0 + WIDE_BTN_H)) {
    sfxPlay(SFX_TAP);
    app.respondChallenge(false, now);
  }
}

void pvpUiDrawPreparing(const PvpApp &app, uint32_t now) {
  pvpDrawCentered(TP(PVP_S_PREPARING), PREP_TITLE_Y, 2, COL_INK);
  pvpDrawCentered(pvpDots(now), PREP_DOTS_Y, 2, COL_INK);
  const PvpPeerInfo &opp = app.model().opponent;
  char lv[16], line[48];
  pvpFormatLevel(lv, sizeof(lv), opp.trainerLevel);
  snprintf(line, sizeof(line), "%s  %s", opp.nick, lv);
  pvpDrawCentered(line, PREP_RIVAL_Y, 2, COL_INK);
  drawCancelButton(CANCEL_BTN_WAIT_Y0, CANCEL_BTN_WAIT_Y1);
}

void pvpUiTapPreparing(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (!pvpHit(x, y, CANCEL_BTN_X0, CANCEL_BTN_WAIT_Y0, CANCEL_BTN_X1, CANCEL_BTN_WAIT_Y1)) return;
  sfxPlay(SFX_TAP);
  app.leave(now);  // PREPARING: the battle flag is not set yet
}
