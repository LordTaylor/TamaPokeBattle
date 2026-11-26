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
#include "pvp_time.h"

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
//
// --- Contract for the integrator (T4) ---
// * Identity: init() takes pvpDeviceIdFromMac(own STA MAC), where the MAC comes from
//   esp_wifi_get_mac(WIFI_IF_STA) with WiFi in WIFI_STA mode, and ESP-NOW peers must be
//   added with ifidx = WIFI_IF_STA so the source MAC seen by the receiver is that MAC.
//   Do NOT use (uint32_t)ESP.getEfuseMac(): its byte order is reversed and every packet
//   would be rejected. Receivers drop HELLO / CHALLENGE / ACCEPT whose deviceId does not
//   match the source MAC. This stops id spoofing inside the peer table; it does not stop
//   a modified firmware from choosing its own MAC (esp_wifi_set_mac) to farm points.
// * Memory: sizeof(PvpSession) is about 2.6 KB. Declare it as a static global, never
//   on a task stack and never new/malloc'd in a loop.
// * Threading: call every method from loop() only. The ESP-NOW receive callback must
//   only copy {mac, data, len} into a FreeRTOS queue and DROP packets with len < 0 or
//   len > PVP_MAX_PACKET; loop() then feeds them to onPacket().
// * Pump: after EVERY onPacket()/tick()/caller action drain popPacket() into
//   esp_now_send and popEvent() into the UI. Queues hold PVP_SEND_QUEUE / PVP_EVENT_QUEUE
//   entries and drop the newest on overflow (see droppedPackets/droppedEvents).
// * Time: nowMs is millis(). Wrap-around and slightly earlier timestamps are tolerated
//   (see pvp_time.h). Call tick() at least every ~100 ms.
// * READY.hp may be 0 ("receiver derives it", PVP_HP_MAX bounds it otherwise). Ranges
//   against the Pokedex (stats per level, types per species) are NOT checked here:
//   T4 must run the engine-side validator on peerReady() before using it and call
//   rejectOpponent() when it fails (ABORT, no points).
// * Leaving the result screen: call canLeave(nowMs) first. After FINISHED the session
//   keeps delivering the final hash for up to PVP_LINGER_MS; leaving earlier could
//   make the peer end ABORTED (no points). startDiscovery() enforces the same rule.
// * Outcome: only state()==FINISHED / resultCounts() may award points or badges.
//   ABORTED (including a quitting loser) gives nothing to either side. The quitter
//   is penalised locally (Pet::recordPvpQuit, docs/trainer-level.md "Ucieczka z walki").
// * Deliberately NOT protected (documented in docs/pvp-protocol.md): commit-reveal of
//   nonces and actions, encryption/authentication, stats checked against the Pokedex.
//
// --- File split ---
// pvp_session.cpp            plumbing, queues, onPacket dispatch, live-session tick
// pvp_session_handshake.cpp  discovery, challenge/accept, abort, version, rate limiting
// pvp_session_battle.cpp     READY exchange and lockstep rounds
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
  // The engine-side validator rejected peerReady(): abort with ABORT(BAD_STATS), no points.
  bool rejectOpponent(uint32_t nowMs);

  // --- Inputs ---
  void onPacket(const uint8_t *data, size_t len, const uint8_t mac[6], uint32_t nowMs);
  void tick(uint32_t nowMs);

  // --- Outputs ---
  bool popPacket(PvpOutPacket &out);
  bool popEvent(PvpEvent &out);

  // --- Queries ---
  PvpState state() const { return state_; }
  bool resultCounts() const { return state_ == PVP_STATE_FINISHED; }
  // False while a session is live (use abort()) and while a FINISHED session still
  // has the final hash in flight; true otherwise.
  bool canLeave(uint32_t nowMs) const;
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

  // Handshake (pvp_session_handshake.cpp) and dispatch (pvp_session.cpp)
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
  void tickFinished(uint32_t nowMs);
  void onBye(uint32_t nowMs);
  bool replyAllowed(uint32_t nowMs);
  void fillIdentity(PvpMessage &msg) const;

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
  uint32_t lastReplyMs_ = 0;
  bool replySent_ = false;
  char peerNick_[PVP_NICK_BUF] = {0};
  uint8_t peerLevel_ = 0;
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
