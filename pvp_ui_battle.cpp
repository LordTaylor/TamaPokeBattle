// PvP battle screen (spec C.5): phases CHOOSE / WAIT_OPP / WAIT_CONFIRM / WAIT_FINAL.
// Round log and the five action buttons; the scene itself is pvp_ui_battle_scene.cpp.
#include <stdio.h>

#include "pvp_app.h"
#include "pvp_time.h"
#include "pvp_ui_battle_logic.h"
#include "pvp_ui_battle_parts.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_screens.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

struct ActionButton {
  int x, y, w, h;
  uint8_t action;
  uint16_t color;
  StrId label;
};

// Main row = the PvE geometry (ATTACK / DODGE / REST); QUICK and HEAVY are a fixed second row.
const ActionButton BUTTONS[PVP_ACTION_COUNT] = {
    {BATTLE_MAIN_X[0], BATTLE_MAIN_Y0, BATTLE_MAIN_W, BATTLE_MAIN_H, BATTLE_ATTACK, COL_ACT_ATTACK, S_ATTACK},
    {BATTLE_MAIN_X[1], BATTLE_MAIN_Y0, BATTLE_MAIN_W, BATTLE_MAIN_H, BATTLE_DODGE, COL_ACT_DODGE, S_DODGE},
    {BATTLE_MAIN_X[2], BATTLE_MAIN_Y0, BATTLE_MAIN_W, BATTLE_MAIN_H, BATTLE_REST, COL_ACT_REST, S_REST},
    {BATTLE_ALT_X[0], BATTLE_ALT_Y0, BATTLE_ALT_W, BATTLE_ALT_H, BATTLE_ATTACK_QUICK, COL_ACT_QUICK, S_QUICK_ATTACK},
    {BATTLE_ALT_X[1], BATTLE_ALT_Y0, BATTLE_ALT_W, BATTLE_ALT_H, BATTLE_ATTACK_HEAVY, COL_ACT_HEAVY, S_HEAVY_ATTACK},
};

uint32_t noRestUntil = 0;  // "no rest left" note shown until this time (0 = off)

bool battlePhase(PvpPhase p) {
  return p == PVP_UI_CHOOSE || p == PVP_UI_WAIT_OPP || p == PVP_UI_WAIT_CONFIRM;
}

uint8_t restLeft(const PvpApp &app) {
  const PvpMatch &match = app.match();
  return pvpMySide(match.runtime(), match.side()).restUsesLeft;
}

uint16_t toneColor(PvpTone tone, uint16_t ink) {
  if (tone == PVP_TONE_BAD) return COL_BAD;
  if (tone == PVP_TONE_WARN) return COL_WARN;
  return ink;
}

void drawLine(const PvpLogLine &line, int y, uint16_t ink) {
  if (line.text[0]) pvpDrawCentered(line.text, y, 2, toneColor(line.tone, ink));
}

// Line 1 and 2 of the log: waiting texts while the round is open, the last round afterwards.
void drawLog(const PvpApp &app, uint32_t now, uint16_t ink) {
  const PvpUiModel &m = app.model();
  PvpLogLine mine, foe;
  bool resolved = app.match().runtime().round > 0;
  if (m.phase == PVP_UI_WAIT_FINAL) {
    pvpDrawCentered(TP(PVP_S_WAIT_CONFIRM), BATTLE_LOG1_Y, 2, ink);
    return;
  }
  if (m.phase == PVP_UI_WAIT_OPP) {
    pvpDrawCentered(m.autoAction ? TP(PVP_S_AUTO_ATTACK) : TP(PVP_S_WAIT_OPP), BATTLE_LOG1_Y, 2,
                    m.autoAction ? COL_WARN : ink);
    return;
  }
  if (noRestUntil && !pvpElapsed(now, noRestUntil, 0)) {
    pvpDrawCentered(T(S_NO_REST), BATTLE_LOG1_Y, 2, COL_BAD);
    return;
  }
  if (!resolved) {
    pvpDrawCentered(TP(PVP_S_REST_NOTE), BATTLE_LOG1_Y, 1, ink);
    return;
  }
  pvpMineLine(app.match().lastTurn(), mine);
  pvpFoeLine(app.match().lastTurn(), foe);
  drawLine(mine, BATTLE_LOG1_Y, ink);
  drawLine(foe, BATTLE_LOG2_Y, ink);
}

void drawRestPill(const ActionButton &b, uint8_t left) {
  int cx = b.x + b.w - BATTLE_REST_PILL_R - 2;
  int cy = b.y + BATTLE_REST_PILL_R + 2;
  gfx->fillCircle(cx, cy, BATTLE_REST_PILL_R, COL_INK);
  char n[4];
  snprintf(n, sizeof(n), "%u", (unsigned)left);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_BG);
  gfx->setCursor(cx - CHAR_PX, cy - CHAR_PX_H);  // size 2: 12 x 16 px glyph
  gfx->print(n);
}

void drawOutline(const ActionButton &b) {
  for (int i = 0; i < BATTLE_PICK_OUTLINE; i++)
    gfx->drawRoundRect(b.x - i, b.y - i, b.w + 2 * i, b.h + 2 * i, BTN_R + i, COL_INK);
}

void drawButtons(const PvpApp &app, uint16_t ink) {
  const PvpUiModel &m = app.model();
  bool chosen = m.phase != PVP_UI_CHOOSE;
  uint8_t left = restLeft(app);
  for (const ActionButton &b : BUTTONS) {
    bool picked = chosen && m.myAction == b.action;
    bool spent = b.action == BATTLE_REST && left == 0;
    bool muted = (chosen && !picked) || spent;
    pvpDrawButton(b.x, b.y, b.x + b.w, b.y + b.h, muted ? COL_MUTED : b.color, muted ? ink : COL_BG, T(b.label));
    if (picked) drawOutline(b);
    if (b.action == BATTLE_REST) drawRestPill(b, left);
  }
}

bool tapAction(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  for (const ActionButton &b : BUTTONS) {
    if (!pvpHit(x, y, b.x, b.y, b.x + b.w, b.y + b.h)) continue;
    if (app.model().phase != PVP_UI_CHOOSE) return true;  // chosen already: ignored quietly
    if (b.action == BATTLE_REST && restLeft(app) == 0) {
      noRestUntil = now + BATTLE_NOTE_MS;  // a REST without uses would only turn into a DODGE
      sfxPlay(SFX_DENY);
      return true;
    }
    if (app.submitAction(b.action, now)) sfxPlay(SFX_TAP);  // false while the round lock runs
    return true;
  }
  return false;
}

}  // namespace

void pvpUiDrawBattle(const PvpApp &app, uint32_t now) {
  uint16_t ink = pvpBattleInk();
  drawGameScene();
  pvpBattleDrawHeader(app, now, ink);
  pvpBattleDrawSides(app, now, ink);
  pvpBattleDrawSprites(app, now);
  PvpPhase phase = app.model().phase;
  bool quit = pvpQuitOpen(app);
  if (battlePhase(phase))
    pvpDrawButton(BATTLE_QUIT_X0, BATTLE_QUIT_Y0, BATTLE_QUIT_X1, BATTLE_QUIT_Y1, COL_MUTED, COL_INK, T(S_RUN_BATTLE));
  drawLog(app, now, ink);
  if (phase != PVP_UI_WAIT_FINAL) drawButtons(app, ink);
  if (quit) pvpQuitDraw();
}

void pvpUiTapBattle(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (pvpQuitOpen(app)) {
    pvpQuitTap(app, x, y, now);
    return;
  }
  PvpPhase phase = app.model().phase;
  if (phase == PVP_UI_WAIT_FINAL) return;
  if (battlePhase(phase) && pvpHit(x, y, BATTLE_QUIT_X0, BATTLE_QUIT_Y0, BATTLE_QUIT_X1, BATTLE_QUIT_Y1)) {
    sfxPlay(SFX_TAP);
    pvpQuitShow();
    return;
  }
  tapAction(app, x, y, now);
}
