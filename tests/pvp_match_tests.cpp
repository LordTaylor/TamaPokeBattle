// PvpMatch: the lockstep wrapper must look identical (mirrored) from both sides.
#include "battle_core.h"
#include "pvp_match.h"
#include "pvp_test_harness.h"

static PvpReadyPayload mon(uint16_t dex, uint8_t level, uint16_t atk, uint16_t def, uint16_t spe, uint8_t t1) {
  PvpReadyPayload r;
  r.dex = dex; r.level = level; r.atk = atk; r.def = def; r.spe = spe; r.type1 = t1;
  return r;
}

static uint8_t actionFor(int side, int round) { return (uint8_t)((side * 2 + round * 3) % PVP_ACTION_COUNT); }
static uint8_t luckFor(int round) { return (uint8_t)((round * 37 + 11) % PVP_LUCK_MOD); }

static void testBothSidesSeeTheSameBattle() {
  PvpReadyPayload a = mon(6, 40, 90, 70, 100, 2);   // side A's Pokemon
  PvpReadyPayload b = mon(9, 38, 80, 95, 70, 3);
  PvpMatch ma, mb;
  EXPECT_TRUE(ma.begin(a, b, true));    // A: local = a
  EXPECT_TRUE(mb.begin(b, a, false));   // B: local = b, opponent = a
  for (int round = 0; round < 25 && ma.outcome() == PVP_VIEW_ONGOING; round++) {
    uint32_t ha = 0, hb = 0;
    uint8_t x = actionFor(0, round), y = actionFor(1, round);
    EXPECT_TRUE(ma.applyRound((uint16_t)round, x, y, luckFor(round), ha));
    EXPECT_TRUE(mb.applyRound((uint16_t)round, x, y, luckFor(round), hb));
    EXPECT_EQ(ha, hb);
    // mirrored views
    EXPECT_EQ(ma.lastTurn().mine.damageDealt, mb.lastTurn().theirs.damageDealt);
    EXPECT_EQ(ma.lastTurn().theirs.damageDealt, mb.lastTurn().mine.damageDealt);
    EXPECT_EQ(ma.runtime().side[0].hp, mb.runtime().side[0].hp);
  }
  EXPECT_TRUE(ma.outcome() != PVP_VIEW_ONGOING);
  bool mirrored = (ma.outcome() == PVP_VIEW_WIN && mb.outcome() == PVP_VIEW_LOSS) ||
                  (ma.outcome() == PVP_VIEW_LOSS && mb.outcome() == PVP_VIEW_WIN) ||
                  (ma.outcome() == PVP_VIEW_DRAW && mb.outcome() == PVP_VIEW_DRAW);
  EXPECT_TRUE(mirrored);
  EXPECT_EQ(ma.side(), PVP_SIDE_A);
  EXPECT_EQ(mb.side(), PVP_SIDE_B);
}

static void testBeginZeroesHpAndEngineDerivesMax() {
  PvpReadyPayload a = mon(6, 40, 90, 70, 100, 2);
  PvpReadyPayload b = mon(9, 38, 80, 95, 70, 3);
  PvpReadyPayload cheat = b;
  cheat.hp = 999;
  PvpMatch honest, cheating;
  EXPECT_TRUE(honest.begin(a, b, true));
  EXPECT_TRUE(cheating.begin(a, cheat, true));
  EXPECT_EQ(cheating.runtime().side[1].stats.hp, 0);
  EXPECT_EQ(cheating.runtime().side[1].maxHp, honest.runtime().side[1].maxHp);
  EXPECT_EQ(cheating.runtime().side[1].maxHp, battle_core::turnMaxHp(honest.runtime().side[1].stats));
  EXPECT_EQ(pvpStateHash(cheating.runtime()), pvpStateHash(honest.runtime()));
}

static void testRejectsBadInputWithoutChangingState() {
  PvpMatch m;
  uint32_t h = 0;
  EXPECT_TRUE(!m.applyRound(0, 0, 0, 0, h));  // not begun
  PvpReadyPayload bad = mon(6, 0, 90, 70, 100, 2);  // level 0
  EXPECT_TRUE(!m.begin(bad, mon(9, 38, 80, 95, 70, 3), true));
  EXPECT_TRUE(!m.ready());

  EXPECT_TRUE(m.begin(mon(6, 40, 90, 70, 100, 2), mon(9, 38, 80, 95, 70, 3), true));
  uint32_t before = pvpStateHash(m.runtime());
  EXPECT_TRUE(!m.applyRound(0, PVP_ACTION_COUNT, 0, 10, h));   // action out of range
  EXPECT_TRUE(!m.applyRound(0, 0, 0, PVP_LUCK_MOD, h));        // luck out of range
  EXPECT_TRUE(!m.applyRound(3, 0, 0, 10, h));                  // wrong round
  EXPECT_EQ(pvpStateHash(m.runtime()), before);
  EXPECT_TRUE(m.applyRound(0, 0, 0, 10, h));
}

int main() {
  testBothSidesSeeTheSameBattle();
  testBeginZeroesHpAndEngineDerivesMax();
  testRejectsBadInputWithoutChangingState();
  if (failures) {
    std::cerr << failures << " Fehler\n";
    return 1;
  }
  std::cout << "pvp_match_tests: OK\n";
  return 0;
}
