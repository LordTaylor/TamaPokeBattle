// Top half of the PvP battle screen (spec C.5): round clock, names, HP bars, type chips, sprites.
#include <stdio.h>

#include "battle_core.h"
#include "pvp_ui_battle_logic.h"
#include "pvp_ui_battle_parts.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_phase.h"
#include "pvp_time.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

uint16_t toneColor(PvpTone tone) {
  if (tone == PVP_TONE_BAD) return COL_BAD;
  if (tone == PVP_TONE_WARN) return COL_WARN;
  return COL_OK;
}

void drawHpBar(int x, int w, int hp, int maxHp, uint16_t color) {
  if (maxHp <= 0) maxHp = 1;
  pvpDrawBar(x, x + w, BATTLE_HPBAR_Y, BATTLE_HPBAR_H, hp * 1000 / maxHp, color);
}

void drawHpNumber(const PvpSide &side, int x, bool alignRight, uint16_t ink) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u/%u", (unsigned)side.hp, (unsigned)side.maxHp);
  if (alignRight) pvpDrawRight(buf, x, BATTLE_HPNUM_Y, 1, ink);
  else pvpDrawLeft(buf, x, BATTLE_HPNUM_Y, 1, ink);
}

void drawDamage(const PvpSideTurn &dealer, int x, bool alignRight) {
  if (dealer.damageDealt == 0) return;
  char buf[12];
  snprintf(buf, sizeof(buf), "-%u", (unsigned)dealer.damageDealt);
  if (alignRight) pvpDrawRight(buf, x, BATTLE_HPNUM_Y + CHAR_PX_H + 2, 1, COL_BAD);
  else pvpDrawLeft(buf, x, BATTLE_HPNUM_Y + CHAR_PX_H + 2, 1, COL_BAD);
}

void drawNames(const PvpApp &app, const PvpSide &mine, uint16_t ink) {
  const PvpUiModel &m = app.model();
  pvpDrawLeft(pet.nick[0] ? pet.nick : dexName(pet.speciesId), BATTLE_NAME_X_L, BATTLE_NAME_Y, 2, ink);
  pvpDrawRight(m.opponent.nick, BATTLE_NAME_X_R, BATTLE_NAME_Y, 2, ink);
  char lv[16];
  char buf[40];
  pvpFormatLevel(lv, sizeof(lv), mine.stats.level);
  pvpDrawLeft(lv, BATTLE_NAME_X_L, BATTLE_LV_Y, 1, ink);
  pvpFormatLevel(lv, sizeof(lv), m.oppReady.level);
  snprintf(buf, sizeof(buf), "%s%s %s", m.oppReady.shiny ? "*" : "", dexName(m.oppReady.dex), lv);
  pvpDrawRight(buf, BATTLE_NAME_X_R, BATTLE_LV_Y, 1, ink);
}

}  // namespace

uint16_t pvpBattleInk() {
  int h = sceneHour();
  bool night = h < BATTLE_NIGHT_TO_HOUR || h >= BATTLE_NIGHT_FROM_HOUR;
  return night ? COL_INK_NIGHT : COL_INK;
}

void pvpBattleDrawHeader(const PvpApp &app, uint32_t now, uint16_t ink) {
  const PvpUiModel &m = app.model();
  const PvpRuntime &rt = app.match().runtime();
  unsigned shown = rt.round + 1;
  if (shown > battle_core::MAX_TURN_ROUNDS) shown = battle_core::MAX_TURN_ROUNDS;
  char fmt[16];
  char buf[40];
  snprintf(fmt, sizeof(fmt), T(S_ROUND_SHORT_FMT), shown);
  if (m.phase == PVP_UI_CHOOSE) {
    int secs = pvpRoundSecondsLeft(now, m.roundStartMs, PVP_ACTION_TIMEOUT_MS);
    snprintf(buf, sizeof(buf), "%s/%u  %us", fmt, (unsigned)battle_core::MAX_TURN_ROUNDS, (unsigned)secs);
  } else {
    snprintf(buf, sizeof(buf), "%s/%u", fmt, (unsigned)battle_core::MAX_TURN_ROUNDS);
  }
  pvpDrawCentered(buf, BATTLE_ROUND_Y, 2, ink);
  if (m.phase != PVP_UI_CHOOSE) return;
  int left = pvpRoundLeftPermille(now, m.roundStartMs, PVP_ACTION_TIMEOUT_MS);
  pvpDrawBar(BATTLE_TIMEBAR_X0, BATTLE_TIMEBAR_X1, BATTLE_TIMEBAR_Y, BATTLE_TIMEBAR_H, left,
             toneColor(pvpTimeTone(left)));
}

bool pvpBattleShowDamage(const PvpApp &app, uint32_t now) {
  PvpPhase phase = app.model().phase;
  if (app.match().runtime().round == 0) return false;
  if (phase == PVP_UI_WAIT_CONFIRM || phase == PVP_UI_WAIT_FINAL) return true;
  return phase == PVP_UI_CHOOSE && !pvpElapsed(now, pvpUiPhaseSinceMs(), BATTLE_DMG_FLASH_MS);
}

void pvpBattleDrawSides(const PvpApp &app, uint32_t now, uint16_t ink) {
  const PvpMatch &match = app.match();
  const PvpSide &mine = pvpMySide(match.runtime(), match.side());
  const PvpSide &theirs = pvpTheirSide(match.runtime(), match.side());
  drawNames(app, mine, ink);
  drawHpBar(BATTLE_HPBAR_L_X, BATTLE_HPBAR_W, mine.hp, mine.maxHp, COL_OK);
  drawHpBar(BATTLE_HPBAR_R_X, BATTLE_HPBAR_W, theirs.hp, theirs.maxHp, COL_BAD);
  drawHpNumber(mine, BATTLE_HPBAR_L_X, false, ink);
  drawHpNumber(theirs, BATTLE_HPBAR_R_X + BATTLE_HPBAR_W, true, ink);
  if (pvpBattleShowDamage(app, now)) {
    drawDamage(match.lastTurn().theirs, BATTLE_HPBAR_L_X, false);
    drawDamage(match.lastTurn().mine, BATTLE_HPBAR_R_X + BATTLE_HPBAR_W, true);
  }
  drawTypeChips(BATTLE_HPBAR_L_X, BATTLE_CHIPS_Y, DEX_TBL[pet.speciesId], false);
  drawTypeChips(BATTLE_HPBAR_R_X + BATTLE_HPBAR_W, BATTLE_CHIPS_Y, DEX_TBL[app.model().oppReady.dex], true);
}

// No SD (no animation, no thumbnail): a species-colored disc with the Pokedex number keeps both fighters visible.
static void drawSpriteStandIn(int16_t dex, int cx) {
  if (dex < 0 || dex > DEX_COUNT) dex = 0;
  gfx->fillCircle(cx, BATTLE_STANDIN_Y, BATTLE_STANDIN_R, DEX_TBL[dex].accent);
  gfx->drawCircle(cx, BATTLE_STANDIN_Y, BATTLE_STANDIN_R, COL_INK);
  char num[8];
  snprintf(num, sizeof(num), "%d", (int)dex);
  pvpDrawCentered(num, BATTLE_STANDIN_Y - 7, 2, COL_BG);
}

void pvpBattleDrawSprites(const PvpApp &app, uint32_t now) {
  if (pmd.loaded) {
    drawPmdAct(PMD_IDLE, BATTLE_SPRITE_L_X, BATTLE_SPRITE_GROUND_Y, now, true, false, BATTLE_SPRITE_SCALE);
  } else if (const uint8_t *th = thumbs.get(pet.speciesId)) {
    drawThumb(th, BATTLE_THUMB_L_X, BATTLE_THUMB_Y, BATTLE_SPRITE_SCALE, false);
  } else {
    drawSpriteStandIn(pet.speciesId, BATTLE_SPRITE_L_X);
  }
  if (wildPmd.loaded) {
    drawPmdActM(wildPmd, PMD_IDLE, BATTLE_SPRITE_R_X, BATTLE_SPRITE_GROUND_Y, now, true, false, BATTLE_SPRITE_SCALE);
  } else if (const uint8_t *th = thumbs.get(app.model().oppReady.dex)) {
    drawThumb(th, BATTLE_THUMB_R_X, BATTLE_THUMB_Y, BATTLE_SPRITE_SCALE, false);
  } else {
    drawSpriteStandIn(app.model().oppReady.dex, BATTLE_SPRITE_R_X);
  }
}
