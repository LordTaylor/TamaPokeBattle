#pragma once

// Test-only helpers for the PvP protocol: a lossy link between two PvpSession
// instances (loss, duplication, reordering via jitter, delay) and a scripted peer.

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "pvp_session.h"

static int failures = 0;

#define EXPECT_TRUE(expr) do { \
  if (!(expr)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #expr << "\n"; \
    failures++; \
  } \
} while (0)

#define EXPECT_EQ(actual, expected) do { \
  auto pvpA_ = (actual); \
  auto pvpE_ = (expected); \
  if (!(pvpA_ == pvpE_)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #actual \
              << " war " << +pvpA_ << ", erwartet " << +pvpE_ << "\n"; \
    failures++; \
  } \
} while (0)

struct TestRng {
  uint32_t s;
  uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
  bool pct(int p) { return (int)(next() % 100) < p; }
};

struct RoundRecord {
  uint16_t round;
  uint8_t actionA;
  uint8_t actionB;
  uint8_t luck;
};

// One device with an auto-driving "UI" that reacts to events like T4 would.
struct TestDevice {
  PvpSession s;
  uint32_t id = 0;
  uint8_t mac[6] = {0};
  bool initiator = false;
  int lastRound = 3;
  int badHashRound = -1;      // corrupt the hash of this round (desync test)
  bool autoChallenge = true;
  bool autoAccept = true;
  uint32_t chain = 2166136261u;
  std::vector<PvpEvent> log;
  std::vector<RoundRecord> rounds;

  int count(uint8_t type) const {
    int n = 0;
    for (const PvpEvent &e : log) n += e.type == type;
    return n;
  }
  const PvpEvent *find(uint8_t type) const {
    for (const PvpEvent &e : log) if (e.type == type) return &e;
    return nullptr;
  }
};

inline PvpReadyPayload testReady(uint16_t dex = 25) {
  PvpReadyPayload r;
  r.dex = dex; r.shiny = 0; r.level = 30;
  r.hp = 120; r.atk = 70; r.def = 60; r.spe = 80; r.type1 = 5; r.type2 = 0;
  return r;
}

inline uint8_t testAction(uint32_t id, uint16_t round) { return (uint8_t)((id * 7 + round * 3) % 5); }

struct TestLink {
  struct InFlight { int to; uint32_t at; PvpOutPacket p; };
  TestDevice d[2];
  std::vector<InFlight> q;
  TestRng rng{12345};
  int lossPct = 0;
  int dupPct = 0;
  int jitterMs = 0;
  int baseMs = 5;
  bool blackout = false;      // drop everything while true
  uint32_t now = 1000;

  TestLink(uint32_t idA, uint32_t idB) {
    uint32_t ids[2] = { idA, idB };
    for (int i = 0; i < 2; i++) {
      d[i].id = ids[i];
      d[i].mac[0] = (uint8_t)(0x10 * (i + 1));
      d[i].mac[1] = 0;
      for (int b = 0; b < 4; b++) d[i].mac[2 + b] = (uint8_t)(ids[i] >> (24 - 8 * b));  // deviceId = f(MAC)
      char nick[PVP_NICK_BUF] = "PLAYER-A";
      nick[7] = (char)('A' + i);
      d[i].s.init(ids[i], nick, 10);
    }
    d[0].initiator = true;
  }

  void enqueue(int from, const PvpOutPacket &p) {
    if (blackout || rng.pct(lossPct)) return;
    int copies = rng.pct(dupPct) ? 2 : 1;
    for (int c = 0; c < copies; c++) {
      uint32_t delay = (uint32_t)baseMs + (jitterMs ? rng.next() % (uint32_t)(jitterMs + 1) : 0);
      q.push_back({ 1 - from, now + delay, p });
    }
  }

  void react(int i, const PvpEvent &e) {
    TestDevice &me = d[i];
    PvpSession &s = me.s;
    me.log.push_back(e);
    switch (e.type) {
      case PVP_EVT_PEER_FOUND:
        if (me.initiator && me.autoChallenge) s.challenge(e.peer.deviceId, me.id * 1000 + 1, me.id * 0x9E3779B1u, now);
        break;
      case PVP_EVT_CHALLENGE_RECEIVED:
        if (me.autoAccept) s.accept(me.id * 0x85EBCA6Bu, now);
        break;
      case PVP_EVT_ACCEPTED: s.setLocalReady(testReady((uint16_t)(25 + i)), now); break;
      case PVP_EVT_BATTLE_START:
      case PVP_EVT_ROUND_CONFIRMED: s.submitAction(testAction(me.id, s.round()), now); break;
      case PVP_EVT_ACTION_TIMEOUT: s.submitDefaultAction(now); break;
      case PVP_EVT_ROUND_READY: {
        me.rounds.push_back({ e.round, e.actionA, e.actionB, e.luck });
        uint32_t h = me.chain;
        for (uint32_t v : { (uint32_t)e.round, (uint32_t)e.actionA, (uint32_t)e.actionB, (uint32_t)e.luck }) {
          h = (h ^ v) * 16777619u;
        }
        me.chain = h;
        if ((int)e.round == me.badHashRound && i == 1) h ^= 1;
        s.submitRoundHash(h, (int)e.round == me.lastRound, now);
        break;
      }
      default: break;
    }
  }

  void drain() {
    for (int i = 0; i < 2; i++) {
      PvpEvent e;
      while (d[i].s.popEvent(e)) react(i, e);
      PvpOutPacket p;
      while (d[i].s.popPacket(p)) enqueue(i, p);
    }
  }

  void step(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t++) {
      now++;
      for (int i = 0; i < 2; i++) d[i].s.tick(now);
      drain();
      for (size_t k = 0; k < q.size();) {
        if ((int32_t)(now - q[k].at) < 0) { k++; continue; }
        InFlight f = q[k];
        q.erase(q.begin() + (long)k);
        d[f.to].s.onPacket(f.p.data, f.p.len, d[1 - f.to].mac, now);
        drain();
      }
    }
  }

  bool runUntil(uint32_t maxMs, bool (*done)(TestLink &)) {
    for (uint32_t t = 0; t < maxMs; t += 10) {
      step(10);
      if (done(*this)) return true;
    }
    return false;
  }

  bool bothFinished() { return d[0].s.state() == PVP_STATE_FINISHED && d[1].s.state() == PVP_STATE_FINISHED; }
};

// A hand-driven peer for a single PvpSession under test. The session under test
// has the HIGHER id (side B); the scripted peer has the lower id (side A).
struct ScriptedPeer {
  static constexpr uint32_t PEER_ID = 1;
  static constexpr uint32_t LOCAL_ID = 2;
  static constexpr uint32_t SESSION = 777;
  PvpSession s;
  uint8_t peerMac[6] = { 0xAA, 1, 0, 0, 0, 1 };  // pvpDeviceIdFromMac == PEER_ID
  uint32_t now = 5000;
  uint16_t peerSeq = 1;
  std::vector<PvpOutPacket> sent;
  std::vector<PvpEvent> log;

  ScriptedPeer() { s.init(LOCAL_ID, "LOCAL", 7); }

  void pump() {
    PvpOutPacket p;
    while (s.popPacket(p)) sent.push_back(p);
    PvpEvent e;
    while (s.popEvent(e)) log.push_back(e);
  }

  void inject(PvpMessage m, const uint8_t *mac = nullptr) {
    uint8_t buf[PVP_MAX_PACKET];
    size_t n = pvpSerialize(m, buf, sizeof(buf));
    s.onPacket(buf, n, mac ? mac : peerMac, now);
    pump();
  }

  void injectRaw(const uint8_t *buf, size_t n, const uint8_t *mac = nullptr) {
    s.onPacket(buf, n, mac ? mac : peerMac, now);
    pump();
  }

  void advance(uint32_t ms, uint32_t stepMs = 10) {
    for (uint32_t t = 0; t < ms; t += stepMs) { now += stepMs; s.tick(now); pump(); }
  }

  PvpMessage msg(uint8_t type, bool reliable) {
    PvpMessage m;
    m.type = type;
    m.session = SESSION;
    m.seq = reliable ? peerSeq++ : 0;
    return m;
  }

  void hello() {
    PvpMessage m;
    m.type = PVP_MSG_HELLO;
    m.deviceId = PEER_ID;
    strcpy(m.nick, "ASH");
    m.trainerLevel = 5;
    inject(m);
  }

  int count(uint8_t type) const {
    int n = 0;
    for (const PvpEvent &e : log) n += e.type == type;
    return n;
  }

  // Counts packets of `type` sent since `from`, optionally with a given first payload byte.
  int sentCount(uint8_t type, size_t from = 0) const {
    int n = 0;
    for (size_t i = from; i < sent.size(); i++) {
      PvpMessage m;
      if (pvpParse(sent[i].data, sent[i].len, m) == PVP_PARSE_OK && m.type == type) n++;
    }
    return n;
  }

  // discovery -> challenge -> accept -> ready exchange -> BATTLE (round 0)
  void toBattle() {
    s.startDiscovery(now);
    hello();
    s.challenge(PEER_ID, SESSION, 0xAAAA0001u, now);
    pump();
    PvpMessage acc = msg(PVP_MSG_ACCEPT, true);
    acc.deviceId = PEER_ID;
    acc.nonce = 0xBBBB0002u;
    strcpy(acc.nick, "ASH");
    inject(acc);
    s.setLocalReady(testReady(), now);
    PvpMessage rdy = msg(PVP_MSG_READY, true);
    rdy.ready = testReady(150);
    inject(rdy);
    pump();
  }

  void ack(uint16_t seq) {
    PvpMessage a = msg(PVP_MSG_ACK, false);
    a.ackSeq = seq;
    inject(a);
  }
};
