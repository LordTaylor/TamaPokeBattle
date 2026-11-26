// Ready exchange and lockstep round handling of PvpSession.
#include "pvp_session.h"

#include <string.h>

void PvpSession::resetRoundData() {
  phase_ = PVP_PHASE_WAIT_ACTIONS;
  actionTimeoutSent_ = false;
  localActionSet_ = false;
  localAction_ = 0;
  localHashSet_ = false;
  localHash_ = 0;
  localFinal_ = false;
  peerHashGot_ = false;
  peerHash_ = 0;
  peerFinal_ = false;
}

bool PvpSession::setLocalReady(const PvpReadyPayload &ready, uint32_t nowMs) {
  if (state_ != PVP_STATE_HANDSHAKE || localReadySet_ || !pvpValidReady(ready)) return false;
  PvpMessage msg;
  msg.type = PVP_MSG_READY;
  msg.ready = ready;
  if (!sendReliable(msg, 0, nowMs)) return false;
  localReadySet_ = true;
  enterState(PVP_STATE_READY_EXCHANGE, nowMs);
  tryEnterBattle(nowMs);
  return true;
}

bool PvpSession::onReady(const PvpMessage &m, uint32_t nowMs) {
  bool pre = state_ == PVP_STATE_HANDSHAKE || state_ == PVP_STATE_READY_EXCHANGE;
  if (!pre || peerReadyGot_) return false;
  peerReady_ = m.ready;
  peerReadyGot_ = true;
  tx_.drop(PVP_MSG_ACCEPT, false, 0);  // the peer clearly got our ACCEPT
  PvpEvent e;
  e.type = PVP_EVT_OPPONENT_READY;
  e.peer = peerInfo();
  e.ready = m.ready;
  events_.push(e);
  tryEnterBattle(nowMs);
  return true;
}

void PvpSession::tryEnterBattle(uint32_t nowMs) {
  if (state_ != PVP_STATE_READY_EXCHANGE || !peerReadyGot_) return;
  enterState(PVP_STATE_BATTLE, nowMs);
  round_ = 0;
  beginRound(nowMs);
  pushEvent(PVP_EVT_BATTLE_START);
}

void PvpSession::beginRound(uint32_t nowMs) {
  uint8_t keepAction = peerAction_;
  bool keepGot = peerActionGot_;
  resetRoundData();
  peerAction_ = keepAction;
  peerActionGot_ = keepGot;
  roundStartMs_ = nowMs;
  progressMs_ = nowMs;
}

void PvpSession::advanceRound(uint32_t nowMs) {
  round_++;
  peerActionGot_ = nextActionGot_;
  peerAction_ = nextAction_;
  nextActionGot_ = false;
  nextAction_ = 0;
  beginRound(nowMs);
}

bool PvpSession::submitAction(uint8_t action, uint32_t nowMs) {
  bool ok = state_ == PVP_STATE_BATTLE && phase_ == PVP_PHASE_WAIT_ACTIONS && !localActionSet_;
  if (!ok || action >= PVP_ACTION_COUNT) return false;
  PvpMessage msg;
  msg.type = PVP_MSG_ACTION;
  msg.round = round_;
  msg.action = action;
  if (!sendReliable(msg, round_, nowMs)) return false;
  localActionSet_ = true;
  localAction_ = action;
  tryRoundReady(nowMs);
  return true;
}

bool PvpSession::submitDefaultAction(uint32_t nowMs) {
  return submitAction(PVP_DEFAULT_ACTION, nowMs);
}

bool PvpSession::onPeerAction(const PvpMessage &m, uint32_t nowMs) {
  bool live = state_ == PVP_STATE_HANDSHAKE || state_ == PVP_STATE_READY_EXCHANGE ||
              state_ == PVP_STATE_BATTLE;
  if (!live) return false;
  if (m.round == round_ && !peerActionGot_) {
    peerActionGot_ = true;
    peerAction_ = m.action;
  } else if (m.round == (uint16_t)(round_ + 1) && !nextActionGot_) {
    nextActionGot_ = true;
    nextAction_ = m.action;
    tx_.drop(PVP_MSG_ROUND_HASH, true, round_);  // implied: peer finished round_
  } else {
    return false;
  }
  tx_.drop(PVP_MSG_READY, false, 0);  // implied: peer has our READY
  tryRoundReady(nowMs);
  return true;
}

void PvpSession::tryRoundReady(uint32_t nowMs) {
  bool ready = state_ == PVP_STATE_BATTLE && phase_ == PVP_PHASE_WAIT_ACTIONS &&
               localActionSet_ && peerActionGot_;
  if (!ready) return;
  phase_ = PVP_PHASE_WAIT_HASH;
  progressMs_ = nowMs;
  PvpEvent e;
  e.type = PVP_EVT_ROUND_READY;
  e.round = round_;
  e.actionA = sideA_ ? localAction_ : peerAction_;
  e.actionB = sideA_ ? peerAction_ : localAction_;
  uint32_t nonceA = sideA_ ? localNonce_ : peerNonce_;
  uint32_t nonceB = sideA_ ? peerNonce_ : localNonce_;
  e.luck = pvpLuck(nonceA, nonceB, round_);
  e.peer = peerInfo();
  events_.push(e);
}

bool PvpSession::submitRoundHash(uint32_t hash, bool battleOver, uint32_t nowMs) {
  bool ok = state_ == PVP_STATE_BATTLE && phase_ == PVP_PHASE_WAIT_HASH && !localHashSet_;
  if (!ok) return false;
  PvpMessage msg;
  msg.type = PVP_MSG_ROUND_HASH;
  msg.round = round_;
  msg.hash = hash;
  msg.finalRound = battleOver ? 1 : 0;
  if (!sendReliable(msg, round_, nowMs)) return false;
  localHashSet_ = true;
  localHash_ = hash;
  localFinal_ = battleOver;
  tryConfirm(nowMs);
  return true;
}

bool PvpSession::onPeerHash(const PvpMessage &m, uint32_t nowMs) {
  // Already finished: acknowledge the peer's (re)sent final hash so it stops retransmitting.
  if (state_ == PVP_STATE_FINISHED) return m.round == round_ && m.finalRound != 0;
  if (state_ != PVP_STATE_BATTLE || m.round != round_ || peerHashGot_) return false;
  peerHashGot_ = true;
  peerHash_ = m.hash;
  peerFinal_ = m.finalRound != 0;
  tx_.drop(PVP_MSG_ACTION, true, round_);  // implied: peer processed our action
  tryConfirm(nowMs);
  return true;
}

void PvpSession::tryConfirm(uint32_t nowMs) {
  if (!localHashSet_ || !peerHashGot_) return;
  if (localHash_ != peerHash_ || localFinal_ != peerFinal_) {
    pushEvent(PVP_EVT_DESYNC);
    abortLocal(PVP_REASON_DESYNC, nowMs);
    return;
  }
  if (localFinal_) {
    PvpMessage bye;
    bye.type = PVP_MSG_BYE;
    bye.session = sessionId_;
    sendFire(bye);
    enterState(PVP_STATE_FINISHED, nowMs);
    pushEvent(PVP_EVT_FINISHED);
    return;
  }
  pushEvent(PVP_EVT_ROUND_CONFIRMED);
  advanceRound(nowMs);
}

void PvpSession::tickBattle(uint32_t nowMs) {
  bool overdue = state_ == PVP_STATE_BATTLE && phase_ == PVP_PHASE_WAIT_ACTIONS &&
                 !localActionSet_ && !actionTimeoutSent_ &&
                 pvpElapsed(nowMs, roundStartMs_, PVP_ACTION_TIMEOUT_MS);
  if (!overdue) return;
  actionTimeoutSent_ = true;
  pushEvent(PVP_EVT_ACTION_TIMEOUT);
}
