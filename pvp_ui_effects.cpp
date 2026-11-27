// Phase-change effects of the PvP screens (spec C.2, C.3, C.8, C.12): sounds, screen wake,
// toasts, redraw requests, battle sounds and the rival sprite. Reads PvpApp state only; it never
// acts on the app.
#include "pvp_app.h"
#include "pvp_ui.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_fx.h"
#include "pvp_ui_phase.h"
#include "pvp_ui_toast.h"

namespace {

PvpPhase prevPhase = PVP_UI_IDLE;
PvpNotice prevNotice = PVP_NOTICE_NONE;
uint8_t prevPeers = 0;
bool peerSoundDone = false;
uint32_t phaseSince = 0;

void onPhaseEntered(const PvpUiModel &m, uint32_t now) {
  switch (m.phase) {
    case PVP_UI_SEARCHING:
      if (prevPhase == PVP_UI_IDLE) {
        peerSoundDone = false;
        prevPeers = 0;
        sfxPlay(SFX_MENU);
      }
      break;
    case PVP_UI_CHALLENGED:
      screenOff = false;  // wake: the dim stage counts from lastInteract
      lastInteract = now;
      lockTouchBrief(pvpui::WAKE_TOUCH_LOCK_MS);  // the touch that woke the user must not accept
      sfxPlay(SFX_MENU);
      break;
    case PVP_UI_ENDED:
      sfxPlay(SFX_DENY);
      break;
    case PVP_UI_STOPPING:
      if (m.endReason == PVP_END_IDLE) {
        pvpToastShow(PVP_S_IDLE_EXIT, pvpui::COL_WARN, now, pvpui::NOTICE_MS);
        sfxPlay(SFX_DENY);
      }
      break;
    default:
      break;
  }
}

void onNoticeChanged(const PvpUiModel &m, uint32_t now) {
  if (m.notice == PVP_NOTICE_NONE) return;
  if (m.notice == PVP_NOTICE_DECLINED) pvpToastShow(PVP_S_DECLINED_MSG, pvpui::COL_WARN, now, pvpui::NOTICE_MS);
  else if (m.notice == PVP_NOTICE_EXPIRED) pvpToastShow(PVP_S_EXPIRED, pvpui::COL_WARN, now, pvpui::NOTICE_MS);
  else pvpToastShow(PVP_S_ABORT_UPDATE, pvpui::COL_BAD, now, pvpui::NOTICE_VERSION_MS);
  sfxPlay(SFX_DENY);
}

void trackPeers(const PvpUiModel &m) {
  if (m.phase != PVP_UI_SEARCHING) return;
  if (!peerSoundDone && prevPeers == 0 && m.peerCount > 0) {
    sfxPlay(SFX_EVENT_SPARKLE);  // only the first rival of a search session
    peerSoundDone = true;
  }
  prevPeers = m.peerCount;
}

bool battlePhase(PvpPhase p) { return p >= PVP_UI_CHOOSE && p <= PVP_UI_WAIT_FINAL; }

// Polled once per loop, so it keys on state (battle seen, round counter) and not on single phase
// changes: a round resolved and confirmed within one update still gets its sound.
void trackBattle(const PvpApp &app, uint32_t now) {
  static bool inBattle = false;
  static uint8_t lastRound = 0;
  const PvpUiModel &m = app.model();
  uint8_t round = app.match().runtime().round;
  if (battlePhase(m.phase) && !inBattle) {
    inBattle = true;
    lastRound = 0;
    pvpFxBattleStart(m, now);
  }
  if (inBattle && round != lastRound) {
    lastRound = round;
    pvpFxRoundResolved(app);
  }
  if (inBattle && !battlePhase(m.phase)) {
    inBattle = false;
    pvpFxBattleEnd();
    if (m.phase == PVP_UI_RESULT) pvpFxResult(m, now);
  }
  pvpFxTick(app, now);
}

}  // namespace

uint32_t pvpUiPhaseSinceMs() { return phaseSince; }

void pvpUiTrack(uint32_t now) {
  PvpApp &app = pvpAppInstance();
  const PvpUiModel &m = app.model();
  if (m.phase != prevPhase) {
    phaseSince = now;
    onPhaseEntered(m, now);
    if (m.phase == PVP_UI_IDLE) cardDirty = true;  // back on card page 10: it must be redrawn once
    prevPhase = m.phase;
  }
  if (m.notice != prevNotice) {
    if (m.phase == PVP_UI_SEARCHING) onNoticeChanged(m, now);
    prevNotice = m.notice;
  }
  trackPeers(m);
  trackBattle(app, now);
  if (pvpToastJustExpired(now)) cardDirty = true;
}
