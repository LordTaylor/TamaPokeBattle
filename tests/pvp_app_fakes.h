#pragma once

// Test doubles for PvpApp: FakeLink (transport), FakeHost (profile + ledger) and a rig
// that connects two apps through a lossy / duplicating / delaying "radio".

#include <deque>
#include <string>
#include <vector>

#include "dex.h"
#include "pvp_app.h"
#include "pvp_test_harness.h"
#include "pvp_validator.h"

// One shared, ordered journal of observable calls ("A:send:9", "A:recordQuit", ...).
struct OrderLog {
  std::vector<std::string> entries;
  int count(const std::string &s) const {
    int n = 0;
    for (const std::string &e : entries) n += e == s;
    return n;
  }
  int indexOf(const std::string &s, int from = 0) const {
    for (size_t i = (size_t)from; i < entries.size(); i++) {
      if (entries[i] == s) return (int)i;
    }
    return -1;
  }
  int lastIndexOf(const std::string &s) const {
    for (int i = (int)entries.size() - 1; i >= 0; i--) {
      if (entries[(size_t)i] == s) return i;
    }
    return -1;
  }
};

struct FakeLink : PvpLink {
  std::string tag;
  OrderLog *log = nullptr;
  uint8_t mac[6] = {0};
  bool startOk = true;
  bool up = false;
  int stops = 0;
  uint8_t stuckInFlight = 0;  // pretend the radio never reports the packets as sent
  std::deque<PvpInPacket> inbox;
  std::vector<PvpOutPacket> outbox;  // taken by the rig

  bool start(uint8_t outMac[6]) override {
    if (!startOk) return false;
    memcpy(outMac, mac, 6);
    up = true;
    return true;
  }
  void stop() override { up = false; stops++; log->entries.push_back(tag + ":linkStop"); }
  bool running() const override { return up; }
  uint8_t inFlight() const override { return stuckInFlight; }
  bool send(const PvpOutPacket &p) override {
    if (!up) return false;
    PvpMessage m;
    std::string name = "send:?";
    if (pvpParse(p.data, p.len, m) == PVP_PARSE_OK) {
      name = "send:" + std::to_string(m.type);
      if (m.type == PVP_MSG_ROUND_HASH && m.finalRound) name += "f";
    }
    log->entries.push_back(tag + ":" + name);
    outbox.push_back(p);
    return true;
  }
  bool receive(PvpInPacket &p) override {
    if (inbox.empty()) return false;
    p = inbox.front();
    inbox.pop_front();
    return true;
  }
};

struct MatchRecord {
  PvpResult result;
  std::string nick;
  uint8_t oppLevel;
  uint32_t oppDeviceId;
  uint32_t epoch;
  bool badgeEligible;
};

struct FakeHost : PvpProfile, PvpLedger {
  std::string tag;
  OrderLog *log = nullptr;
  std::string nickStr = "HERO";
  uint8_t level = 10;
  PvpDenyReason deny = PVP_DENY_NONE;
  uint32_t epoch = 100000;
  uint32_t rng = 1;
  PvpReadyPayload ready;
  bool readyOk = true;

  std::vector<MatchRecord> matches;
  int begins = 0, ends = 0, quits = 0;

  bool localReady(PvpReadyPayload &out) override { out = ready; return readyOk; }
  const char *nick() const override { return nickStr.c_str(); }
  uint8_t trainerLevel() const override { return level; }
  bool canFight(PvpDenyReason &r) const override { r = deny; return deny == PVP_DENY_NONE; }
  uint32_t nowEpoch() const override { return epoch; }
  uint32_t random32() override { rng = rng * 1664525u + 1013904223u; return rng; }

  TrainerAward recordMatch(PvpResult result, const char *oppNick, size_t len, uint8_t oppLevel,
                           uint32_t oppDeviceId, uint32_t nowEpoch, bool badgeEligible) override {
    matches.push_back({ result, std::string(oppNick, len), oppLevel, oppDeviceId, nowEpoch, badgeEligible });
    log->entries.push_back(tag + ":recordMatch");
    TrainerAward a;
    a.applied = true;
    return a;
  }
  void battleBegin() override { begins++; log->entries.push_back(tag + ":battleBegin"); }
  void battleEnd() override { ends++; log->entries.push_back(tag + ":battleEnd"); }
  bool recordQuit() override { quits++; log->entries.push_back(tag + ":recordQuit"); return true; }

  // Exactly one outcome per started battle.
  bool ledgerBalanced() const { return begins == (int)matches.size() + ends + quits; }
};

// Legal READY of a species at a level (gene 100, training 0), hp = 0.
inline PvpReadyPayload legalReady(uint16_t dex, uint8_t level) {
  const DexEntry &d = DEX_TBL[dex];
  PvpReadyPayload r;
  r.dex = dex;
  r.level = level;
  r.atk = (uint16_t)(d.bAtk + level);
  r.def = (uint16_t)(d.bDef + level);
  r.spe = (uint16_t)(d.bSpe + level);
  r.type1 = d.type1;
  r.type2 = d.type2;
  return r;
}

struct AppSide {
  FakeLink link;
  FakeHost host;
  PvpApp app;
  AppSide(const std::string &tag, OrderLog *log, uint32_t id, uint16_t dex, uint8_t lvl)
      : app(link, host, host) {
    link.tag = host.tag = tag;
    link.log = host.log = log;
    link.mac[0] = 0x10;
    link.mac[1] = 0;
    for (int b = 0; b < 4; b++) link.mac[2 + b] = (uint8_t)(id >> (24 - 8 * b));
    host.nickStr = "NICK-" + tag;
    host.rng = id * 2654435761u + 7;
    host.ready = legalReady(dex, lvl);
  }
  uint32_t id() const { return pvpDeviceIdFromMac(link.mac); }
  PvpPhase phase() const { return app.model().phase; }
};

// Two apps and a radio between them. Both "players" are bots: A challenges the first
// peer it sees, B accepts, both pick testAction in every round (unless idle).
struct AppRig {
  struct InFlight { int to; uint32_t at; PvpInPacket p; };
  OrderLog log;
  AppSide a, b;
  std::vector<InFlight> q;
  TestRng rng{4242};
  int lossPct = 0, dupPct = 0, jitterMs = 0, baseMs = 5;
  bool blackout = false;
  uint8_t dropType = 0;       // drop every packet of this message type ...
  int dropFrom = -1;          // ... sent by this side (-1 = nobody)
  bool botsAct = true;
  bool idleInBattle = false;  // nobody taps in battle: the session's action timeout takes over
  uint32_t now = 1000;

  AppRig(uint32_t idA = 0x01020304u, uint32_t idB = 0x05060708u)
      : a("A", &log, idA, 25, 30), b("B", &log, idB, 4, 28) {}

  AppSide &side(int i) { return i == 0 ? a : b; }

  void carry(int from) {
    AppSide &s = side(from);
    for (const PvpOutPacket &p : s.link.outbox) {
      if (blackout || rng.pct(lossPct)) continue;
      if (dropFrom == from) {
        PvpMessage m;
        if (pvpParse(p.data, p.len, m) == PVP_PARSE_OK && m.type == dropType) continue;
      }
      int copies = rng.pct(dupPct) ? 2 : 1;
      for (int c = 0; c < copies; c++) {
        PvpInPacket in;
        memcpy(in.mac, s.link.mac, 6);
        in.len = p.len;
        memcpy(in.data, p.data, p.len);
        uint32_t delay = (uint32_t)baseMs + (jitterMs ? rng.next() % (uint32_t)(jitterMs + 1) : 0);
        q.push_back({ 1 - from, now + delay, in });
      }
    }
    s.link.outbox.clear();
  }

  void bots(int i) {
    AppSide &s = side(i);
    const PvpUiModel &m = s.app.model();
    if (!botsAct) return;
    if (m.phase == PVP_UI_SEARCHING && i == 0 && m.peerCount > 0) s.app.challengePeer(0, now);
    if (m.phase == PVP_UI_CHALLENGED) s.app.respondChallenge(true, now);
    if (m.phase == PVP_UI_CHOOSE && !idleInBattle) s.app.submitAction(testAction(s.id(), s.app.model().round), now);
  }

  void step(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t++) {
      now++;
      for (int i = 0; i < 2; i++) {
        for (size_t k = 0; k < q.size();) {
          if (q[k].to != i || (int32_t)(now - q[k].at) < 0) { k++; continue; }
          side(i).link.inbox.push_back(q[k].p);
          q.erase(q.begin() + (long)k);
        }
      }
      for (int i = 0; i < 2; i++) {
        side(i).app.update(now);
        bots(i);
        carry(i);
      }
    }
  }

  bool runUntil(uint32_t maxMs, bool (*done)(AppRig &)) {
    for (uint32_t t = 0; t < maxMs; t += 10) {
      step(10);
      if (done(*this)) return true;
    }
    return false;
  }

  static bool bothResult(AppRig &r) { return r.a.phase() == PVP_UI_RESULT && r.b.phase() == PVP_UI_RESULT; }
  static bool bothInRound(AppRig &r) {
    return r.a.phase() == PVP_UI_CHOOSE && r.b.phase() == PVP_UI_CHOOSE && r.a.app.model().round >= 2 &&
           r.b.app.model().round >= 2;
  }
};
