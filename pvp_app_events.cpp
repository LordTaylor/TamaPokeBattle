// Session events -> engine / ledger / phase (PvpApp). Spec: docs/pvp-integration-spec.md E.2, E.3.
#include <string.h>

#include "pvp_app.h"
#include "pvp_badges.h"
#include "pvp_validator.h"

namespace {

PvpEndReason endReasonFor(uint8_t reason) {
  switch (reason) {
    case PVP_REASON_VERSION: return PVP_END_UPDATE;
    case PVP_REASON_TIMEOUT: return PVP_END_LINK;
    case PVP_REASON_DESYNC: return PVP_END_SYNC;
    case PVP_REASON_BAD_STATS: return PVP_END_STATS;
    case PVP_REASON_USER: return PVP_END_PEER_QUIT;
    default: return PVP_END_PROTO;
  }
}

}  // namespace

void PvpApp::onEvent(const PvpEvent &e, uint32_t nowMs) {
  switch (e.type) {
    case PVP_EVT_PEER_FOUND:
    case PVP_EVT_PEER_LOST:
      if (model_.phase == PVP_UI_SEARCHING) model_.peerCount = session_.peers().count();
      break;
    case PVP_EVT_CHALLENGE_RECEIVED:
      if (model_.phase != PVP_UI_SEARCHING) break;
      model_.opponent = e.peer;
      model_.notice = PVP_NOTICE_NONE;
      touch(nowMs);
      setPhase(PVP_UI_CHALLENGED, nowMs);
      break;
    case PVP_EVT_ACCEPTED: onAccepted(e, nowMs); break;
    case PVP_EVT_DECLINED:
      if (model_.phase == PVP_UI_CHALLENGING) backToSearch(PVP_NOTICE_DECLINED, nowMs);
      break;
    case PVP_EVT_OPPONENT_READY: onOpponentReady(e, nowMs); break;
    case PVP_EVT_BATTLE_START: onBattleStart(nowMs); break;
    case PVP_EVT_ROUND_READY: onRoundReady(e, nowMs); break;
    case PVP_EVT_ROUND_CONFIRMED:
      if (model_.phase != PVP_UI_WAIT_CONFIRM) break;
      model_.roundStartMs = nowMs;
      model_.autoAction = false;
      roundConfirmedMs_ = nowMs;
      roundLocked_ = true;
      setPhase(PVP_UI_CHOOSE, nowMs);
      break;
    case PVP_EVT_ACTION_TIMEOUT:
      if (model_.phase == PVP_UI_CHOOSE && session_.submitDefaultAction(nowMs)) {
        model_.myAction = PVP_DEFAULT_ACTION;
        model_.autoAction = true;
        setPhase(PVP_UI_WAIT_OPP, nowMs);
      }
      break;
    case PVP_EVT_DESYNC:
      model_.desyncs++;  // the ABORTED(DESYNC) that follows ends the match
      break;
    case PVP_EVT_ABORTED: onAborted(e.reason, nowMs); break;
    case PVP_EVT_FINISHED: onFinished(nowMs); break;
    case PVP_EVT_VERSION_MISMATCH:
      if (model_.phase == PVP_UI_SEARCHING || model_.phase == PVP_UI_CHALLENGING) {
        model_.notice = PVP_NOTICE_VERSION;
      }
      break;
    default:
      break;
  }
}

void PvpApp::onAccepted(const PvpEvent &e, uint32_t nowMs) {
  if (model_.phase != PVP_UI_CHALLENGING && model_.phase != PVP_UI_CHALLENGED) return;
  model_.opponent = e.peer;
  rejected_ = false;
  localQuit_ = false;
  match_ = PvpMatch();  // a new session never inherits the previous engine
  setPhase(PVP_UI_PREPARING, nowMs);
  PvpReadyPayload ready;
  if (!profile_.localReady(ready)) ready = PvpReadyPayload();
  ready.hp = 0;  // max HP is derived by the engine on both sides
  localReady_ = ready;
  if (!session_.setLocalReady(ready, nowMs)) {  // our own data is not sendable: give up cleanly
    setEnded(PVP_END_STATS, nowMs);
    session_.abort(nowMs);
  }
}

void PvpApp::onOpponentReady(const PvpEvent &e, uint32_t nowMs) {
  if (model_.phase != PVP_UI_PREPARING) return;
  model_.oppReady = e.ready;
  bool ok = pvpCheckOpponentReady(e.ready) == PVP_STATS_OK &&
            match_.begin(localReady_, e.ready, session_.isSideA());
  if (ok) return;
  rejected_ = true;  // a BATTLE_START that is already on its way must be ignored
  if (!session_.rejectOpponent(nowMs)) setEnded(PVP_END_STATS, nowMs);
}

void PvpApp::onBattleStart(uint32_t nowMs) {
  if (rejected_ || !match_.ready() || model_.phase != PVP_UI_PREPARING) return;
  if (!battleEnabled_) {  // staging gate: no flag, no penalty, no result
    beginStop(nowMs);
    session_.abort(nowMs);
    return;
  }
  battleOpen_ = true;
  ledger_.battleBegin();  // from now on an escape costs a point
  model_.round = 0;
  model_.roundStartMs = nowMs;
  roundLocked_ = false;
  setPhase(PVP_UI_CHOOSE, nowMs);
}

void PvpApp::onRoundReady(const PvpEvent &e, uint32_t nowMs) {
  bool inRound = model_.phase == PVP_UI_CHOOSE || model_.phase == PVP_UI_WAIT_OPP;
  if (!battleOpen_ || !inRound) return;
  uint32_t hash = 0;
  if (!match_.applyRound(e.round, e.actionA, e.actionB, e.luck, hash)) {
    session_.rejectOpponent(nowMs);  // ABORTED(BAD_STATS) follows and closes the battle without a result
    return;
  }
  model_.round = e.round;
  bool over = match_.lastTurn().battleEnded;
  setPhase(over ? PVP_UI_WAIT_FINAL : PVP_UI_WAIT_CONFIRM, nowMs);
  session_.submitRoundHash(hash, over, nowMs);
}

void PvpApp::onFinished(uint32_t nowMs) {
  if (!battleOpen_) return;
  PvpResult result = pvpResultFromView(match_.outcome());
  if (result == PVP_ABORTED) {  // FINISHED while our engine says "ongoing": inconsistent
    endBattleWithoutResult(PVP_END_PROTO, nowMs);
    return;
  }
  // The pump has already sent everything queued up to this event: the (slow, flash
  // freezing) save cannot delay the last hash / BYE.
  model_.result = result;
  model_.award = ledger_.recordMatch(result, model_.opponent.nick, strlen(model_.opponent.nick),
                                     model_.opponent.trainerLevel, pvpDeviceIdFromMac(model_.opponent.mac),
                                     profile_.nowEpoch(), true);
  model_.badgeEarned = pvpBadgeEarned(result, model_.award, true);
  model_.resultRecorded = true;
  battleOpen_ = false;
  setPhase(PVP_UI_RESULT, nowMs);
}

void PvpApp::onAborted(uint8_t reason, uint32_t nowMs) {
  PvpPhase p = model_.phase;
  if (p == PVP_UI_IDLE || p == PVP_UI_RESULT || p == PVP_UI_ENDED || p == PVP_UI_STOPPING) return;
  if (battleOpen_) {
    if (reason == PVP_REASON_USER) {
      if (localQuit_) finishQuit(nowMs);
      else finishPeerQuit(nowMs);
    } else {
      endBattleWithoutResult(endReasonFor(reason), nowMs);
    }
    return;
  }
  if (p == PVP_UI_CHALLENGED && (reason == PVP_REASON_TIMEOUT || reason == PVP_REASON_USER)) {
    backToSearch(PVP_NOTICE_EXPIRED, nowMs);
  } else if (p == PVP_UI_CHALLENGING && reason == PVP_REASON_USER) {
    backToSearch(PVP_NOTICE_DECLINED, nowMs);
  } else if (p == PVP_UI_CHALLENGING && reason == PVP_REASON_TIMEOUT) {
    setEnded(PVP_END_NO_REPLY, nowMs);
  } else {
    setEnded(endReasonFor(reason), nowMs);
  }
}

void PvpApp::finishQuit(uint32_t nowMs) {
  ledger_.recordQuit();  // never battleEnd() here: it would clear the flag first
  battleOpen_ = false;
  model_.quitPenalty = true;
  setEnded(PVP_END_YOU_QUIT, nowMs);
}

// The rival escaped explicitly: walkover win for the one who stayed, no badge.
void PvpApp::finishPeerQuit(uint32_t nowMs) {
  model_.result = PVP_WIN;
  model_.award = ledger_.recordMatch(PVP_WIN, model_.opponent.nick, strlen(model_.opponent.nick),
                                     model_.opponent.trainerLevel, pvpDeviceIdFromMac(model_.opponent.mac),
                                     profile_.nowEpoch(), false);
  model_.badgeEarned = false;
  model_.resultRecorded = true;
  battleOpen_ = false;
  setEnded(PVP_END_PEER_QUIT, nowMs);
}

void PvpApp::endBattleWithoutResult(PvpEndReason reason, uint32_t nowMs) {
  ledger_.battleEnd();
  battleOpen_ = false;
  setEnded(reason, nowMs);
}

void PvpApp::backToSearch(PvpNotice notice, uint32_t nowMs) {
  model_.notice = notice;
  if (session_.startDiscovery(nowMs)) {
    model_.peerCount = session_.peers().count();
    setPhase(PVP_UI_SEARCHING, nowMs);
  } else {
    setEnded(PVP_END_PROTO, nowMs);
  }
}
