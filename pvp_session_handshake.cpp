// Discovery, challenge/accept handshake and session teardown of PvpSession.
#include <string.h>

#include "pvp_session.h"
#include "pvp_time.h"

bool PvpSession::challenge(uint32_t peerId, uint32_t sessionId, uint32_t nonce, uint32_t nowMs) {
  if (state_ != PVP_STATE_DISCOVERY || sessionId == 0 || peerId == localId_) return false;
  const PvpPeerInfo *peer = peers_.find(peerId);
  if (!peer) return false;
  clearSession();
  sessionId_ = sessionId;
  peerId_ = peerId;
  memcpy(peerMac_, peer->mac, 6);
  localNonce_ = nonce;
  memcpy(peerNick_, peer->nick, PVP_NICK_BUF);
  peerLevel_ = peer->trainerLevel;
  sideA_ = localId_ < peerId_;
  lastRxMs_ = nowMs;
  lastKeepaliveMs_ = nowMs;
  enterState(PVP_STATE_CHALLENGING, nowMs);
  PvpMessage msg;
  msg.type = PVP_MSG_CHALLENGE;
  fillIdentity(msg);
  msg.nonce = localNonce_;
  return sendReliable(msg, 0, nowMs);
}

void PvpSession::establish(uint32_t nowMs) {
  PvpMessage msg;
  msg.type = PVP_MSG_ACCEPT;
  fillIdentity(msg);
  msg.nonce = localNonce_;
  enterState(PVP_STATE_HANDSHAKE, nowMs);
  if (sendReliable(msg, 0, nowMs)) pushEvent(PVP_EVT_ACCEPTED);
}

bool PvpSession::accept(uint32_t nonce, uint32_t nowMs) {
  if (state_ != PVP_STATE_CHALLENGED) return false;
  localNonce_ = nonce;
  establish(nowMs);
  return state_ == PVP_STATE_HANDSHAKE;
}

bool PvpSession::decline(uint32_t nowMs) {
  if (state_ != PVP_STATE_CHALLENGED) return false;
  PvpMessage msg;
  msg.type = PVP_MSG_DECLINE;
  msg.session = sessionId_;
  sendFire(msg);
  toDiscovery(nowMs);
  return true;
}

void PvpSession::abort(uint32_t nowMs) {
  if (state_ == PVP_STATE_DISCOVERY) {
    stopDiscovery();
  } else if (inSession() && state_ != PVP_STATE_FINISHED) {
    abortLocal(PVP_REASON_USER, nowMs);
  }
}

void PvpSession::abortLocal(uint8_t reason, uint32_t nowMs) {
  PvpMessage msg;
  msg.type = PVP_MSG_ABORT;
  msg.session = sessionId_;
  msg.reason = reason;
  sendFire(msg);
  tx_.reset();
  enterState(PVP_STATE_ABORTED, nowMs);
  pushEvent(PVP_EVT_ABORTED, reason);
}

void PvpSession::toDiscovery(uint32_t nowMs) {
  clearSession();
  enterState(PVP_STATE_DISCOVERY, nowMs);
  nextHelloMs_ = nowMs;
}

void PvpSession::onBadVersion(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs) {
  if (state_ == PVP_STATE_DISCOVERY && m.type == PVP_MSG_CHALLENGE) {
    if (!replyAllowed(nowMs)) return;
    PvpEvent e;
    e.type = PVP_EVT_VERSION_MISMATCH;
    memcpy(e.peer.mac, mac, 6);
    events_.push(e);
    PvpMessage abortMsg;
    abortMsg.type = PVP_MSG_ABORT;
    abortMsg.session = m.session;
    abortMsg.reason = PVP_REASON_VERSION;
    sendUnreliable(abortMsg, mac);
  } else if (state_ == PVP_STATE_CHALLENGING && fromPeer(m, mac)) {
    pushEvent(PVP_EVT_VERSION_MISMATCH);
    tx_.reset();
    enterState(PVP_STATE_ABORTED, nowMs);
    pushEvent(PVP_EVT_ABORTED, PVP_REASON_VERSION);
  }
}

void PvpSession::onDiscoveryHello(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs) {
  if (state_ != PVP_STATE_DISCOVERY || m.deviceId == localId_) return;
  PvpPeerTable::UpdateResult res = peers_.update(m, mac, nowMs);
  if (res != PvpPeerTable::ADDED) return;
  PvpEvent e;
  e.type = PVP_EVT_PEER_FOUND;
  e.peer = *peers_.find(m.deviceId);
  events_.push(e);
}

void PvpSession::adoptChallenge(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs) {
  uint32_t keepNonce = localNonce_;
  clearSession();
  localNonce_ = keepNonce;
  sessionId_ = m.session;
  peerId_ = m.deviceId;
  memcpy(peerMac_, mac, 6);
  peerNonce_ = m.nonce;
  memcpy(peerNick_, m.nick, PVP_NICK_BUF);
  peerLevel_ = m.trainerLevel;
  sideA_ = localId_ < peerId_;
  lastRxMs_ = nowMs;
  lastKeepaliveMs_ = nowMs;
  rxWin_.mark(m.seq);
  sendAck(m.seq);
}

void PvpSession::onNewChallenge(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs) {
  if (m.deviceId == localId_ || m.session == 0 || state_ == PVP_STATE_FINISHED) return;
  if (state_ == PVP_STATE_DISCOVERY) {
    adoptChallenge(m, mac, nowMs);
    enterState(PVP_STATE_CHALLENGED, nowMs);
    pushEvent(PVP_EVT_CHALLENGE_RECEIVED);
    return;
  }
  bool simultaneous = state_ == PVP_STATE_CHALLENGING && m.deviceId == peerId_ &&
                      pvpMacEqual(mac, peerMac_);
  if (simultaneous) {
    if (sideA_) return;  // lower id wins: the peer will accept our challenge
    adoptChallenge(m, mac, nowMs);
    establish(nowMs);    // side B drops its own challenge and accepts A's
    return;
  }
  if (!replyAllowed(nowMs)) return;
  PvpMessage busy;
  busy.type = PVP_MSG_DECLINE;
  busy.session = m.session;
  sendUnreliable(busy, mac);
}

bool PvpSession::onAccept(const PvpMessage &m, uint32_t nowMs) {
  if (state_ != PVP_STATE_CHALLENGING || m.deviceId != peerId_) return false;
  peerNonce_ = m.nonce;
  memcpy(peerNick_, m.nick, PVP_NICK_BUF);
  peerLevel_ = m.trainerLevel;
  tx_.drop(PVP_MSG_CHALLENGE, false, 0);
  enterState(PVP_STATE_HANDSHAKE, nowMs);
  pushEvent(PVP_EVT_ACCEPTED);
  return true;
}

void PvpSession::onDecline(uint32_t nowMs) {
  if (state_ != PVP_STATE_CHALLENGING) return;
  pushEvent(PVP_EVT_DECLINED);
  toDiscovery(nowMs);
}

void PvpSession::onRemoteAbort(const PvpMessage &m, uint32_t nowMs) {
  if (state_ == PVP_STATE_FINISHED) return;
  if (m.reason == PVP_REASON_DESYNC) pushEvent(PVP_EVT_DESYNC);
  if (m.reason == PVP_REASON_VERSION) pushEvent(PVP_EVT_VERSION_MISMATCH);
  tx_.reset();
  enterState(PVP_STATE_ABORTED, nowMs);
  pushEvent(PVP_EVT_ABORTED, m.reason);
}

void PvpSession::tickDiscovery(uint32_t nowMs) {
  PvpEvent e;
  e.type = PVP_EVT_PEER_LOST;
  while (peers_.popExpired(nowMs, e.peer)) events_.push(e);
  if (pvpElapsed(nowMs, nextHelloMs_, 0)) {
    sendHello(true, nowMs);
    nextHelloMs_ = nowMs + PVP_HELLO_INTERVAL_MS;
  }
}

void PvpSession::tickChallenge(uint32_t nowMs) {
  if (pvpElapsed(nowMs, stateEnterMs_, PVP_CHALLENGE_TIMEOUT_MS)) {
    abortLocal(PVP_REASON_TIMEOUT, nowMs);
    return;
  }
  tx_.tick(nowMs, &PvpSession::sendCallback, this);
}


// Unsolicited DECLINE / VERSION replies are rate limited so a flood of foreign
// CHALLENGEs can neither fill the send queue nor push real events out.
bool PvpSession::replyAllowed(uint32_t nowMs) {
  if (replySent_ && !pvpElapsed(nowMs, lastReplyMs_, PVP_REPLY_MIN_INTERVAL_MS)) return false;
  replySent_ = true;
  lastReplyMs_ = nowMs;
  return true;
}

void PvpSession::fillIdentity(PvpMessage &msg) const {
  msg.deviceId = localId_;
  memcpy(msg.nick, nick_, PVP_NICK_BUF);
  msg.trainerLevel = trainerLevel_;
}
