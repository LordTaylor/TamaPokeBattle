// Dispatch of the PvP screens by phase: rendering, taps and swipes (spec C.10).
#include "pvp_app.h"
#include "pvp_ui.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_screens.h"

namespace {

using namespace pvpui;

void drawPhase(const PvpApp &app, uint32_t now) {
  switch (app.model().phase) {
    case PVP_UI_SEARCHING: pvpUiDrawSearch(app, now); break;
    case PVP_UI_CHALLENGING: pvpUiDrawChallenging(app, now); break;
    case PVP_UI_CHALLENGED: pvpUiDrawChallenged(app, now); break;
    case PVP_UI_PREPARING: pvpUiDrawPreparing(app, now); break;
    case PVP_UI_ENDED: pvpUiDrawEnded(app, now); break;
    case PVP_UI_IDLE:
    case PVP_UI_STOPPING: break;  // blank frame while the last packets leave
    default: pvpUiDrawSoon(app, now); break;  // CHOOSE .. RESULT until the T4.3 screens exist
  }
}

}  // namespace

void renderPvpApp() {
  cardDirty = false;
  gfx->fillScreen(COL_BG);
  drawPhase(pvpAppInstance(), millis());
  flushStampedFrame();
}

void pvpAppTap(int16_t x, int16_t y) {
  PvpApp &app = pvpAppInstance();
  uint32_t now = millis();
  app.touch(now);
  switch (app.model().phase) {
    case PVP_UI_SEARCHING: pvpUiTapSearch(app, x, y, now); break;
    case PVP_UI_CHALLENGING: pvpUiTapChallenging(app, x, y, now); break;
    case PVP_UI_CHALLENGED: pvpUiTapChallenged(app, x, y, now); break;
    case PVP_UI_PREPARING: pvpUiTapPreparing(app, x, y, now); break;
    case PVP_UI_ENDED: pvpUiTapEnded(app, x, y, now); break;
    case PVP_UI_STOPPING: break;
    default: pvpUiTapSoon(app, x, y, now); break;
  }
  cardDirty = true;
}

// Swipes are ignored while a challenge or a battle is on (C.10); the list and the end screens
// treat "up" as back.
void pvpAppSwipeV(int dir) {
  PvpApp &app = pvpAppInstance();
  uint32_t now = millis();
  app.touch(now);
  if (dir >= 0) return;
  PvpPhase phase = app.model().phase;
  if (phase == PVP_UI_SEARCHING) {
    sfxPlay(SFX_TAP);
    app.leave(now);
  } else if ((phase == PVP_UI_ENDED || phase == PVP_UI_RESULT) && app.canLeaveNow(now)) {
    sfxPlay(SFX_TAP);
    app.leave(now);
  }
  cardDirty = true;
}

void pvpAppSwipe(int dir) {
  PvpApp &app = pvpAppInstance();
  uint32_t now = millis();
  app.touch(now);
  if (app.model().phase == PVP_UI_SEARCHING) pvpUiSwipeSearch(app, dir, now);
  cardDirty = true;
}
