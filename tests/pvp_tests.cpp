#include "pvp_test_harness.h"

// ---------------------------------------------------------------- packets

static PvpMessage sampleMsg(uint8_t type) {
  PvpMessage m;
  m.type = type;
  m.session = 0xDEADBEEF;
  m.seq = 0x1234;
  m.deviceId = 0xCAFEBABE;
  m.nonce = 0x01020304;
  strcpy(m.nick, "ASH.KETCHU");
  m.trainerLevel = 42;
  m.busy = 1;
  m.ready = testReady(386);
  m.ready.shiny = 1;
  m.round = 513;
  m.action = 4;
  m.hash = 0xA1B2C3D4;
  m.finalRound = 1;
  m.ackSeq = 0xFFEE;
  m.reason = PVP_REASON_DESYNC;
  return m;
}

static const uint8_t ALL_TYPES[] = {
  PVP_MSG_HELLO, PVP_MSG_CHALLENGE, PVP_MSG_ACCEPT, PVP_MSG_DECLINE, PVP_MSG_READY,
  PVP_MSG_ACTION, PVP_MSG_ROUND_HASH, PVP_MSG_ACK, PVP_MSG_ABORT, PVP_MSG_BYE,
};

static void testCrcVector() {
  const uint8_t data[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  EXPECT_EQ(pvpCrc16(data, sizeof(data)), (uint16_t)0x29B1);
}

static void testRoundTripAllTypes() {
  for (uint8_t type : ALL_TYPES) {
    PvpMessage in = sampleMsg(type);
    uint8_t buf[PVP_MAX_PACKET];
    size_t n = pvpSerialize(in, buf, sizeof(buf));
    EXPECT_EQ(n, (size_t)(PVP_HEADER_SIZE + pvpPayloadSize(type)));
    EXPECT_TRUE(n <= PVP_ESPNOW_MAX);
    PvpMessage out;
    EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_OK);
    EXPECT_EQ(out.type, type);
    EXPECT_EQ(out.session, in.session);
    EXPECT_EQ(out.seq, in.seq);
    if (type == PVP_MSG_HELLO) {
      EXPECT_EQ(out.deviceId, in.deviceId);
      EXPECT_TRUE(strcmp(out.nick, "ASH.KETCHU") == 0);
      EXPECT_EQ(out.trainerLevel, in.trainerLevel);
      EXPECT_EQ(out.busy, in.busy);
      EXPECT_EQ(out.version, PVP_PROTO_VERSION);
    }
    if (type == PVP_MSG_CHALLENGE || type == PVP_MSG_ACCEPT) {
      EXPECT_EQ(out.deviceId, in.deviceId);
      EXPECT_EQ(out.nonce, in.nonce);
    }
    if (type == PVP_MSG_READY) {
      EXPECT_EQ(out.ready.dex, in.ready.dex);
      EXPECT_EQ(out.ready.shiny, in.ready.shiny);
      EXPECT_EQ(out.ready.level, in.ready.level);
      EXPECT_EQ(out.ready.hp, in.ready.hp);
      EXPECT_EQ(out.ready.atk, in.ready.atk);
      EXPECT_EQ(out.ready.def, in.ready.def);
      EXPECT_EQ(out.ready.spe, in.ready.spe);
      EXPECT_EQ(out.ready.type1, in.ready.type1);
    }
    if (type == PVP_MSG_ACTION) { EXPECT_EQ(out.round, in.round); EXPECT_EQ(out.action, in.action); }
    if (type == PVP_MSG_ROUND_HASH) {
      EXPECT_EQ(out.round, in.round);
      EXPECT_EQ(out.hash, in.hash);
      EXPECT_EQ(out.finalRound, in.finalRound);
    }
    if (type == PVP_MSG_ACK) EXPECT_EQ(out.ackSeq, in.ackSeq);
    if (type == PVP_MSG_ABORT) EXPECT_EQ(out.reason, in.reason);
  }
}

static void testWireIsLittleEndian() {
  PvpMessage m = sampleMsg(PVP_MSG_ACTION);
  uint8_t buf[PVP_MAX_PACKET];
  size_t n = pvpSerialize(m, buf, sizeof(buf));
  EXPECT_EQ(n, (size_t)16);
  EXPECT_EQ(buf[0], 0x50);  // magic 0x5450 little-endian
  EXPECT_EQ(buf[1], 0x54);
  EXPECT_EQ(buf[2], PVP_PROTO_VERSION);
  EXPECT_EQ(buf[4], 0xEF);  // session low byte first
  EXPECT_EQ(buf[7], 0xDE);
  EXPECT_EQ(buf[13], 0x01); // round 513 = 0x0201
  EXPECT_EQ(buf[14], 0x02);
}

static size_t serialized(uint8_t type, uint8_t *buf) {
  return pvpSerialize(sampleMsg(type), buf, PVP_MAX_PACKET);
}

static void testParseRejectsBadFraming() {
  uint8_t buf[PVP_MAX_PACKET];
  PvpMessage out;
  size_t n = serialized(PVP_MSG_ACTION, buf);

  uint8_t bad[PVP_MAX_PACKET];
  memcpy(bad, buf, n);
  bad[0] ^= 0xFF;  // magic
  EXPECT_EQ((int)pvpParse(bad, n, out), (int)PVP_PARSE_BAD_MAGIC);

  memcpy(bad, buf, n);
  bad[14] ^= 0x01;  // payload bit without re-sealing: CRC must catch it
  EXPECT_EQ((int)pvpParse(bad, n, out), (int)PVP_PARSE_BAD_CRC);

  memcpy(bad, buf, n);
  bad[11] ^= 0x80;  // crc field itself
  EXPECT_EQ((int)pvpParse(bad, n, out), (int)PVP_PARSE_BAD_CRC);

  EXPECT_EQ((int)pvpParse(buf, n - 1, out), (int)PVP_PARSE_BAD_LENGTH);   // truncated
  EXPECT_EQ((int)pvpParse(buf, 5, out), (int)PVP_PARSE_TOO_SHORT);
  EXPECT_EQ((int)pvpParse(buf, 0, out), (int)PVP_PARSE_TOO_SHORT);

  memcpy(bad, buf, n);
  bad[n] = 0;  // one trailing byte
  EXPECT_EQ((int)pvpParse(bad, n + 1, out), (int)PVP_PARSE_BAD_LENGTH);
  uint8_t huge[PVP_ESPNOW_MAX];
  memset(huge, 0, sizeof(huge));
  memcpy(huge, buf, n);
  EXPECT_EQ((int)pvpParse(huge, sizeof(huge), out), (int)PVP_PARSE_BAD_LENGTH);
}

static void testParseRejectsBadTypeAndLen() {
  uint8_t buf[PVP_MAX_PACKET];
  PvpMessage out;
  size_t n = serialized(PVP_MSG_ACK, buf);
  buf[3] = 99;  // unknown type, valid CRC
  pvpSealCrc(buf, n);
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_TYPE);
  buf[3] = 0;
  pvpSealCrc(buf, n);
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_TYPE);

  n = serialized(PVP_MSG_ACK, buf);  // wrong declared length for the type (HELLO needs 18)
  buf[3] = PVP_MSG_HELLO;
  pvpSealCrc(buf, n);
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_LENGTH);
}

static void testParseVersionMismatchKeepsHeader() {
  uint8_t buf[PVP_MAX_PACKET];
  PvpMessage out;
  size_t n = serialized(PVP_MSG_CHALLENGE, buf);
  buf[2] = PVP_PROTO_VERSION + 1;
  pvpSealCrc(buf, n);
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_VERSION);
  EXPECT_EQ(out.type, PVP_MSG_CHALLENGE);
  EXPECT_EQ(out.session, 0xDEADBEEFu);
}

static void testValueRanges() {
  PvpReadyPayload ok = testReady();
  EXPECT_TRUE(pvpValidReady(ok));
  PvpReadyPayload r = ok; r.level = 0;       EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.level = 101;                      EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.dex = 0;                          EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.dex = 387;                        EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.hp = 0;                           EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.hp = PVP_STAT_MAX + 1;            EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.atk = 65535;                      EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.def = 0;                          EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.spe = 1000;                       EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.type1 = 19;                       EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.type2 = 200;                      EXPECT_TRUE(!pvpValidReady(r));
  r = ok; r.shiny = 2;                        EXPECT_TRUE(!pvpValidReady(r));

  // The same ranges apply on the wire: a hand-crafted READY with level 101 is rejected.
  uint8_t buf[PVP_MAX_PACKET];
  PvpMessage m = sampleMsg(PVP_MSG_READY);
  size_t n = pvpSerialize(m, buf, sizeof(buf));
  buf[PVP_HEADER_SIZE + 3] = 101;
  pvpSealCrc(buf, n);
  PvpMessage out;
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_VALUE);
  r = ok; r.level = 101; m.ready = r;
  EXPECT_EQ(pvpSerialize(m, buf, sizeof(buf)), (size_t)0);  // never send invalid values
}

static void testNickAndActionValidation() {
  char nick[PVP_NICK_BUF] = {0};
  strcpy(nick, "ASH-K.");
  EXPECT_TRUE(pvpValidNick(nick));
  strcpy(nick, "ash");     EXPECT_TRUE(!pvpValidNick(nick));
  strcpy(nick, "AS H");    EXPECT_TRUE(!pvpValidNick(nick));
  strcpy(nick, "ASH1");    EXPECT_TRUE(!pvpValidNick(nick));
  memset(nick, 0, sizeof(nick));  EXPECT_TRUE(!pvpValidNick(nick));  // empty
  memset(nick, 'A', PVP_NICK_BUF);                                   // no NUL terminator
  EXPECT_TRUE(!pvpValidNick(nick));
  memset(nick, 0, sizeof(nick)); nick[0] = 'A'; nick[5] = 'B';       // junk after NUL
  EXPECT_TRUE(!pvpValidNick(nick));

  uint8_t buf[PVP_MAX_PACKET];
  PvpMessage m = sampleMsg(PVP_MSG_HELLO);
  size_t n = pvpSerialize(m, buf, sizeof(buf));
  buf[PVP_HEADER_SIZE + 4] = 'a';  // lowercase letter in the nick, valid CRC
  pvpSealCrc(buf, n);
  PvpMessage out;
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_VALUE);

  m = sampleMsg(PVP_MSG_ACTION);
  n = pvpSerialize(m, buf, sizeof(buf));
  buf[PVP_HEADER_SIZE + 2] = PVP_ACTION_COUNT;
  pvpSealCrc(buf, n);
  EXPECT_EQ((int)pvpParse(buf, n, out), (int)PVP_PARSE_BAD_VALUE);
}

static void testLuckIsDeterministicAndBounded() {
  int hist[PVP_LUCK_MOD] = {0};
  for (uint16_t r = 0; r < 2000; r++) {
    uint8_t a = pvpLuck(11, 22, r);
    EXPECT_TRUE(a < PVP_LUCK_MOD);
    EXPECT_EQ(a, pvpLuck(11, 22, r));
    hist[a]++;
  }
  int distinct = 0;
  for (int h : hist) distinct += h > 0;
  EXPECT_TRUE(distinct > 90);
  EXPECT_TRUE(pvpLuck(11, 22, 3) != pvpLuck(22, 11, 3) || pvpLuck(11, 22, 4) != pvpLuck(22, 11, 4));
}

// --------------------------------------------------------- full sessions

static bool finished(TestLink &l) { return l.bothFinished(); }

static void expectCleanBattle(TestLink &l, size_t rounds) {
  EXPECT_TRUE(l.d[0].s.resultCounts());
  EXPECT_TRUE(l.d[1].s.resultCounts());
  EXPECT_EQ(l.d[0].count(PVP_EVT_FINISHED), 1);
  EXPECT_EQ(l.d[1].count(PVP_EVT_FINISHED), 1);
  EXPECT_EQ(l.d[0].count(PVP_EVT_ABORTED), 0);
  EXPECT_EQ(l.d[1].count(PVP_EVT_ABORTED), 0);
  EXPECT_EQ(l.d[0].rounds.size(), rounds);  // each round processed exactly once
  EXPECT_EQ(l.d[1].rounds.size(), rounds);
  if (l.d[0].rounds.size() != rounds || l.d[1].rounds.size() != rounds) return;
  for (size_t r = 0; r < rounds; r++) {
    const RoundRecord &a = l.d[0].rounds[r];
    const RoundRecord &b = l.d[1].rounds[r];
    EXPECT_EQ(a.round, (uint16_t)r);
    EXPECT_EQ(a.round, b.round);
    EXPECT_EQ(a.luck, b.luck);
    EXPECT_EQ(a.actionA, b.actionA);
    EXPECT_EQ(a.actionB, b.actionB);
    // side A is the lower deviceId (device 0)
    EXPECT_EQ(a.actionA, testAction(l.d[0].id, (uint16_t)r));
    EXPECT_EQ(a.actionB, testAction(l.d[1].id, (uint16_t)r));
  }
}

static void testFullSessionLossless() {
  TestLink l(0x1111, 0x2222);
  l.d[0].s.startDiscovery(l.now);
  l.d[1].s.startDiscovery(l.now);
  EXPECT_TRUE(l.runUntil(30000, finished));
  EXPECT_TRUE(l.d[0].s.isSideA());
  EXPECT_TRUE(!l.d[1].s.isSideA());
  EXPECT_EQ(l.d[0].s.sessionId(), l.d[1].s.sessionId());
  EXPECT_EQ(l.d[0].count(PVP_EVT_PEER_FOUND), 1);
  EXPECT_EQ(l.d[1].count(PVP_EVT_CHALLENGE_RECEIVED), 1);
  EXPECT_EQ(l.d[0].count(PVP_EVT_ACCEPTED), 1);
  EXPECT_EQ(l.d[1].count(PVP_EVT_ACCEPTED), 1);
  EXPECT_EQ(l.d[0].count(PVP_EVT_OPPONENT_READY), 1);
  EXPECT_EQ(l.d[0].find(PVP_EVT_OPPONENT_READY)->ready.dex, (uint16_t)26);
  EXPECT_EQ(l.d[1].find(PVP_EVT_OPPONENT_READY)->ready.dex, (uint16_t)25);
  expectCleanBattle(l, 4);
}

static void testOrientationWhenInitiatorIsHigherId() {
  TestLink l(0x1111, 0x2222);
  l.d[0].initiator = false;
  l.d[1].initiator = true;  // higher id challenges
  l.d[0].s.startDiscovery(l.now);
  l.d[1].s.startDiscovery(l.now);
  EXPECT_TRUE(l.runUntil(30000, finished));
  expectCleanBattle(l, 4);
  EXPECT_TRUE(l.d[0].s.isSideA());
}

static void testLossyDuplicatingReorderingLink() {
  int completed = 0;
  for (uint32_t seed = 1; seed <= 25; seed++) {
    TestLink l(0x1111, 0x2222);
    l.rng.s = seed * 7919u;
    l.lossPct = 30;
    l.dupPct = 25;
    l.jitterMs = 120;  // jitter above the retransmit interval reorders packets
    l.d[0].lastRound = l.d[1].lastRound = 5;
    l.d[0].s.startDiscovery(l.now);
    l.d[1].s.startDiscovery(l.now);
    bool ok = l.runUntil(120000, finished);
    EXPECT_TRUE(ok);
    if (!ok) continue;
    completed++;
    expectCleanBattle(l, 6);
  }
  EXPECT_EQ(completed, 25);
}

static void testSimultaneousChallengeBothWays() {
  TestLink l(0x1111, 0x2222);
  l.d[1].initiator = true;  // both challenge at once
  l.baseMs = 40;            // challenges cross on the wire
  l.d[0].s.startDiscovery(l.now);
  l.d[1].s.startDiscovery(l.now);
  EXPECT_TRUE(l.runUntil(30000, finished));
  EXPECT_EQ(l.d[0].count(PVP_EVT_ABORTED), 0);
  EXPECT_EQ(l.d[1].count(PVP_EVT_ABORTED), 0);
  EXPECT_EQ(l.d[0].s.sessionId(), (uint32_t)(0x1111 * 1000 + 1));  // lower id's session wins
  EXPECT_EQ(l.d[1].s.sessionId(), (uint32_t)(0x1111 * 1000 + 1));
  expectCleanBattle(l, 4);
}

static bool bothAborted(TestLink &l) {
  return l.d[0].s.state() == PVP_STATE_ABORTED && l.d[1].s.state() == PVP_STATE_ABORTED;
}

static void testWrongHashEndsInDesync() {
  TestLink l(0x1111, 0x2222);
  l.d[0].badHashRound = l.d[1].badHashRound = 1;
  l.d[0].s.startDiscovery(l.now);
  l.d[1].s.startDiscovery(l.now);
  EXPECT_TRUE(l.runUntil(30000, bothAborted));
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(l.d[i].count(PVP_EVT_DESYNC), 1);
    EXPECT_EQ(l.d[i].count(PVP_EVT_FINISHED), 0);
    EXPECT_TRUE(!l.d[i].s.resultCounts());
    const PvpEvent *a = l.d[i].find(PVP_EVT_ABORTED);
    EXPECT_TRUE(a && a->reason == PVP_REASON_DESYNC);
  }
}

static void testDeclineReturnsToDiscovery() {
  TestLink l(0x1111, 0x2222);
  l.d[1].autoAccept = false;
  l.d[0].s.startDiscovery(l.now);
  l.d[1].s.startDiscovery(l.now);
  l.step(600);
  EXPECT_EQ(l.d[1].count(PVP_EVT_CHALLENGE_RECEIVED), 1);
  EXPECT_TRUE(l.d[1].s.decline(l.now));
  l.step(300);
  EXPECT_EQ(l.d[0].count(PVP_EVT_DECLINED), 1);
  EXPECT_EQ((int)l.d[0].s.state(), (int)PVP_STATE_DISCOVERY);
  EXPECT_EQ((int)l.d[1].s.state(), (int)PVP_STATE_DISCOVERY);
}

// --------------------------------------------- scripted single-session tests

static void testActionForWrongOrFutureRound() {
  ScriptedPeer sp;
  sp.toBattle();
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_BATTLE);
  EXPECT_EQ(sp.count(PVP_EVT_BATTLE_START), 1);
  size_t mark = sp.sent.size();

  PvpMessage far = sp.msg(PVP_MSG_ACTION, false);
  far.seq = 90;
  far.round = 5;  // two or more rounds ahead: ignored, not acknowledged
  far.action = 1;
  sp.inject(far);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK, mark), 0);

  uint16_t seqNext = sp.peerSeq++;
  PvpMessage next = sp.msg(PVP_MSG_ACTION, false);
  next.seq = seqNext;
  next.round = 1;  // one ahead: buffered and acknowledged
  next.action = 3;
  sp.inject(next);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK, mark), 1);
  sp.s.submitAction(2, sp.now);
  sp.pump();
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 0);  // buffered action must not complete round 0

  PvpMessage cur = sp.msg(PVP_MSG_ACTION, true);
  cur.round = 0;
  cur.action = 4;
  sp.inject(cur);
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 1);
  EXPECT_EQ(sp.log.back().actionA, 4);  // peer is side A
  EXPECT_EQ(sp.log.back().actionB, 2);

  // Confirm round 0; the buffered round-1 action is then used immediately.
  sp.s.submitRoundHash(0x55, false, sp.now);
  PvpMessage h = sp.msg(PVP_MSG_ROUND_HASH, true);
  h.round = 0; h.hash = 0x55;
  sp.inject(h);
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_CONFIRMED), 1);
  sp.s.submitAction(1, sp.now);
  sp.pump();
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 2);
  EXPECT_EQ(sp.log.back().round, 1);
  EXPECT_EQ(sp.log.back().actionA, 3);

  // A stale action for the finished round 0 (fresh seq) is rejected without ACK.
  mark = sp.sent.size();
  PvpMessage stale = sp.msg(PVP_MSG_ACTION, true);
  stale.round = 0; stale.action = 0;
  sp.inject(stale);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK, mark), 0);
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 2);
}

static void testDuplicateActionAckedButProcessedOnce() {
  ScriptedPeer sp;
  sp.toBattle();
  sp.s.submitAction(1, sp.now);
  size_t mark = sp.sent.size();
  PvpMessage a = sp.msg(PVP_MSG_ACTION, true);
  a.round = 0; a.action = 2;
  for (int i = 0; i < 4; i++) sp.inject(a);  // original plus three duplicates
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 1);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK, mark), 4);  // every copy is acknowledged again
}

static void testRetransmitScheduleAndAck() {
  ScriptedPeer sp;
  sp.toBattle();
  sp.pump();
  size_t mark = sp.sent.size();
  sp.s.submitAction(1, sp.now);
  sp.pump();
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACTION, mark), 1);
  sp.advance(1400);  // first send plus 8 retransmits at 150 ms, then slow
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACTION, mark), 9);
  sp.advance(1000);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACTION, mark), 10);  // slow phase keeps trying
  // ACK for the right seq stops it (our ACTION went out with the next free seq).
  PvpMessage last;
  for (size_t i = sp.sent.size(); i-- > 0;) {
    if (pvpParse(sp.sent[i].data, sp.sent[i].len, last) == PVP_PARSE_OK && last.type == PVP_MSG_ACTION) break;
  }
  PvpMessage keep = sp.msg(PVP_MSG_HELLO, false);  // keep the link alive for the timeout rule
  keep.deviceId = ScriptedPeer::PEER_ID; strcpy(keep.nick, "ASH");
  sp.inject(keep);
  sp.ack(last.seq);
  size_t after = sp.sent.size();
  sp.advance(3000);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACTION, after), 0);
}

static void testLostAckPeerRetransmitsAndIsReacked() {
  ScriptedPeer sp;
  sp.toBattle();
  PvpMessage a = sp.msg(PVP_MSG_ACTION, true);
  a.round = 0; a.action = 2;
  sp.inject(a);
  size_t mark = sp.sent.size();
  sp.advance(150);
  sp.inject(a);  // the peer never saw our ACK and retransmits
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK, mark), 1);
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_BATTLE);
}

static void testSilenceTimeoutAbortsWithoutPoints() {
  ScriptedPeer sp;
  sp.toBattle();
  uint32_t start = sp.now;
  sp.advance(PVP_PEER_TIMEOUT_MS - 20);  // still inside the window
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_BATTLE);
  EXPECT_TRUE(sp.now - start < PVP_PEER_TIMEOUT_MS);
  sp.advance(30);
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_ABORTED);
  EXPECT_TRUE(!sp.s.resultCounts());
  EXPECT_EQ(sp.count(PVP_EVT_FINISHED), 0);
  EXPECT_EQ(sp.log.back().type, PVP_EVT_ABORTED);
  EXPECT_EQ(sp.log.back().reason, PVP_REASON_TIMEOUT);
}

static void testKeepaliveExtendsSilenceWindow() {
  ScriptedPeer sp;
  sp.toBattle();
  sp.advance(8000);
  PvpMessage keep = sp.msg(PVP_MSG_HELLO, false);
  keep.deviceId = ScriptedPeer::PEER_ID; strcpy(keep.nick, "ASH");
  sp.inject(keep);
  sp.advance(8000);  // 16 s after the start, 8 s after the keepalive
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_BATTLE);
  EXPECT_TRUE(sp.sentCount(PVP_MSG_HELLO) > 0);  // we keep the peer alive too
}

static void testActionTimeoutEventAndDefault() {
  ScriptedPeer sp;
  sp.toBattle();
  // Keep the peer alive; do not submit our action.
  for (uint32_t t = 0; t + 1000 < PVP_ACTION_TIMEOUT_MS; t += 1000) {
    sp.advance(1000);
    PvpMessage keep = sp.msg(PVP_MSG_HELLO, false);
    keep.deviceId = ScriptedPeer::PEER_ID; strcpy(keep.nick, "ASH");
    sp.inject(keep);
  }
  EXPECT_EQ(sp.count(PVP_EVT_ACTION_TIMEOUT), 0);
  sp.advance(1100);
  EXPECT_EQ(sp.count(PVP_EVT_ACTION_TIMEOUT), 1);
  sp.advance(100);
  EXPECT_EQ(sp.count(PVP_EVT_ACTION_TIMEOUT), 1);  // reported once per round
  size_t mark = sp.sent.size();
  EXPECT_TRUE(sp.s.submitDefaultAction(sp.now));
  sp.pump();
  PvpMessage m;
  bool found = false;
  for (size_t i = mark; i < sp.sent.size(); i++) {
    if (pvpParse(sp.sent[i].data, sp.sent[i].len, m) == PVP_PARSE_OK && m.type == PVP_MSG_ACTION) found = true;
  }
  EXPECT_TRUE(found && m.action == PVP_DEFAULT_ACTION);
  EXPECT_TRUE(!sp.s.submitAction(1, sp.now));  // already submitted this round
}

static void testForeignSessionAndSpoofIgnored() {
  ScriptedPeer sp;
  sp.toBattle();
  size_t mark = sp.sent.size();
  PvpMessage a = sp.msg(PVP_MSG_ACTION, true);
  a.round = 0; a.action = 2;

  PvpMessage foreign = a;
  foreign.session = ScriptedPeer::SESSION + 1;
  sp.inject(foreign);
  uint8_t other[6] = { 0xEE, 0, 0, 0, 0, 9 };
  sp.inject(a, other);  // right session, wrong source MAC
  PvpMessage abortMsg = sp.msg(PVP_MSG_ABORT, false);
  abortMsg.reason = PVP_REASON_USER;
  sp.inject(abortMsg, other);  // spoofed abort
  abortMsg.session = ScriptedPeer::SESSION + 5;
  sp.inject(abortMsg);         // abort of someone else's session
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK, mark), 0);
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_BATTLE);
  sp.s.submitAction(1, sp.now);
  sp.pump();
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 0);  // none of that counted as the peer's action

  sp.inject(a);  // the genuine one still works
  EXPECT_EQ(sp.count(PVP_EVT_ROUND_READY), 1);
}

static void testPacketsOutsideSessionAreIgnored() {
  ScriptedPeer sp;
  sp.s.startDiscovery(sp.now);
  PvpMessage a = sp.msg(PVP_MSG_ACTION, true);
  a.round = 0;
  sp.inject(a);
  PvpMessage hs = sp.msg(PVP_MSG_ACCEPT, true);
  hs.deviceId = ScriptedPeer::PEER_ID;
  sp.inject(hs);
  PvpMessage ab = sp.msg(PVP_MSG_ABORT, false);
  sp.inject(ab);
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_DISCOVERY);
  EXPECT_EQ(sp.log.size(), (size_t)0);
  EXPECT_EQ(sp.sentCount(PVP_MSG_ACK), 0);
}

static void testPeerExpiresAfterThreeSeconds() {
  ScriptedPeer sp;
  sp.s.startDiscovery(sp.now);
  sp.hello();
  EXPECT_EQ(sp.count(PVP_EVT_PEER_FOUND), 1);
  sp.advance(PVP_PEER_EXPIRE_MS - 10);
  EXPECT_EQ(sp.count(PVP_EVT_PEER_LOST), 0);
  sp.hello();  // refresh
  sp.advance(PVP_PEER_EXPIRE_MS - 10);
  EXPECT_EQ(sp.count(PVP_EVT_PEER_LOST), 0);
  sp.advance(20);
  EXPECT_EQ(sp.count(PVP_EVT_PEER_LOST), 1);
  EXPECT_EQ(sp.s.peers().count(), 0);
}

static void testPeerListCapsAtEight() {
  ScriptedPeer sp;
  sp.s.startDiscovery(sp.now);
  for (uint32_t i = 0; i < 10; i++) {
    PvpMessage m;
    m.type = PVP_MSG_HELLO;
    m.deviceId = 100 + i;
    strcpy(m.nick, "BOT");
    uint8_t mac[6] = { 1, 1, 1, 1, 1, (uint8_t)i };
    sp.inject(m, mac);
  }
  EXPECT_EQ(sp.s.peers().count(), (int)PVP_MAX_PEERS);
  EXPECT_EQ(sp.count(PVP_EVT_PEER_FOUND), (int)PVP_MAX_PEERS);
}

static void testVersionMismatch() {
  // Foreign CHALLENGE with another protocol version while discovering.
  ScriptedPeer sp;
  sp.s.startDiscovery(sp.now);
  PvpMessage c = sp.msg(PVP_MSG_CHALLENGE, true);
  c.deviceId = ScriptedPeer::PEER_ID;
  uint8_t buf[PVP_MAX_PACKET];
  size_t n = pvpSerialize(c, buf, sizeof(buf));
  buf[2] = PVP_PROTO_VERSION + 1;
  pvpSealCrc(buf, n);
  sp.injectRaw(buf, n);
  EXPECT_EQ(sp.count(PVP_EVT_VERSION_MISMATCH), 1);
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_DISCOVERY);
  PvpMessage reply;
  bool got = false;
  for (const PvpOutPacket &p : sp.sent) {
    if (pvpParse(p.data, p.len, reply) == PVP_PARSE_OK && reply.type == PVP_MSG_ABORT) got = true;
  }
  EXPECT_TRUE(got && reply.reason == PVP_REASON_VERSION && reply.session == ScriptedPeer::SESSION);

  // The challenger side: the peer answers with ABORT(VERSION).
  ScriptedPeer ch;
  ch.s.startDiscovery(ch.now);
  ch.hello();
  ch.s.challenge(ScriptedPeer::PEER_ID, ScriptedPeer::SESSION, 1, ch.now);
  ch.pump();
  PvpMessage ab = ch.msg(PVP_MSG_ABORT, false);
  ab.reason = PVP_REASON_VERSION;
  ch.inject(ab);
  EXPECT_EQ(ch.count(PVP_EVT_VERSION_MISMATCH), 1);
  EXPECT_EQ((int)ch.s.state(), (int)PVP_STATE_ABORTED);

  // Same, but the peer's own packet carries a different proto byte.
  ScriptedPeer ch2;
  ch2.s.startDiscovery(ch2.now);
  ch2.hello();
  ch2.s.challenge(ScriptedPeer::PEER_ID, ScriptedPeer::SESSION, 1, ch2.now);
  ch2.pump();
  ab = ch2.msg(PVP_MSG_ABORT, false);
  ab.reason = PVP_REASON_VERSION;
  n = pvpSerialize(ab, buf, sizeof(buf));
  buf[2] = 9;
  pvpSealCrc(buf, n);
  ch2.injectRaw(buf, n);
  EXPECT_EQ(ch2.count(PVP_EVT_VERSION_MISMATCH), 1);
  EXPECT_EQ((int)ch2.s.state(), (int)PVP_STATE_ABORTED);
}

static void testBitFlippedPacketsNeverChangeState() {
  ScriptedPeer sp;
  sp.toBattle();
  sp.advance(100);
  size_t events = sp.log.size();
  size_t sentBefore = sp.sent.size();
  const uint8_t types[] = { PVP_MSG_ACTION, PVP_MSG_ROUND_HASH, PVP_MSG_ABORT, PVP_MSG_READY };
  for (uint8_t type : types) {
    PvpMessage m = sp.msg(type, true);
    m.round = 0; m.action = 1; m.hash = 5; m.reason = PVP_REASON_USER; m.ready = testReady();
    uint8_t buf[PVP_MAX_PACKET];
    size_t n = pvpSerialize(m, buf, sizeof(buf));
    for (size_t bit = 0; bit < n * 8; bit++) {
      uint8_t bad[PVP_MAX_PACKET];
      memcpy(bad, buf, n);
      bad[bit / 8] ^= (uint8_t)(1u << (bit % 8));
      sp.injectRaw(bad, n);
    }
    sp.injectRaw(buf, n > 3 ? n - 3 : 0);
  }
  TestRng rng{99};
  for (int i = 0; i < 3000; i++) {
    uint8_t junk[40];
    size_t n = rng.next() % sizeof(junk);
    for (size_t k = 0; k < n; k++) junk[k] = (uint8_t)rng.next();
    sp.injectRaw(junk, n);
  }
  EXPECT_EQ(sp.log.size(), events);
  EXPECT_EQ(sp.sent.size(), sentBefore);  // not even an ACK
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_BATTLE);
}

static void testAckAfterFinishLetsPeerComplete() {
  // Our final hash must keep being retransmitted after we reached FINISHED.
  ScriptedPeer sp;
  sp.toBattle();
  sp.s.submitAction(1, sp.now);
  PvpMessage a = sp.msg(PVP_MSG_ACTION, true);
  a.round = 0; a.action = 2;
  sp.inject(a);
  sp.s.submitRoundHash(0x77, true, sp.now);
  PvpMessage h = sp.msg(PVP_MSG_ROUND_HASH, true);
  h.round = 0; h.hash = 0x77; h.finalRound = 1;
  sp.inject(h);
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_FINISHED);
  EXPECT_EQ(sp.count(PVP_EVT_FINISHED), 1);
  EXPECT_TRUE(sp.s.resultCounts());
  size_t mark = sp.sent.size();
  sp.advance(400);
  EXPECT_TRUE(sp.sentCount(PVP_MSG_ROUND_HASH, mark) >= 2);
  sp.inject(h);  // duplicate of the peer's hash still gets an ACK
  EXPECT_EQ((int)sp.s.state(), (int)PVP_STATE_FINISHED);
  EXPECT_EQ(sp.count(PVP_EVT_FINISHED), 1);
  EXPECT_EQ(sp.count(PVP_EVT_ABORTED), 0);
}

int main() {
  testCrcVector();
  testRoundTripAllTypes();
  testWireIsLittleEndian();
  testParseRejectsBadFraming();
  testParseRejectsBadTypeAndLen();
  testParseVersionMismatchKeepsHeader();
  testValueRanges();
  testNickAndActionValidation();
  testLuckIsDeterministicAndBounded();
  testFullSessionLossless();
  testOrientationWhenInitiatorIsHigherId();
  testLossyDuplicatingReorderingLink();
  testSimultaneousChallengeBothWays();
  testWrongHashEndsInDesync();
  testDeclineReturnsToDiscovery();
  testActionForWrongOrFutureRound();
  testDuplicateActionAckedButProcessedOnce();
  testRetransmitScheduleAndAck();
  testLostAckPeerRetransmitsAndIsReacked();
  testSilenceTimeoutAbortsWithoutPoints();
  testKeepaliveExtendsSilenceWindow();
  testActionTimeoutEventAndDefault();
  testForeignSessionAndSpoofIgnored();
  testPacketsOutsideSessionAreIgnored();
  testPeerExpiresAfterThreeSeconds();
  testPeerListCapsAtEight();
  testVersionMismatch();
  testBitFlippedPacketsNeverChangeState();
  testAckAfterFinishLetsPeerComplete();
  if (failures) {
    std::cerr << failures << " Fehler\n";
    return 1;
  }
  std::cout << "pvp_tests: alle bestanden\n";
  return 0;
}
