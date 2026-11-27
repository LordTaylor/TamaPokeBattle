// Rival list (spec C.2): phase SEARCHING.
#include <stdio.h>

#include "pvp_app.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_screens.h"
#include "pvp_ui_theme.h"
#include "pvp_ui_toast.h"

namespace {

using namespace pvpui;

uint8_t page = 0;

uint8_t pageCount(uint8_t peers) {
  return peers ? (uint8_t)((peers + SEARCH_ROWS_PER_PAGE - 1) / SEARCH_ROWS_PER_PAGE) : 1;
}

uint8_t clampedPage(uint8_t peers) {
  uint8_t pages = pageCount(peers);
  if (page >= pages) page = pages - 1;
  return page;
}

int rowY(uint8_t slot) { return SEARCH_ROW_Y0 + slot * SEARCH_ROW_STEP; }

void drawRow(const PvpPeerInfo &peer, uint8_t slot) {
  int y = rowY(slot);
  gfx->fillRoundRect(SEARCH_ROW_X, y, SEARCH_ROW_W, SEARCH_ROW_H, BTN_R, COL_WHITE);
  gfx->drawRoundRect(SEARCH_ROW_X, y, SEARCH_ROW_W, SEARCH_ROW_H, BTN_R, COL_INK);
  int textY = y + (SEARCH_ROW_H - CHAR_PX_H * 2) / 2;
  pvpDrawLeft(peer.nick, SEARCH_ROW_X + SEARCH_ROW_PAD, textY, 2, COL_INK);
  char lv[16];
  pvpFormatLevel(lv, sizeof(lv), peer.trainerLevel);
  pvpDrawRight(lv, SEARCH_ROW_X + SEARCH_ROW_W - SEARCH_ROW_PAD, textY, 2, COL_INK);
}

void drawEmpty(uint32_t now) {
  int r = PULSE_R_MIN + (int)((now / PULSE_PERIOD_MS) % PULSE_R_RANGE);
  gfx->drawCircle(SCREEN_CX, SEARCH_PULSE_Y, r, COL_MUTED);
  gfx->drawCircle(SCREEN_CX, SEARCH_PULSE_Y, PULSE_R_MIN + PULSE_R_RANGE - (r - PULSE_R_MIN), COL_MUTED);
  pvpDrawCentered(TP(PVP_S_NONE_NEARBY), SEARCH_NONE_Y, 2, COL_INK);
}

void drawPager(uint8_t current, uint8_t pages) {
  char line[16];
  snprintf(line, sizeof(line), "< %u/%u >", (unsigned)(current + 1), (unsigned)pages);
  pvpDrawCentered(line, SEARCH_PAGER_Y, 2, COL_INK);
}

void drawHintLine(uint32_t now) {
  uint16_t color = 0;
  const char *toast = pvpToastText(now, color);
  if (toast) pvpDrawCentered(toast, SEARCH_HINT_Y, 1, color);
  else pvpDrawCentered(TP(PVP_S_HINT), SEARCH_HINT_Y, 1, COL_MUTED);
}

bool tapRow(PvpApp &app, int16_t x, int16_t y, uint8_t first, uint32_t now) {
  for (uint8_t slot = 0; slot < SEARCH_ROWS_PER_PAGE; slot++) {
    uint8_t index = first + slot;
    if (index >= app.session().peers().count()) break;
    if (!pvpHit(x, y, SEARCH_ROW_X, rowY(slot), SEARCH_ROW_X + SEARCH_ROW_W, rowY(slot) + SEARCH_ROW_H)) continue;
    sfxPlay(SFX_TAP);
    app.challengePeer(index, now);
    return true;
  }
  return false;
}

}  // namespace

void pvpUiDrawSearch(const PvpApp &app, uint32_t now) {
  pvpDrawCentered(TP(PVP_S_SEARCHING), SEARCH_TITLE_Y, 2, COL_INK);
  pvpDrawCentered(pvpDots(now), SEARCH_DOTS_Y, 2, COL_INK);
  drawHintLine(now);
  const PvpPeerTable &peers = app.session().peers();
  uint8_t count = peers.count();
  if (count == 0) {
    drawEmpty(now);
  } else {
    uint8_t first = clampedPage(count) * SEARCH_ROWS_PER_PAGE;
    for (uint8_t slot = 0; slot < SEARCH_ROWS_PER_PAGE && first + slot < count; slot++)
      drawRow(peers.at(first + slot), slot);
    if (count > SEARCH_ROWS_PER_PAGE) drawPager(clampedPage(count), pageCount(count));
  }
  pvpDrawButton(CANCEL_BTN_X0, CANCEL_BTN_Y0, CANCEL_BTN_X1, CANCEL_BTN_Y1, COL_MUTED, COL_INK, TP(PVP_S_CANCEL));
}

void pvpUiSwipeSearch(PvpApp &app, int dir, uint32_t now) {
  (void)now;
  uint8_t count = app.session().peers().count();
  uint8_t pages = pageCount(count);
  uint8_t current = clampedPage(count);
  if (dir < 0 && current + 1 < pages) page = current + 1;  // swipe left = next page
  else if (dir > 0 && current > 0) page = current - 1;
  else return;
  sfxPlay(SFX_MENU);
}

void pvpUiTapSearch(PvpApp &app, int16_t x, int16_t y, uint32_t now) {
  if (pvpHit(x, y, CANCEL_BTN_X0, CANCEL_BTN_Y0, CANCEL_BTN_X1, CANCEL_BTN_Y1)) {
    sfxPlay(SFX_TAP);
    app.leave(now);
    return;
  }
  uint8_t count = app.session().peers().count();
  if (count > SEARCH_ROWS_PER_PAGE && y >= SEARCH_PAGER_Y - HIT_SLOP && y <= SEARCH_PAGER_Y + CHAR_PX_H * 2 + HIT_SLOP) {
    pvpUiSwipeSearch(app, x < SCREEN_CX ? 1 : -1, now);  // left half = previous page
    return;
  }
  tapRow(app, x, y, clampedPage(count) * SEARCH_ROWS_PER_PAGE, now);
}
