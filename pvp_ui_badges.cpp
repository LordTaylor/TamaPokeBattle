// Badge list, a sub-view of card page 10 (spec C.9, D.4). One badge per rival, newest first.
#include <stdio.h>

#include "pvp_ui.h"
#include "pvp_ui_battle_logic.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_draw.h"
#include "pvp_ui_screens.h"
#include "pvp_ui_theme.h"

namespace {

using namespace pvpui;

bool viewOpen = false;
uint8_t page = 0;

// Non-empty badges, in ring order (the ring keeps the newest first).
uint8_t collect(const PvpBadge *out[PVP_BADGE_SLOTS]) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++)
    if (pet.pvpBadges[i].seq != 0) out[n++] = &pet.pvpBadges[i];
  return n;
}

// Gold star = two triangles.
void drawStar(int cx, int cy, int r) {
  int dx = r * 87 / 100;
  gfx->fillTriangle(cx, cy - r, cx - dx, cy + r / 2, cx + dx, cy + r / 2, COL_GOLD);
  gfx->fillTriangle(cx, cy + r, cx - dx, cy - r / 2, cx + dx, cy - r / 2, COL_GOLD);
}

void drawRow(const PvpBadge &b, int y) {
  gfx->fillRoundRect(BADGES_ROW_X0, y, BADGES_ROW_X1 - BADGES_ROW_X0, BADGES_ROW_H, BTN_R, COL_WHITE);
  gfx->drawRoundRect(BADGES_ROW_X0, y, BADGES_ROW_X1 - BADGES_ROW_X0, BADGES_ROW_H, BTN_R, COL_MUTED);
  int mid = y + BADGES_ROW_H / 2;
  int starX = BADGES_ROW_X0 + BADGES_ROW_PAD + BADGES_STAR_R;
  drawStar(starX, mid, BADGES_STAR_R);
  int textY = mid - CHAR_PX_H;
  pvpDrawLeft(b.nick, starX + BADGES_STAR_R + BADGES_ROW_PAD, textY, 2, COL_INK);
  char lv[16];
  char line[32];
  pvpFormatLevel(lv, sizeof(lv), b.oppLevel);
  snprintf(line, sizeof(line), "%s x%u", lv, (unsigned)b.wins);
  pvpDrawRight(line, BADGES_ROW_X1 - BADGES_ROW_PAD, textY, 2, COL_INK);
}

void drawNav(uint8_t pages) {
  if (pages < 2) return;
  pvpDrawButton(BADGES_NAV_L_X0, BADGES_NAV_Y0, BADGES_NAV_L_X1, BADGES_NAV_Y1, page > 0 ? COL_WARN : COL_MUTED, COL_BG,
                "<");
  pvpDrawButton(BADGES_NAV_R_X0, BADGES_NAV_Y0, BADGES_NAV_R_X1, BADGES_NAV_Y1,
                page + 1 < pages ? COL_WARN : COL_MUTED, COL_BG, ">");
}

}  // namespace

bool pvpBadgesViewOpen() { return viewOpen; }

void pvpBadgesOpen() {
  viewOpen = true;
  page = 0;
  sfxPlay(SFX_TAP);
  cardDirty = true;
}

void pvpBadgesClose() { viewOpen = false; page = 0; }

void pvpBadgesDraw() {
  const PvpBadge *list[PVP_BADGE_SLOTS];
  uint8_t count = collect(list);
  uint8_t pages = pvpPageCount(count, BADGES_PER_PAGE);
  if (page >= pages) page = pages - 1;
  pvpDrawCentered(TP(PVP_S_BADGES_TITLE), BADGES_TITLE_Y, 3, COL_INK);
  if (count == 0) pvpDrawCentered(TP(PVP_S_BADGES_NONE), BADGES_EMPTY_Y, 2, COL_INK);
  for (uint8_t slot = 0; slot < BADGES_PER_PAGE; slot++) {
    uint8_t index = page * BADGES_PER_PAGE + slot;
    if (index >= count) break;
    drawRow(*list[index], BADGES_ROW_Y0 + slot * BADGES_ROW_STEP);
  }
  drawNav(pages);
}

// Taps in the sub-view. Always handled (true), the bottom strip returns to the main page.
bool pvpBadgesTap(int16_t x, int16_t y) {
  cardDirty = true;
  if (y >= CARD_BACK_Y) {
    sfxPlay(SFX_TAP);
    pvpBadgesClose();
    return true;
  }
  const PvpBadge *list[PVP_BADGE_SLOTS];
  uint8_t pages = pvpPageCount(collect(list), BADGES_PER_PAGE);
  if (pvpHit(x, y, BADGES_NAV_L_X0, BADGES_NAV_Y0, BADGES_NAV_L_X1, BADGES_NAV_Y1) && page > 0) {
    page--;
    sfxPlay(SFX_MENU);
  } else if (pvpHit(x, y, BADGES_NAV_R_X0, BADGES_NAV_Y0, BADGES_NAV_R_X1, BADGES_NAV_Y1) && page + 1 < pages) {
    page++;
    sfxPlay(SFX_MENU);
  }
  return true;
}
