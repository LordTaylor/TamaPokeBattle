// Phase-change effects of the PvP screens (spec C.2, C.3, C.8, C.12): sounds, screen wake,
// toasts, redraw requests. Reads PvpApp::model() only; the sole action it takes on the app is the
// T4.2 stage guard (leave before the battle starts, see pvpui::BATTLE_UI_READY).
#include "pvp_app.h"
#include "pvp_ui.h"
#include "pvp_ui_bridge.h"
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

// T4.2 stage guard: no battle UI yet, so the match ends before BATTLE_START with a "coming soon" note.
void guardMissingBattleUi(PvpApp &app, uint32_t now) {
  static bool shown = false;
  const PvpUiModel &m = app.model();
  if (m.phase == PVP_UI_IDLE) shown = false;
  if (pvpui::BATTLE_UI_READY || shown || m.phase != PVP_UI_STOPPING || m.oppReady.dex == 0) return;
  shown = true;  // the core gate (PVP_BATTLE_ENABLED) ended the match; tell the player why
  pvpToastShow(PVP_S_SOON, pvpui::COL_WARN, now, pvpui::NOTICE_MS);
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
  guardMissingBattleUi(app, now);
  if (pvpToastJustExpired(now)) cardDirty = true;
}
