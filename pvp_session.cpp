#include "pvp_session.h"

#include <string.h>

#include "pvp_time.h"

static_assert(PVP_ACTION_COUNT == BATTLE_REST + 1, "PvP actions must match BattleAction");

namespace {

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
    strncpy(nick_, PVP_DEFAULT_NICK, PVP_NICK_MAX);
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
  memset(peerNick_, 0, sizeof(peerNick_));
  peerLevel_ = 0;
  lastKeepaliveMs_ = 0;
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
  return inSession() && m.session == sessionId_ && pvpMacEqual(mac, peerMac_);
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
  if (peerNick_[0] != '\0') {  // nick announced in CHALLENGE/ACCEPT beats the beacon entry
    memcpy(out.nick, peerNick_, PVP_NICK_BUF);
    out.trainerLevel = peerLevel_;
  }
  return out;
}

bool PvpSession::popPacket(PvpOutPacket &out) { return sendQueue_.pop(out); }
bool PvpSession::popEvent(PvpEvent &out) { return events_.pop(out); }

// ------------------------------------------------------------ caller actions

bool PvpSession::startDiscovery(uint32_t nowMs) {
  if (!canLeave(nowMs)) return false;
  clearSession();
  peers_.clear();
  enterState(PVP_STATE_DISCOVERY, nowMs);
  nextHelloMs_ = nowMs;
  return true;
}

bool PvpSession::canLeave(uint32_t nowMs) const {
  if (state_ == PVP_STATE_FINISHED) {
    return tx_.empty() || pvpElapsed(nowMs, stateEnterMs_, PVP_LINGER_MS);
  }
  return !inSession();
}

void PvpSession::stopDiscovery() {
  if (state_ != PVP_STATE_DISCOVERY) return;
  state_ = PVP_STATE_IDLE;
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
  bool carriesId = m.type == PVP_MSG_HELLO || m.type == PVP_MSG_CHALLENGE || m.type == PVP_MSG_ACCEPT;
  if (carriesId && m.deviceId != pvpDeviceIdFromMac(mac)) return;  // id must match the source MAC
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

bool PvpSession::dispatch(const PvpMessage &m, uint32_t nowMs) {
  switch (m.type) {
    case PVP_MSG_ACK: tx_.ack(m.ackSeq); return false;
    case PVP_MSG_ACCEPT: return onAccept(m, nowMs);
    case PVP_MSG_DECLINE: onDecline(nowMs); return false;
    case PVP_MSG_READY: return onReady(m, nowMs);
    case PVP_MSG_ACTION: return onPeerAction(m, nowMs);
    case PVP_MSG_ROUND_HASH: return onPeerHash(m, nowMs);
    case PVP_MSG_ABORT: onRemoteAbort(m, nowMs); return false;
    case PVP_MSG_BYE: onBye(nowMs); return false;
    default: return false;  // HELLO keepalive: liveness only
  }
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
    case PVP_STATE_FINISHED: tickFinished(nowMs); break;
    default: break;
  }
}

void PvpSession::tickLive(uint32_t nowMs) {
  bool silent = pvpElapsed(nowMs, lastRxMs_, PVP_PEER_TIMEOUT_MS);
  bool stalled = pvpElapsed(nowMs, progressMs_, PVP_STALL_MS);
  if (silent || stalled) {
    abortLocal(PVP_REASON_TIMEOUT, nowMs);
    return;
  }
  if (pvpElapsed(nowMs, lastKeepaliveMs_, PVP_KEEPALIVE_MS)) sendHello(false, nowMs);
  tickBattle(nowMs);
  tx_.tick(nowMs, &PvpSession::sendCallback, this);
}

void PvpSession::tickFinished(uint32_t nowMs) {
  if (pvpElapsed(nowMs, stateEnterMs_, PVP_LINGER_MS)) {
    tx_.reset();  // the peer is gone or already done: stop retransmitting
    return;
  }
  tx_.tick(nowMs, &PvpSession::sendCallback, this);
}

// BYE means "I am finished". If our final hash is already out and we are only
// waiting for the peer's, the peer has verified it: finish too, so both sides
// agree on the outcome even when the peer's final hash was lost.
void PvpSession::onBye(uint32_t nowMs) {
  if (state_ == PVP_STATE_FINISHED) {
    tx_.reset();
    return;
  }
  bool waitingForFinalHash = state_ == PVP_STATE_BATTLE && phase_ == PVP_PHASE_WAIT_HASH &&
                             localHashSet_ && localFinal_;
  if (!waitingForFinalHash) return;
  tx_.reset();  // a peer that finished has everything we sent
  enterState(PVP_STATE_FINISHED, nowMs);
  pushEvent(PVP_EVT_FINISHED);
}
