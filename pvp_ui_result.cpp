// PvP result screen (spec C.7): result, points, trainer level, badge. The match is already settled
// (recorded on FINISHED), so nothing here can lose a result; OK only leaves the screen.
#include <stdio.h>

#include "pvp_app.h"
#include "pvp_time.h"
#include "pvp_ui.h"
#include "pvp_ui_battle_logic.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_phase.h"
#include "pvp_ui_screens.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

constexpr size_t LINE = 48;

void drawTitle(PvpResult result) {
  if (result == PVP_WIN) pvpDrawCentered(T(S_WIN), RESULT_TITLE_Y, 4, COL_OK);
  else if (result == PVP_LOSS) pvpDrawCentered(T(S_LOSS), RESULT_TITLE_Y, 4, COL_BAD);
  else pvpDrawCentered(TP(PVP_S_RESULT_DRAW), RESULT_TITLE_Y, 4, COL_WARN);
}

void drawRival(const PvpUiModel &m) {
  char lv[16];
  char line[LINE];
  pvpFormatLevel(lv, sizeof(lv), m.opponent.trainerLevel);
  snprintf(line, sizeof(line), "vs %s %s", m.opponent.nick, lv);
  pvpDrawCentered(line, RESULT_VS_Y, 2, COL_INK);
}

// Points of the match, or the reason there are none.
void drawPoints(const PvpUiModel &m) {
  char line[LINE];
  if (m.award.applied) {
    snprintf(line, sizeof(line), TP(PVP_S_POINTS_DELTA_FMT), (int)m.award.pointsDelta);
    uint16_t color = m.award.pointsDelta > 0 ? COL_OK : (m.award.pointsDelta < 0 ? COL_BAD : COL_WARN);
    pvpDrawCentered(line, RESULT_POINTS_Y, 3, color);
  } else if (m.award.blocked) {
    pvpDrawCentered(TP(pvpBlockedText(pvpProfileInstance().nowEpoch())), RESULT_BLOCKED_Y, 2, COL_WARN);
  }
}

void drawLevelAndBar(const PvpUiModel &m) {
  const TrainerStats &st = pet.trainerStats;
  char line[LINE];
  if (m.award.levelsGained > 0) {
    snprintf(line, sizeof(line), TP(PVP_S_LEVEL_UP_FMT), (unsigned)st.level);
    pvpDrawCentered(line, RESULT_LEVELUP_Y, 2, COL_GOLD);
  }
  uint16_t cost = trainerLevelCost(st.level);
  pvpDrawBar(CARD_BAR_X0, CARD_BAR_X1, RESULT_BAR_Y, RESULT_BAR_H, cost ? (int)((uint32_t)st.progress * 1000 / cost) : 1000,
             COL_OK);
  if (cost) snprintf(line, sizeof(line), TP(PVP_S_POINTS_FMT), (unsigned)st.progress, (unsigned)cost);
  else snprintf(line, sizeof(line), "%s", TP(PVP_S_LEVEL_MAX));
  pvpDrawCentered(line, RESULT_BAR_Y + RESULT_BAR_H + 4, 1, COL_INK);
}

void drawRounds(const PvpApp &app) {
  const PvpMatch &match = app.match();
  char fmt[16];
  char line[LINE];
  snprintf(fmt, sizeof(fmt), T(S_ROUND_SHORT_FMT), (unsigned)match.runtime().round);
  snprintf(line, sizeof(line), "%s  %u/%u", fmt, (unsigned)pvpMySide(match.runtime(), match.side()).damageTotal,
           (unsigned)pvpTheirSide(match.runtime(), match.side()).damageTotal);
  pvpDrawCentered(line, RESULT_STATS_Y, 2, COL_INK);
}

bool okReady(const PvpApp &app, uint32_t now) {
  return app.canLeaveNow(now) && pvpElapsed(now, pvpUiPhaseSinceMs(), PVP_RESULT_LOCK_MS);
}

}  // namespace

void pvpUiDrawResult(const PvpApp &app, uint32_t now) {
  const PvpUiModel &m = app.model();
  drawTitle(m.result);
  drawRival(m);
  drawPoints(m);
  drawLevelAndBar(m);
  if (m.badgeEarned) {
    char line[LINE];
    snprintf(line, sizeof(line), TP(PVP_S_BADGE_FMT), m.opponent.nick);
    pvpDrawCentered(line, RESULT_BADGE_Y, 2, COL_GOLD);
  }
  drawRounds(app);
  bool ready = okReady(app, now);
  pvpDrawButton(END_OK_X0, RESULT_OK_Y0, END_OK_X1, RESULT_OK_Y1, ready ? COL_OK : COL_MUTED, ready ? COL_BG : COL_INK,
                ready ? T(S_OK) : pvpDots(now));
}

void pvpUiTapResult(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (!pvpHit(x, y, END_OK_X0, RESULT_OK_Y0, END_OK_X1, RESULT_OK_Y1) || !okReady(app, now)) return;
  sfxPlay(SFX_TAP);
  app.leave(now);
}
