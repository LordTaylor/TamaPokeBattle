// Card page 10 (spec C.1): trainer profile and the "TRAINER BATTLE" entry button.
#include <stdio.h>
#include <string.h>

#include "pvp_app.h"
#include "pvp_ui.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_text.h"
#include "pvp_ui_theme.h"
#include "pvp_ui_toast.h"

namespace {

using namespace pvpui;

constexpr size_t LINE = 40;

PvpStrId denyText(PvpDenyReason reason) {
  switch (reason) {
    case PVP_DENY_SLEEPING: return PVP_S_ASLEEP;
    case PVP_DENY_LOW_BATTERY: return PVP_S_LOW_BATTERY;
    default: return PVP_S_UNAVAILABLE;  // egg, ceremony, no nick: the general fallback
  }
}

uint8_t badgeCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++)
    if (pet.pvpBadges[i].seq != 0) n++;
  return n;
}

void drawIdentity() {
  const char *nick = pet.trainer[0] ? pet.trainer : PVP_DEFAULT_NICK;
  pvpDrawCentered(nick, CARD_NICK_Y, 3, COL_INK);
  char line[LINE];
  snprintf(line, sizeof(line), TP(PVP_S_BADGES_COUNT_FMT), (unsigned)badgeCount());
  pvpDrawCentered(line, CARD_BADGES_Y, 1, COL_MUTED);
  char lv[16];
  pvpFormatLevel(lv, sizeof(lv), pet.trainerStats.level);
  snprintf(line, sizeof(line), "%s %s", TP(PVP_S_TRAINER_LABEL), lv);
  pvpDrawCentered(line, CARD_TRAINER_Y, 2, COL_INK);
}

void drawProgress() {
  const TrainerStats &st = pet.trainerStats;
  uint16_t cost = trainerLevelCost(st.level);
  int permille = cost ? (int)((uint32_t)st.progress * 1000 / cost) : 1000;
  pvpDrawBar(CARD_BAR_X0, CARD_BAR_X1, CARD_BAR_Y, CARD_BAR_H, permille, COL_OK);
  char line[LINE];
  if (cost) snprintf(line, sizeof(line), TP(PVP_S_POINTS_FMT), (unsigned)st.progress, (unsigned)cost);
  else snprintf(line, sizeof(line), "%s", TP(PVP_S_LEVEL_MAX));
  pvpDrawCentered(line, CARD_POINTS_Y, 2, COL_INK);
}

void drawTile(int index, uint16_t frame, uint16_t value, PvpStrId label) {
  int x = CARD_TILE_X[index];
  gfx->fillRoundRect(x, CARD_TILE_Y, CARD_TILE_W, CARD_TILE_H, BTN_R, COL_WHITE);
  gfx->drawRoundRect(x, CARD_TILE_Y, CARD_TILE_W, CARD_TILE_H, BTN_R, frame);
  char num[8];
  snprintf(num, sizeof(num), "%u", (unsigned)value);
  int mid = x + CARD_TILE_W / 2;
  gfx->setTextSize(3);
  gfx->setTextColor(COL_INK);
  gfx->setCursor(mid - pvpTextWidth((int)strlen(num), 3) / 2, CARD_TILE_Y + CARD_TILE_NUM_DY);
  gfx->print(num);
  const char *name = TP(label);
  gfx->setTextSize(1);
  gfx->setCursor(mid - pvpTextWidth((int)strlen(name), 1) / 2, CARD_TILE_Y + CARD_TILE_LABEL_DY);
  gfx->print(name);
}

uint16_t resultColor(uint8_t result) {
  if (result == PVP_WIN) return COL_OK;
  if (result == PVP_LOSS) return COL_BAD;
  return COL_WARN;
}

void drawHistory() {
  int x0 = SCREEN_CX - ((PVP_HISTORY_SIZE - 1) * CARD_DOT_STEP) / 2;
  for (uint8_t i = 0; i < PVP_HISTORY_SIZE; i++) {
    const PvpHistoryEntry &h = pet.pvpHistory[i];
    int x = x0 + i * CARD_DOT_STEP;
    if (h.seq == 0) gfx->drawCircle(x, CARD_DOTS_Y, CARD_DOT_R, COL_MUTED);
    else gfx->fillCircle(x, CARD_DOTS_Y, CARD_DOT_R, resultColor(h.result));
  }
}

void drawStepsLine() {
  char line[LINE];
  if (stepEncounterPending) {
    snprintf(line, sizeof(line), "%s", TP(PVP_S_WILD_WAITING));
  } else {
    char value[8] = "?";
    if (stepsToEncounter) snprintf(value, sizeof(value), "%u", (unsigned)stepsToEncounter);
    snprintf(line, sizeof(line), TP(PVP_S_NEXT_WILD_FMT), value);
  }
  pvpDrawCentered(line, CARD_STEPS_Y, 2, COL_INK);
}

void drawEntry(bool allowed, PvpDenyReason reason, uint32_t now) {
  pvpDrawButton(CARD_BTN_X0, CARD_BTN_Y0, CARD_BTN_X1, CARD_BTN_Y1, allowed ? COL_WARN : COL_MUTED,
                allowed ? COL_BG : COL_INK, TP(PVP_S_BTN_FIGHT));
  uint16_t toastColor = 0;
  const char *toast = pvpToastText(now, toastColor);
  if (toast) pvpDrawCentered(toast, CARD_HINT_Y, 1, toastColor);
  else if (allowed) pvpDrawCentered(TP(PVP_S_HINT), CARD_HINT_Y, 1, COL_MUTED);
  else pvpDrawCentered(TP(denyText(reason)), CARD_HINT_Y, 1, COL_BAD);
}

}  // namespace

void renderCardPvp() {
  PvpDenyReason reason = PVP_DENY_NONE;
  bool allowed = pvpProfileInstance().canFight(reason);
  drawIdentity();
  drawProgress();
  drawTile(0, COL_OK, pet.trainerStats.wins, PVP_S_STAT_WINS);
  drawTile(1, COL_BAD, pet.trainerStats.losses, PVP_S_STAT_LOSSES);
  drawTile(2, COL_WARN, pet.trainerStats.draws, PVP_S_STAT_DRAWS);
  drawHistory();
  drawStepsLine();
  drawEntry(allowed, reason, millis());
}

void pvpCardResetSub() {
  // The badge sub-view arrives with T4.3; nothing to reset yet.
}

bool pvpCardTap(int16_t x, int16_t y) {
  if (y >= CARD_BACK_Y || !pvpHit(x, y, CARD_BTN_X0, CARD_BTN_Y0, CARD_BTN_X1, CARD_BTN_Y1)) return false;
  uint32_t now = millis();
  PvpApp &app = pvpAppInstance();
  PvpDenyReason reason = PVP_DENY_NONE;
  if (!pvpProfileInstance().canFight(reason)) {
    sfxPlay(SFX_DENY);  // the reason is already on screen
    return true;
  }
  if (app.open(now)) {
    cardDirty = true;
  } else {
    if (app.model().endReason == PVP_END_RADIO_FAIL)
      pvpToastShow(PVP_S_RADIO_FAIL, COL_BAD, now, NOTICE_MS);
    sfxPlay(SFX_DENY);
    cardDirty = true;
  }
  return true;
}
