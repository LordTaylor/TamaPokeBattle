#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pvp_event.h"
#include "pvp_out_packet.h"
#include "pvp_packet.h"
#include "pvp_peers.h"
#include "pvp_reliable.h"
#include "pvp_ring.h"
#include "pvp_seq_window.h"
#include "pvp_state.h"

// PvP session state machine. Pure logic: no I/O, no clock, no randomness.
// The caller feeds packets (onPacket) and time (tick / nowMs arguments), drains
// popPacket() into the transport and popEvent() into the UI.
//
// Typical flow (both sides symmetric):
//   startDiscovery -> [challenge | ChallengeReceived -> accept/decline]
//   -> Accepted -> setLocalReady -> OpponentReady -> per round:
//   submitAction -> RoundReady{actionA,actionB,luck} -> (engine) -> submitRoundHash
//   -> RoundConfirmed ... last round: submitRoundHash(hash, true) -> Finished.
// Side A is the device with the LOWER deviceId; RoundReady actions are oriented A/B.
class PvpSession {
 public:
  void init(uint32_t localDeviceId, const char *nick, uint8_t trainerLevel);

  // --- Caller actions (all return false when invalid in the current state) ---
  bool startDiscovery(uint32_t nowMs);
  void stopDiscovery();
  bool challenge(uint32_t peerId, uint32_t sessionId, uint32_t nonce, uint32_t nowMs);
  bool accept(uint32_t nonce, uint32_t nowMs);
  bool decline(uint32_t nowMs);
  bool setLocalReady(const PvpReadyPayload &ready, uint32_t nowMs);
  bool submitAction(uint8_t action, uint32_t nowMs);
  bool submitDefaultAction(uint32_t nowMs);  // after ActionTimeout: PVP_DEFAULT_ACTION
  bool submitRoundHash(uint32_t hash, bool battleOver, uint32_t nowMs);
  void abort(uint32_t nowMs);

  // --- Inputs ---
  void onPacket(const uint8_t *data, size_t len, const uint8_t mac[6], uint32_t nowMs);
  void tick(uint32_t nowMs);

  // --- Outputs ---
  bool popPacket(PvpOutPacket &out);
  bool popEvent(PvpEvent &out);

  // --- Queries ---
  PvpState state() const { return state_; }
  bool resultCounts() const { return state_ == PVP_STATE_FINISHED; }
  bool isSideA() const { return sideA_; }
  uint32_t sessionId() const { return sessionId_; }
  uint16_t round() const { return round_; }
  const PvpPeerTable &peers() const { return peers_; }
  const PvpReadyPayload &peerReady() const { return peerReady_; }
  uint16_t droppedPackets() const { return sendQueue_.dropped(); }
  uint16_t droppedEvents() const { return events_.dropped(); }

 private:
  // Plumbing (pvp_session.cpp)
  void enterState(PvpState s, uint32_t nowMs);
  void clearSession();
  void queueUnicast(const uint8_t *mac, const uint8_t *data, uint8_t len);
  void queueBroadcast(const uint8_t *data, uint8_t len);
  bool sendReliable(PvpMessage &msg, uint16_t round, uint32_t nowMs);
  void sendUnreliable(PvpMessage &msg, const uint8_t *mac);
  void sendFire(PvpMessage &msg);
  void sendAck(uint16_t seq);
  void sendHello(bool broadcast, uint32_t nowMs);
  void pushEvent(uint8_t type, uint8_t reason = 0);
  PvpPeerInfo peerInfo() const;
  bool inSession() const;
  bool fromPeer(const PvpMessage &m, const uint8_t *mac) const;
  static void sendCallback(void *ctx, const uint8_t *data, uint8_t len);

  // Handshake and dispatch (pvp_session.cpp)
  void onBadVersion(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs);
  void onDiscoveryHello(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs);
  void onNewChallenge(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs);
  void adoptChallenge(const PvpMessage &m, const uint8_t *mac, uint32_t nowMs);
  bool dispatch(const PvpMessage &m, uint32_t nowMs);
  bool onAccept(const PvpMessage &m, uint32_t nowMs);
  void onDecline(uint32_t nowMs);
  void onRemoteAbort(const PvpMessage &m, uint32_t nowMs);
  void establish(uint32_t nowMs);
  void abortLocal(uint8_t reason, uint32_t nowMs);
  void toDiscovery(uint32_t nowMs);
  void tickDiscovery(uint32_t nowMs);
  void tickChallenge(uint32_t nowMs);
  void tickLive(uint32_t nowMs);

  // Battle (pvp_session_battle.cpp)
  bool onReady(const PvpMessage &m, uint32_t nowMs);
  bool onPeerAction(const PvpMessage &m, uint32_t nowMs);
  bool onPeerHash(const PvpMessage &m, uint32_t nowMs);
  void tryEnterBattle(uint32_t nowMs);
  void tryRoundReady(uint32_t nowMs);
  void tryConfirm(uint32_t nowMs);
  void beginRound(uint32_t nowMs);
  void advanceRound(uint32_t nowMs);
  void tickBattle(uint32_t nowMs);
  void resetRoundData();

  // Identity
  uint32_t localId_ = 0;
  char nick_[PVP_NICK_BUF] = {0};
  uint8_t trainerLevel_ = 1;

  // Session
  PvpState state_ = PVP_STATE_IDLE;
  uint32_t sessionId_ = 0;
  uint32_t peerId_ = 0;
  uint8_t peerMac_[6] = {0};
  uint32_t localNonce_ = 0;
  uint32_t peerNonce_ = 0;
  bool sideA_ = false;
  uint16_t nextSeq_ = 1;
  PvpSeqWindow rxWin_;
  PvpReliableTx tx_;
  uint32_t lastRxMs_ = 0;
  uint32_t lastKeepaliveMs_ = 0;
  uint32_t nextHelloMs_ = 0;
  uint32_t stateEnterMs_ = 0;
  uint32_t progressMs_ = 0;

  // Ready exchange
  bool localReadySet_ = false;
  bool peerReadyGot_ = false;
  PvpReadyPayload peerReady_;

  // Rounds: current slot and one buffered round ahead
  uint16_t round_ = 0;
  uint8_t phase_ = PVP_PHASE_WAIT_ACTIONS;
  uint32_t roundStartMs_ = 0;
  bool actionTimeoutSent_ = false;
  bool localActionSet_ = false;
  uint8_t localAction_ = 0;
  bool peerActionGot_ = false;
  uint8_t peerAction_ = 0;
  bool nextActionGot_ = false;
  uint8_t nextAction_ = 0;
  bool localHashSet_ = false;
  uint32_t localHash_ = 0;
  bool localFinal_ = false;
  bool peerHashGot_ = false;
  uint32_t peerHash_ = 0;
  bool peerFinal_ = false;

  PvpPeerTable peers_;
  PvpRing<PvpOutPacket, PVP_SEND_QUEUE> sendQueue_;
  PvpRing<PvpEvent, PVP_EVENT_QUEUE> events_;
};
