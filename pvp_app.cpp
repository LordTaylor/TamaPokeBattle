#include "pvp_app.h"

#include "pvp_time.h"
#include "pvp_versions.h"

void PvpApp::setPhase(PvpPhase phase, uint32_t nowMs) {
  model_.phase = phase;
  if (phase == PVP_UI_RESULT || phase == PVP_UI_ENDED) lastInteract_ = nowMs;
}

void PvpApp::setEnded(PvpEndReason reason, uint32_t nowMs) {
  model_.endReason = reason;
  setPhase(PVP_UI_ENDED, nowMs);
}

bool PvpApp::open(uint32_t nowMs) {
  if (model_.phase != PVP_UI_IDLE || link_.running()) return false;
  model_ = PvpUiModel();
  PvpDenyReason deny = PVP_DENY_NONE;
  if (!profile_.canFight(deny)) {
    model_.deny = deny;
    return false;
  }
  uint8_t mac[6] = {0};
  if (!link_.start(mac)) {
    model_.endReason = PVP_END_RADIO_FAIL;
    return false;
  }
  session_.init(pvpDeviceIdFromMac(mac), profile_.nick(), profile_.trainerLevel());
  localQuit_ = false;
  rejected_ = false;
  battleOpen_ = false;
  roundLocked_ = false;
  match_ = PvpMatch();
  if (!session_.startDiscovery(nowMs)) {
    link_.stop();
    model_.endReason = PVP_END_RADIO_FAIL;
    return false;
  }
  lastInteract_ = nowMs;
  setPhase(PVP_UI_SEARCHING, nowMs);
  return true;
}

void PvpApp::update(uint32_t nowMs) {
  if (model_.phase == PVP_UI_IDLE) return;
  if (link_.running()) pump_.update(nowMs, *this);
  tickTimers(nowMs);
}

void PvpApp::tickTimers(uint32_t nowMs) {
  switch (model_.phase) {
    case PVP_UI_SEARCHING:
      model_.peerCount = session_.peers().count();
      if (pvpElapsed(nowMs, lastInteract_, PVP_SEARCH_IDLE_MS)) {
        model_.endReason = PVP_END_IDLE;
        session_.stopDiscovery();
        beginStop(nowMs);
      }
      break;
    case PVP_UI_RESULT:
    case PVP_UI_ENDED:
      if (session_.canLeave(nowMs) && pvpElapsed(nowMs, lastInteract_, PVP_RESULT_IDLE_MS)) beginStop(nowMs);
      break;
    case PVP_UI_STOPPING:
      if (link_.inFlight() == 0 || pvpElapsed(nowMs, stopSinceMs_, PVP_LINK_FLUSH_MS)) finishStop();
      break;
    default:
      break;
  }
  if (roundLocked_ && pvpElapsed(nowMs, roundConfirmedMs_, PVP_ROUND_LOCK_MS)) roundLocked_ = false;
}

void PvpApp::beginStop(uint32_t nowMs) {
  stopSinceMs_ = nowMs;
  model_.phase = PVP_UI_STOPPING;  // keeps pumping so the last packets leave
}

void PvpApp::finishStop() {
  session_.stopDiscovery();
  link_.stop();
  battleOpen_ = false;
  model_.phase = PVP_UI_IDLE;
}

bool PvpApp::challengePeer(uint8_t index, uint32_t nowMs) {
  if (model_.phase != PVP_UI_SEARCHING || index >= session_.peers().count()) return false;
  touch(nowMs);
  const PvpPeerInfo peer = session_.peers().at(index);
  uint32_t sessionId = profile_.random32();
  if (sessionId == 0) sessionId = 1;
  if (!session_.challenge(peer.deviceId, sessionId, profile_.random32(), nowMs)) return false;
  model_.opponent = peer;
  model_.notice = PVP_NOTICE_NONE;
  setPhase(PVP_UI_CHALLENGING, nowMs);
  pump_.drain(nowMs, *this);
  return true;
}

bool PvpApp::respondChallenge(bool accept, uint32_t nowMs) {
  if (model_.phase != PVP_UI_CHALLENGED) return false;
  touch(nowMs);
  bool ok;
  if (accept) {
    ok = session_.accept(profile_.random32(), nowMs);
  } else {
    ok = session_.decline(nowMs);
    if (ok) setPhase(PVP_UI_SEARCHING, nowMs);
  }
  pump_.drain(nowMs, *this);
  return ok;
}

bool PvpApp::submitAction(uint8_t action, uint32_t nowMs) {
  if (model_.phase != PVP_UI_CHOOSE || roundLocked_) return false;
  if (!session_.submitAction(action, nowMs)) return false;
  model_.myAction = action;
  model_.autoAction = false;
  setPhase(PVP_UI_WAIT_OPP, nowMs);
  pump_.drain(nowMs, *this);
  return true;
}

bool PvpApp::quit(uint32_t nowMs) {
  bool inBattle = model_.phase == PVP_UI_CHOOSE || model_.phase == PVP_UI_WAIT_OPP ||
                  model_.phase == PVP_UI_WAIT_CONFIRM;
  if (!inBattle || !battleOpen_) return false;
  localQuit_ = true;  // BEFORE abort(): ABORTED(USER) is the same event for both sides
  session_.abort(nowMs);
  pump_.drain(nowMs, *this);  // ABORT copies leave first, then the handler runs recordQuit
  if (battleOpen_) finishQuit(nowMs);
  return true;
}

bool PvpApp::leave(uint32_t nowMs) {
  switch (model_.phase) {
    case PVP_UI_SEARCHING:
      session_.stopDiscovery();
      beginStop(nowMs);
      return true;
    case PVP_UI_CHALLENGING:
    case PVP_UI_CHALLENGED:
    case PVP_UI_PREPARING:  // no battle yet: no flag, no penalty
      beginStop(nowMs);
      session_.abort(nowMs);
      pump_.drain(nowMs, *this);  // events are ignored in STOPPING
      return true;
    case PVP_UI_RESULT:
    case PVP_UI_ENDED:
      if (!session_.canLeave(nowMs)) return false;
      beginStop(nowMs);
      return true;
    default:
      return false;
  }
}

void PvpApp::forceStop(uint32_t nowMs) {
  if (model_.phase == PVP_UI_STOPPING) return;  // already flushing; update() finishes it
  if (model_.phase == PVP_UI_IDLE) {
    if (link_.running()) link_.stop();
    return;
  }
  if (battleOpen_) {
    localQuit_ = true;
    session_.abort(nowMs);
    pump_.drain(nowMs, *this);
    if (battleOpen_) finishQuit(nowMs);
  } else {
    beginStop(nowMs);  // events are ignored in STOPPING
    session_.abort(nowMs);
    pump_.drain(nowMs, *this);
  }
  beginStop(nowMs);  // the ABORT copies get PVP_LINK_FLUSH_MS to leave the radio before stop()
}
