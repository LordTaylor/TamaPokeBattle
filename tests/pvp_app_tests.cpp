// PvpApp end to end: two apps over a simulated radio. Covers ledger discipline (exactly one of
// recordMatch / recordQuit / battleEnd per battle), quit ordering, walkover, rejected stats.
#include "pvp_app_fakes.h"

static void openBoth(AppRig &r) {
  EXPECT_TRUE(r.a.app.open(r.now));
  EXPECT_TRUE(r.b.app.open(r.now));
}

static bool bothIdle(AppRig &r) { return r.a.phase() == PVP_UI_IDLE && r.b.phase() == PVP_UI_IDLE; }
static bool bothEnded(AppRig &r) { return r.a.phase() == PVP_UI_ENDED && r.b.phase() == PVP_UI_ENDED; }
static bool aEnded(AppRig &r) { return r.a.phase() == PVP_UI_ENDED; }
static bool bEnded(AppRig &r) { return r.b.phase() == PVP_UI_ENDED; }

static void expectMirroredFinish(AppRig &r) {
  EXPECT_EQ((int)r.a.host.matches.size(), 1);
  EXPECT_EQ((int)r.b.host.matches.size(), 1);
  if (r.a.host.matches.empty() || r.b.host.matches.empty()) return;
  PvpResult ra = r.a.host.matches[0].result;
  PvpResult rb = r.b.host.matches[0].result;
  bool mirrored = (ra == PVP_WIN && rb == PVP_LOSS) || (ra == PVP_LOSS && rb == PVP_WIN) ||
                  (ra == PVP_DRAW && rb == PVP_DRAW);
  EXPECT_TRUE(mirrored);
  // identity comes from the sender MAC, not from a payload
  EXPECT_EQ(r.a.host.matches[0].oppDeviceId, r.b.id());
  EXPECT_EQ(r.b.host.matches[0].oppDeviceId, r.a.id());
  EXPECT_TRUE(r.a.host.matches[0].badgeEligible);
  EXPECT_TRUE(r.a.host.matches[0].nick == "NICK-B");
  EXPECT_TRUE(r.b.host.matches[0].nick == "NICK-A");
  EXPECT_EQ(r.a.host.begins, 1);
  EXPECT_EQ(r.a.host.quits + r.a.host.ends, 0);
  EXPECT_TRUE(r.a.host.ledgerBalanced());
  EXPECT_TRUE(r.b.host.ledgerBalanced());
}

static void testFullMatchLossless() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(120000, AppRig::bothResult));
  expectMirroredFinish(r);
  // recordMatch (slow flash save) only after the BYE / last hash of that iteration left the radio
  int bye = r.log.indexOf("A:send:10");
  int rec = r.log.indexOf("A:recordMatch");
  EXPECT_TRUE(bye >= 0 && rec >= 0 && bye < rec);
  EXPECT_TRUE(r.log.indexOf("A:battleBegin") < rec);
  EXPECT_TRUE(r.a.app.model().resultRecorded);
  // OK after the linger: radio stops
  r.step(PVP_LINGER_MS + 200);
  EXPECT_TRUE(r.a.app.leave(r.now));
  EXPECT_TRUE(r.b.app.leave(r.now));
  EXPECT_TRUE(r.runUntil(2000, bothIdle));
  EXPECT_EQ(r.a.link.stops, 1);
}

static void testFullMatchWithLossAndDuplication() {
  AppRig r;
  r.lossPct = 20;
  r.dupPct = 10;
  r.jitterMs = 40;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(240000, AppRig::bothResult));
  expectMirroredFinish(r);
}

static void testIdleBattleTimesOutIntoDefaultActionsAndFinishes() {
  AppRig r;
  r.idleInBattle = true;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(30 * 60 * 1000, AppRig::bothResult));
  expectMirroredFinish(r);
}

static void testQuitSendsAbortThriceBeforePenalty() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(60000, AppRig::bothInRound));
  EXPECT_TRUE(r.a.app.quit(r.now));
  int rec = r.log.indexOf("A:recordQuit");
  EXPECT_TRUE(rec >= 0);
  int aborts = 0;
  for (int i = 0; i < rec; i++) aborts += r.log.entries[(size_t)i] == "A:send:9";
  EXPECT_EQ(aborts, (int)PVP_FIRE_COPIES);  // the penalty never goes out before the ABORT copies
  EXPECT_EQ(r.a.host.quits, 1);
  EXPECT_EQ(r.a.host.ends, 0);  // battleEnd() in the quit branch would clear the flag first
  EXPECT_TRUE(r.a.host.ledgerBalanced());
  EXPECT_TRUE(r.a.app.model().quitPenalty);
  EXPECT_EQ(r.a.app.model().endReason, PVP_END_YOU_QUIT);

  // the rival who stayed: walkover win, no badge, no penalty
  EXPECT_TRUE(r.runUntil(3000, bEnded));
  EXPECT_EQ(r.b.app.model().endReason, PVP_END_PEER_QUIT);
  EXPECT_EQ((int)r.b.host.matches.size(), 1);
  if (!r.b.host.matches.empty()) {
    EXPECT_EQ(r.b.host.matches[0].result, PVP_WIN);
    EXPECT_TRUE(!r.b.host.matches[0].badgeEligible);
  }
  EXPECT_EQ(r.b.host.quits, 0);
  EXPECT_TRUE(r.b.host.ledgerBalanced());
}

static void testLinkLossEndsWithoutPenaltyOrResult() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(60000, AppRig::bothInRound));
  r.blackout = true;
  EXPECT_TRUE(r.runUntil(PVP_PEER_TIMEOUT_MS + 3000, bothEnded));
  EXPECT_EQ(r.a.app.model().endReason, PVP_END_LINK);
  EXPECT_EQ(r.b.app.model().endReason, PVP_END_LINK);
  for (int i = 0; i < 2; i++) {
    FakeHost &h = r.side(i).host;
    EXPECT_EQ(h.ends, 1);
    EXPECT_EQ((int)h.matches.size() + h.quits, 0);
    EXPECT_TRUE(h.ledgerBalanced());
  }
}

static void testForceStopInBattleAbortsThenPenalizes() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(60000, AppRig::bothInRound));
  r.a.app.forceStop(r.now);
  // radio stays up for the flush window so the ABORT copies can leave
  EXPECT_EQ(r.a.phase(), PVP_UI_STOPPING);
  EXPECT_TRUE(r.a.app.radioOn());
  EXPECT_EQ(r.a.link.stops, 0);
  r.a.app.forceStop(r.now);  // repeated calls (called every loop) are harmless
  EXPECT_EQ(r.a.phase(), PVP_UI_STOPPING);
  r.step(PVP_LINK_FLUSH_MS + 10);
  EXPECT_EQ(r.a.phase(), PVP_UI_IDLE);
  EXPECT_EQ(r.log.count("A:linkStop"), 1);
  int rec = r.log.indexOf("A:recordQuit");
  int stop = r.log.indexOf("A:linkStop");
  int aborts = 0;
  for (int i = 0; i < rec; i++) aborts += r.log.entries[(size_t)i] == "A:send:9";
  EXPECT_TRUE(rec >= 0);
  EXPECT_EQ(aborts, (int)PVP_FIRE_COPIES);
  EXPECT_TRUE(stop > rec);
  EXPECT_EQ(r.a.host.ends, 0);
  EXPECT_TRUE(!r.a.app.radioOn());
}

// The card-closed safety net runs every loop: it must not keep postponing the flush deadline.
static void testRepeatedForceStopStillStopsWithinFlushLimit() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(60000, AppRig::bothInRound));
  r.a.link.stuckInFlight = 1;
  for (uint32_t t = 0; t < PVP_LINK_FLUSH_MS + 50; t++) {
    r.a.app.forceStop(r.now);
    r.step(1);
  }
  EXPECT_EQ(r.a.phase(), PVP_UI_IDLE);
  EXPECT_EQ(r.log.count("A:linkStop"), 1);
}

static void testNewSessionNeverInheritsThePreviousEngine() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(120000, AppRig::bothResult));
  EXPECT_TRUE(r.a.app.match().ready());
  r.step(PVP_LINGER_MS + 200);
  EXPECT_TRUE(r.a.app.leave(r.now));
  EXPECT_TRUE(r.b.app.leave(r.now));
  EXPECT_TRUE(r.runUntil(2000, bothIdle));
  r.q.clear();
  r.dropType = PVP_MSG_READY;  // second session stalls in PREPARING
  r.dropFrom = 1;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(10000, [](AppRig &x) { return x.a.phase() == PVP_UI_PREPARING; }));
  EXPECT_TRUE(!r.a.app.match().ready());
  EXPECT_EQ(r.a.host.begins, 1);
}

static void testPeerLeavingBeforeBattleNeverTouchesTheLedger() {
  {  // rival leaves while we are still in PREPARING
    AppRig r;
    r.dropType = PVP_MSG_READY;
    r.dropFrom = 1;
    openBoth(r);
    EXPECT_TRUE(r.runUntil(10000, [](AppRig &x) { return x.a.phase() == PVP_UI_PREPARING && x.b.phase() == PVP_UI_PREPARING; }));
    EXPECT_TRUE(r.b.app.leave(r.now));
    EXPECT_TRUE(r.runUntil(3000, aEnded));
    EXPECT_EQ(r.a.app.model().endReason, PVP_END_PEER_QUIT);
    EXPECT_EQ(r.a.host.begins + (int)r.a.host.matches.size() + r.a.host.quits + r.a.host.ends, 0);
  }
  {  // challenger withdraws: the challenged side goes back to the list
    AppRig r;
    r.botsAct = false;
    openBoth(r);
    EXPECT_TRUE(r.runUntil(5000, [](AppRig &x) { return x.a.app.model().peerCount > 0; }));
    EXPECT_TRUE(r.a.app.challengePeer(0, r.now));
    EXPECT_TRUE(r.runUntil(3000, [](AppRig &x) { return x.b.phase() == PVP_UI_CHALLENGED; }));
    EXPECT_TRUE(r.a.app.leave(r.now));
    EXPECT_TRUE(r.runUntil(3000, [](AppRig &x) { return x.b.phase() == PVP_UI_SEARCHING; }));
    EXPECT_EQ(r.b.app.model().notice, PVP_NOTICE_EXPIRED);
  }
  {  // challenged side leaves instead of answering: the challenger sees it as a decline
    AppRig r;
    r.botsAct = false;
    openBoth(r);
    EXPECT_TRUE(r.runUntil(5000, [](AppRig &x) { return x.a.app.model().peerCount > 0; }));
    EXPECT_TRUE(r.a.app.challengePeer(0, r.now));
    EXPECT_TRUE(r.runUntil(3000, [](AppRig &x) { return x.b.phase() == PVP_UI_CHALLENGED; }));
    EXPECT_TRUE(r.b.app.leave(r.now));
    EXPECT_TRUE(r.runUntil(3000, [](AppRig &x) { return x.a.app.model().notice == PVP_NOTICE_DECLINED; }));
    EXPECT_EQ(r.a.phase(), PVP_UI_SEARCHING);
  }
}

static void testRejectedStatsNeverStartABattleEvenAfterEarlierMatch() {
  AppRig r;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(120000, AppRig::bothResult));
  r.step(PVP_LINGER_MS + 200);
  EXPECT_TRUE(r.a.app.leave(r.now));
  EXPECT_TRUE(r.b.app.leave(r.now));
  EXPECT_TRUE(r.runUntil(2000, bothIdle));
  EXPECT_EQ(r.a.host.begins, 1);

  // second session: B announces an attack stat outside the legal band of its species
  r.b.host.ready.atk = (uint16_t)(r.b.host.ready.atk + 150);
  r.q.clear();
  openBoth(r);
  EXPECT_TRUE(r.runUntil(30000, aEnded));
  EXPECT_EQ(r.a.app.model().endReason, PVP_END_STATS);
  EXPECT_EQ(r.a.host.begins, 1);  // BATTLE_START after rejectOpponent is ignored
  EXPECT_EQ((int)r.a.host.matches.size(), 1);
  EXPECT_EQ(r.a.host.quits + r.a.host.ends, 0);
  r.step(2000);
  EXPECT_TRUE(r.b.host.ledgerBalanced());
  EXPECT_EQ(r.b.host.quits, 0);  // the rejected side is never penalized
}

static void testDeclineReturnsToSearchWithoutLedgerCalls() {
  AppRig r;
  r.botsAct = false;
  openBoth(r);
  EXPECT_TRUE(r.runUntil(5000, [](AppRig &x) { return x.a.app.model().peerCount > 0; }));
  EXPECT_TRUE(r.a.app.challengePeer(0, r.now));
  EXPECT_TRUE(r.runUntil(3000, [](AppRig &x) { return x.b.phase() == PVP_UI_CHALLENGED; }));
  EXPECT_TRUE(r.b.app.respondChallenge(false, r.now));
  EXPECT_TRUE(r.runUntil(3000, [](AppRig &x) { return x.a.app.model().notice == PVP_NOTICE_DECLINED; }));
  EXPECT_EQ(r.a.phase(), PVP_UI_SEARCHING);
  EXPECT_EQ(r.b.phase(), PVP_UI_SEARCHING);
  EXPECT_EQ(r.a.host.begins + r.b.host.begins, 0);
}

static void testOpenRefusalsLeaveRadioOff() {
  AppRig r;
  r.a.host.deny = PVP_DENY_LOW_BATTERY;
  EXPECT_TRUE(!r.a.app.open(r.now));
  EXPECT_EQ(r.a.app.model().deny, PVP_DENY_LOW_BATTERY);
  EXPECT_TRUE(!r.a.app.radioOn());
  r.b.link.startOk = false;
  EXPECT_TRUE(!r.b.app.open(r.now));
  EXPECT_EQ(r.b.app.model().endReason, PVP_END_RADIO_FAIL);
  EXPECT_EQ(r.b.phase(), PVP_UI_IDLE);
}

static void testPumpHandlesAtMostEightPacketsPerUpdate() {
  AppRig r;
  EXPECT_TRUE(r.a.app.open(r.now));
  for (int i = 0; i < 20; i++) {
    PvpInPacket p;
    p.len = 4;
    p.data[0] = PVP_MAGIC & 0xFF;
    p.data[1] = PVP_MAGIC >> 8;
    r.a.link.inbox.push_back(p);
  }
  r.a.app.update(r.now + 1);
  EXPECT_EQ((int)r.a.link.inbox.size(), 20 - (int)PVP_PUMP_MAX_RX);
}

int main() {
  testFullMatchLossless();
  testFullMatchWithLossAndDuplication();
  testQuitSendsAbortThriceBeforePenalty();
  testLinkLossEndsWithoutPenaltyOrResult();
  testForceStopInBattleAbortsThenPenalizes();
  testRejectedStatsNeverStartABattleEvenAfterEarlierMatch();
  testRepeatedForceStopStillStopsWithinFlushLimit();
  testNewSessionNeverInheritsThePreviousEngine();
  testPeerLeavingBeforeBattleNeverTouchesTheLedger();
  testDeclineReturnsToSearchWithoutLedgerCalls();
  testOpenRefusalsLeaveRadioOff();
  testPumpHandlesAtMostEightPacketsPerUpdate();
  testIdleBattleTimesOutIntoDefaultActionsAndFinishes();
  if (failures) {
    std::cerr << failures << " Fehler\n";
    return 1;
  }
  std::cout << "pvp_app_tests: OK\n";
  return 0;
}
