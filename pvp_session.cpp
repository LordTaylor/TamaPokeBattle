#include "pvp_session.h"

#include <string.h>

static_assert(PVP_ACTION_COUNT == BATTLE_REST + 1, "PvP actions must match BattleAction");

namespace {

bool macEqual(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

bool isReliable(uint8_t type) {
  return type == PVP_MSG_CHALLENGE || type == PVP_MSG_ACCEPT || type == PVP_MSG_READY ||
         type == PVP_MSG_ACTION || type == PVP_MSG_ROUND_HASH;
}

}  // namespace

// ---------------------------------------------------------------- plumbing

void PvpSession::init(uint32_t localDeviceId, const char *nick, uint8_t trainerLevel) {
  localId_ = localDeviceId;
  trainerLevel_ = trainerLevel;
  memset(nick_, 0, sizeof(nick_));
  strncpy(nick_, nick ? nick : "", PVP_NICK_MAX);
  if (!pvpValidNick(nick_)) {
    memset(nick_, 0, sizeof(nick_));
    strncpy(nick_, "PLAYER", PVP_NICK_MAX);
  }
  state_ = PVP_STATE_IDLE;
  clearSession();
  peers_.clear();
  sendQueue_.clear();
  events_.clear();
}

void PvpSession::enterState(PvpState s, uint32_t nowMs) {
  state_ = s;
  stateEnterMs_ = nowMs;
  progressMs_ = nowMs;
}

void PvpSession::clearSession() {
  sessionId_ = 0;
  peerId_ = 0;
  memset(peerMac_, 0, sizeof(peerMac_));
  localNonce_ = 0;
  peerNonce_ = 0;
  sideA_ = false;
  nextSeq_ = 1;
  rxWin_.reset();
  tx_.reset();
  localReadySet_ = false;
  peerReadyGot_ = false;
  peerReady_ = PvpReadyPayload();
  round_ = 0;
  nextActionGot_ = false;
  nextAction_ = 0;
  peerActionGot_ = false;
  peerAction_ = 0;
  resetRoundData();
}

bool PvpSession::inSession() const {
  return state_ >= PVP_STATE_CHALLENGING && state_ <= PVP_STATE_FINISHED;
}

bool PvpSession::fromPeer(const PvpMessage &m, const uint8_t *mac) const {
  return inSession() && m.session == sessionId_ && macEqual(mac, peerMac_);
}

void PvpSession::queueUnicast(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  PvpOutPacket p;
  p.broadcast = false;
  memcpy(p.mac, mac, 6);
  p.len = len;
  memcpy(p.data, data, len);
  sendQueue_.push(p);
}

void PvpSession::queueBroadcast(const uint8_t *data, uint8_t len) {
  PvpOutPacket p;
  p.broadcast = true;
  p.len = len;
  memcpy(p.data, data, len);
  sendQueue_.push(p);
}

void PvpSession::sendCallback(void *ctx, const uint8_t *data, uint8_t len) {
  PvpSession *self = static_cast<PvpSession *>(ctx);
  self->queueUnicast(self->peerMac_, data, len);
}

bool PvpSession::sendReliable(PvpMessage &msg, uint16_t round, uint32_t nowMs) {
  msg.session = sessionId_;
  msg.seq = nextSeq_++;
  if (nextSeq_ == 0) nextSeq_ = 1;
  uint8_t buf[PVP_MAX_PACKET];
  size_t n = pvpSerialize(msg, buf, sizeof(buf));
  if (n == 0 || !tx_.add(msg.type, msg.seq, round, buf, (uint8_t)n, nowMs)) {
    abortLocal(PVP_REASON_PROTOCOL, nowMs);
    return false;
  }
  queueUnicast(peerMac_, buf, (uint8_t)n);
  return true;
}

void PvpSession::sendUnreliable(PvpMessage &msg, const uint8_t *mac) {
  msg.seq = 0;
  uint8_t buf[PVP_MAX_PACKET];
  size_t n = pvpSerialize(msg, buf, sizeof(buf));
  if (n) queueUnicast(mac, buf, (uint8_t)n);
}

void PvpSession::sendFire(PvpMessage &msg) {
  for (uint8_t i = 0; i < PVP_FIRE_COPIES; i++) sendUnreliable(msg, peerMac_);
}

void PvpSession::sendAck(uint16_t seq) {
  PvpMessage ack;
  ack.type = PVP_MSG_ACK;
  ack.session = sessionId_;
  ack.ackSeq = seq;
  sendUnreliable(ack, peerMac_);
}

void PvpSession::sendHello(bool broadcast, uint32_t nowMs) {
  PvpMessage hello;
  hello.type = PVP_MSG_HELLO;
  hello.deviceId = localId_;
  memcpy(hello.nick, nick_, PVP_NICK_BUF);
  hello.trainerLevel = trainerLevel_;
  hello.busy = broadcast ? 0 : 1;
  hello.session = broadcast ? 0 : sessionId_;
  hello.seq = 0;
  uint8_t buf[PVP_MAX_PACKET];
  size_t n = pvpSerialize(hello, buf, sizeof(buf));
  if (n == 0) return;
  if (broadcast) queueBroadcast(buf, (uint8_t)n);
  else queueUnicast(peerMac_, buf, (uint8_t)n);
  lastKeepaliveMs_ = nowMs;
}

void PvpSession::pushEvent(uint8_t type, uint8_t reason) {
  PvpEvent e;
  e.type = type;
  e.reason = reason;
  e.peer = peerInfo();
  e.round = round_;
  events_.push(e);
}

PvpPeerInfo PvpSession::peerInfo() const {
  PvpPeerInfo out;
  const PvpPeerInfo *known = peers_.find(peerId_);
  if (known) out = *known;
  out.deviceId = peerId_;
  memcpy(out.mac, peerMac_, 6);
  return out;
}

bool PvpSession::popPacket(PvpOutPacket &out) { return sendQueue_.pop(out); }
bool PvpSession::popEvent(PvpEvent &out) { return events_.pop(out); }

// ------------------------------------------------------------ caller actions

bool PvpSession::startDiscovery(uint32_t nowMs) {
  if (inSession() && state_ != PVP_STATE_FINISHED) return false;
  clearSession();
  peers_.clear();
  enterState(PVP_STATE_DISCOVERY, nowMs);
  nextHelloMs_ = nowMs;
  return true;
}

void PvpSession::stopDiscovery() {
  if (state_ != PVP_STATE_DISCOVERY) return;
  state_ = PVP_STATE_IDLE;
}

bool PvpSession::challenge(uint32_t peerId, uint32_t sessionId, uint32_t nonce, uint32_t nowMs) {
  if (state_ != PVP_STATE_DISCOVERY || sessionId == 0 || peerId == localId_) return false;
  const PvpPeerInfo *peer = peers_.find(peerId);
  if (!peer || peer->busy) return false;
  clearSession();
  sessionId_ = sessionId;
  peerId_ = peerId;
  memcpy(peerMac_, peer->mac, 6);
  localNonce_ = nonce;
  sideA_ = localId_ < peerId_;
  lastRxMs_ = nowMs;
  enterState(PVP_STATE_CHALLENGING, nowMs);
  PvpMessage msg;
  msg.type = PVP_MSG_CHALLENGE;
  msg.deviceId = localId_;
  msg.nonce = localNonce_;
  return sendReliable(msg, 0, nowMs);
}

void PvpSession::establish(uint32_t nowMs) {
  PvpMessage msg;
  msg.type = PVP_MSG_ACCEPT;
  msg.deviceId = localId_;
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

// ----------------------------------------------------------------- inputs

void PvpSession::onPacket(const uint8_t *data, size_t len, const uint8_t mac[6], uint32_t nowMs) {
  PvpMessage m;
  PvpParseResult r = pvpParse(data, len, m);
  if (r == PVP_PARSE_BAD_VERSION) {
    onBadVersion(m, mac, nowMs);
    return;
  }
  if (r != PVP_PARSE_OK) return;
  if (state_ == PVP_STATE_IDLE || state_ == PVP_STATE_ABORTED) return;
  if (isReliable(m.type) && m.seq == 0) return;
  if (m.type == PVP_MSG_HELLO && m.session == 0) {
    onDiscoveryHello(m, mac, nowMs);
    return;
  }
  if (m.type == PVP_MSG_CHALLENGE && !(inSession() && m.session == sessionId_)) {
    onNewChallenge(m, mac, nowMs);
    return;
  }
  if (!fromPeer(m, mac)) return;
  lastRxMs_ = nowMs;
  if (isReliable(m.type) && rxWin_.isDuplicate(m.seq)) {
    sendAck(m.seq);
    return;
  }
  if (dispatch(m, nowMs) && isReliable(m.type)) {
    rxWin_.mark(m.seq);
    sendAck(m.seq);
  }
}

void PvpSession::onBadVersion(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs) {
  if (state_ == PVP_STATE_DISCOVERY && m.type == PVP_MSG_CHALLENGE) {
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
  sideA_ = localId_ < peerId_;
  lastRxMs_ = nowMs;
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
                      macEqual(mac, peerMac_);
  if (simultaneous) {
    if (sideA_) return;  // lower id wins: the peer will accept our challenge
    adoptChallenge(m, mac, nowMs);
    establish(nowMs);    // side B drops its own challenge and accepts A's
    return;
  }
  PvpMessage busy;
  busy.type = PVP_MSG_DECLINE;
  busy.session = m.session;
  sendUnreliable(busy, mac);
}

bool PvpSession::dispatch(const PvpMessage &m, uint32_t nowMs) {
  switch (m.type) {
    case PVP_MSG_ACK: tx_.ack(m.ackSeq); return false;
    case PVP_MSG_ACCEPT: return onAccept(m, nowMs);
    case PVP_MSG_DECLINE: onDecline(nowMs); return false;
    case PVP_MSG_READY: return onReady(m, nowMs);
    case PVP_MSG_ACTION: return onPeerAction(m, nowMs);
    case PVP_MSG_ROUND_HASH: return onPeerHash(m, nowMs);
    case PVP_MSG_ABORT: onRemoteAbort(m, nowMs); return false;
    default: return false;  // HELLO keepalive, BYE: liveness only
  }
}

bool PvpSession::onAccept(const PvpMessage &m, uint32_t nowMs) {
  if (state_ != PVP_STATE_CHALLENGING || m.deviceId != peerId_) return false;
  peerNonce_ = m.nonce;
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

// ------------------------------------------------------------------- tick

void PvpSession::tick(uint32_t nowMs) {
  switch (state_) {
    case PVP_STATE_DISCOVERY: tickDiscovery(nowMs); break;
    case PVP_STATE_CHALLENGING:
    case PVP_STATE_CHALLENGED: tickChallenge(nowMs); break;
    case PVP_STATE_HANDSHAKE:
    case PVP_STATE_READY_EXCHANGE:
    case PVP_STATE_BATTLE: tickLive(nowMs); break;
    case PVP_STATE_FINISHED: tx_.tick(nowMs, &PvpSession::sendCallback, this); break;
    default: break;
  }
}

void PvpSession::tickDiscovery(uint32_t nowMs) {
  PvpEvent e;
  e.type = PVP_EVT_PEER_LOST;
  while (peers_.popExpired(nowMs, e.peer)) events_.push(e);
  if ((int32_t)(nowMs - nextHelloMs_) >= 0) {
    sendHello(true, nowMs);
    nextHelloMs_ = nowMs + PVP_HELLO_INTERVAL_MS;
  }
}

void PvpSession::tickChallenge(uint32_t nowMs) {
  if ((uint32_t)(nowMs - stateEnterMs_) >= PVP_CHALLENGE_TIMEOUT_MS) {
    abortLocal(PVP_REASON_TIMEOUT, nowMs);
    return;
  }
  tx_.tick(nowMs, &PvpSession::sendCallback, this);
}

void PvpSession::tickLive(uint32_t nowMs) {
  bool silent = (uint32_t)(nowMs - lastRxMs_) >= PVP_PEER_TIMEOUT_MS;
  bool stalled = (uint32_t)(nowMs - progressMs_) >= PVP_STALL_MS;
  if (silent || stalled) {
    abortLocal(PVP_REASON_TIMEOUT, nowMs);
    return;
  }
  if ((uint32_t)(nowMs - lastKeepaliveMs_) >= PVP_KEEPALIVE_MS) sendHello(false, nowMs);
  tickBattle(nowMs);
  tx_.tick(nowMs, &PvpSession::sendCallback, this);
}
