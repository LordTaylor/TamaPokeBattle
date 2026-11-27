// End screen without a result (spec C.8) and the T4.2 "PvP coming soon" stand-in for the battle phases.
#include <stdio.h>

#include "pvp_app.h"
#include "pvp_ui_bridge.h"
#include "pvp_strings.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_screens.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

struct EndText {
  PvpStrId reason;
  PvpStrId second;  // optional second line, PVP_STR_COUNT = none
  const char *code;  // technical code for the bug report (not translated), nullptr = none
  uint16_t color;
};

constexpr PvpStrId NONE = PVP_STR_COUNT;

EndText endText(PvpEndReason reason) {
  switch (reason) {
    case PVP_END_PEER_QUIT: return {PVP_S_ABORT_PEER_QUIT, NONE, nullptr, COL_WARN};
    case PVP_END_YOU_QUIT: return {PVP_S_ABORT_YOU_QUIT, NONE, nullptr, COL_WARN};
    case PVP_END_LINK: return {PVP_S_ABORT_LINK, PVP_S_NEAR_HINT, nullptr, COL_WARN};
    case PVP_END_NO_REPLY: return {PVP_S_NO_REPLY, NONE, nullptr, COL_WARN};
    case PVP_END_UPDATE: return {PVP_S_ABORT_UPDATE, NONE, nullptr, COL_BAD};
    case PVP_END_RADIO_FAIL: return {PVP_S_RADIO_FAIL, NONE, nullptr, COL_BAD};
    case PVP_END_SYNC: return {PVP_S_ABORT_GENERIC, PVP_S_TRY_AGAIN, "SYNC", COL_BAD};
    case PVP_END_STATS: return {PVP_S_ABORT_GENERIC, PVP_S_TRY_AGAIN, "STATS", COL_BAD};
    default: return {PVP_S_ABORT_GENERIC, PVP_S_TRY_AGAIN, "PROTO", COL_BAD};
  }
}

void drawOk(bool ready, uint32_t now) {
  const char *label = ready ? T(S_OK) : pvpDots(now);
  pvpDrawButton(END_OK_X0, END_OK_Y0, END_OK_X1, END_OK_Y1, ready ? COL_WARN : COL_MUTED, ready ? COL_BG : COL_INK,
                label);
}

bool tapOk(int16_t x, int16_t y) { return pvpHit(x, y, END_OK_X0, END_OK_Y0, END_OK_X1, END_OK_Y1); }

}  // namespace

void pvpUiDrawEnded(const PvpApp &app, uint32_t now) {
  const PvpUiModel &m = app.model();
  EndText text = endText(m.endReason);
  pvpDrawCentered(TP(PVP_S_ABORTED_TITLE), END_TITLE_Y, 2, COL_INK);
  if (text.code) pvpDrawCentered(text.code, END_CODE_Y, 1, COL_MUTED);
  pvpDrawCentered(TP(text.reason), END_REASON_Y, 2, text.color);
  if (text.second != NONE) pvpDrawCentered(TP(text.second), END_REASON2_Y, 2, text.color);
  if (!m.resultRecorded) pvpDrawCentered(TP(PVP_S_NO_POINTS), END_POINTS_Y, 2, COL_INK);
  if (m.quitPenalty) {
    char line[40];
    snprintf(line, sizeof(line), TP(PVP_S_QUIT_PENALTY_FMT), (unsigned)PVP_QUIT_PENALTY);
    pvpDrawCentered(line, END_PENALTY_Y, 2, COL_BAD);
  }
  drawOk(app.canLeaveNow(now), now);
}

void pvpUiTapEnded(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (!tapOk(x, y) || !app.canLeaveNow(now)) return;
  sfxPlay(SFX_TAP);
  app.leave(now);
}

void pvpUiDrawSoon(const PvpApp &app, uint32_t now) {
  pvpDrawCentered(TP(PVP_S_SOON), SOON_Y, 3, COL_INK);
  bool inBattle = app.model().phase != PVP_UI_RESULT;
  if (inBattle) {
    pvpDrawButton(END_OK_X0, END_OK_Y0, END_OK_X1, END_OK_Y1, COL_BAD, COL_WHITE, T(S_RUN_BATTLE));
  } else {
    drawOk(app.canLeaveNow(now), now);
  }
}

void pvpUiTapSoon(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (!tapOk(x, y)) return;
  if (app.model().phase == PVP_UI_RESULT) {
    if (!app.canLeaveNow(now)) return;
    sfxPlay(SFX_TAP);
    app.leave(now);
  } else {
    sfxPlay(SFX_TAP);
    app.quit(now);  // the battle flag is set at this point: the quit penalty applies (spec C.6)
  }
}
