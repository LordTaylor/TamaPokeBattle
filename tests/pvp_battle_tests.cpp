#include <cstdlib>
#include <iostream>

#include "battle_core.h"
#include "pvp_config.h"
#include "trainer.h"
#include "dex.h"
#include "pvp_battle.h"

static int failures = 0;

#define EXPECT_TRUE(expr) do { \
  if (!(expr)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #expr << "\n"; \
    failures++; \
  } \
} while (0)

#define EXPECT_EQ(actual, expected) do { \
  auto a = (actual); \
  auto e = (expected); \
  if (!(a == e)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #actual \
              << " war " << +a << ", erwartet " << +e << "\n"; \
    failures++; \
  } \
} while (0)

static const BattleAction ALL_ACTIONS[5] = {
  BATTLE_ATTACK, BATTLE_ATTACK_QUICK, BATTLE_ATTACK_HEAVY, BATTLE_DODGE, BATTLE_REST,
};
static const uint8_t LUCKS[6] = { 0, 17, 39, 50, 84, 99 };

static BattleStats statsOf(uint16_t atk, uint16_t def, uint16_t spe, uint8_t level,
                           uint8_t t1 = TYPE_NONE, uint8_t t2 = TYPE_NONE) {
  BattleStats s = {};
  s.atk = atk;
  s.def = def;
  s.spe = spe;
  s.level = level;
  s.type1 = t1;
  s.type2 = t2;
  return s;
}

static PvpRuntime begin(const BattleStats &a, const BattleStats &b) {
  PvpRuntime r;
  PvpError err = pvpBegin(r, a, b);
  EXPECT_EQ(err, PVP_OK);
  return r;
}


static uint16_t restHealExpected(uint16_t maxHp, uint16_t hp) {
  uint16_t heal = (uint16_t)((uint32_t)maxHp * PVP_REST_HEAL_PCT / 100);
  if (heal < PVP_REST_HEAL_MIN) heal = PVP_REST_HEAL_MIN;
  uint16_t missing = maxHp - hp;
  return heal > missing ? missing : heal;
}

static bool sameSide(const PvpSide &x, const PvpSide &y) {
  return x.hp == y.hp && x.maxHp == y.maxHp && x.dodgeHeat == y.dodgeHeat && x.damageTotal == y.damageTotal &&
         x.restUsesLeft == y.restUsesLeft && x.counterReady == y.counterReady;
}

static bool sameState(const PvpRuntime &x, const PvpRuntime &y) {
  return x.round == y.round && sameSide(x.side[0], y.side[0]) && sameSide(x.side[1], y.side[1]);
}

// Plays up to N turns with a deterministic luck sequence derived from the turn index.
static PvpRuntime playScripted(const BattleStats &a, const BattleStats &b, BattleAction actA,
                               BattleAction actB, uint8_t luckBase, int turns) {
  PvpRuntime battle = begin(a, b);
  for (int i = 0; i < turns; i++) pvpStep(battle, actA, actB, (uint8_t)((luckBase + i * 37) % 100));
  return battle;
}

static void testInvariantsAllActionPairs() {
  BattleStats a = statsOf(70, 50, 60, 20, TYPE_FIRE);
  BattleStats b = statsOf(65, 55, 45, 19, TYPE_GRASS, TYPE_POISON);
  for (BattleAction actA : ALL_ACTIONS) {
    for (BattleAction actB : ALL_ACTIONS) {
      for (uint8_t luck : LUCKS) {
        PvpRuntime one = playScripted(a, b, actA, actB, luck, 25);
        PvpRuntime two = playScripted(a, b, actA, actB, luck, 25);
        EXPECT_TRUE(sameState(one, two));
        EXPECT_EQ(pvpStateHash(one), pvpStateHash(two));
        EXPECT_TRUE(one.round <= battle_core::MAX_TURN_ROUNDS);
        EXPECT_TRUE(pvpOutcome(one) != PVP_OUTCOME_ONGOING);
      }
    }
  }
}

static void expectMirrored(const PvpRuntime &x, const PvpRuntime &y) {
  EXPECT_EQ(x.round, y.round);
  EXPECT_TRUE(sameSide(x.side[0], y.side[1]));
  EXPECT_TRUE(sameSide(x.side[1], y.side[0]));
}

static void testSymmetryWhenSidesAreSwapped() {
  BattleStats a = statsOf(90, 40, 80, 25, TYPE_WATER);
  BattleStats b = statsOf(60, 70, 30, 22, TYPE_ELECTRIC, TYPE_FLYING);
  for (BattleAction actA : ALL_ACTIONS) {
    for (BattleAction actB : ALL_ACTIONS) {
      for (uint8_t luck : LUCKS) {
        PvpRuntime x = begin(a, b);
        PvpRuntime y = begin(b, a);
        for (int i = 0; i < 12; i++) {
          uint8_t roll = (uint8_t)((luck + i * 23) % 100);
          PvpTurnResult tx = pvpStep(x, actA, actB, roll);
          PvpTurnResult ty = pvpStep(y, actB, actA, (uint8_t)(99 - roll));
          expectMirrored(x, y);
          EXPECT_EQ(tx.side[0].damageDealt, ty.side[1].damageDealt);
          EXPECT_EQ(tx.side[1].damageDealt, ty.side[0].damageDealt);
          EXPECT_EQ(tx.side[0].evaded, ty.side[1].evaded);
        }
        PvpOutcome ox = pvpOutcome(x);
        PvpOutcome oy = pvpOutcome(y);
        EXPECT_EQ(pvpViewOutcome(ox, PVP_SIDE_A), pvpViewOutcome(oy, PVP_SIDE_B));
        EXPECT_EQ(pvpViewOutcome(ox, PVP_SIDE_B), pvpViewOutcome(oy, PVP_SIDE_A));
      }
    }
  }
}

static void testKnockoutDecidesWinner() {
  PvpRuntime battle = begin(statsOf(200, 20, 10, 50), statsOf(10, 20, 10, 5));
  battle.side[1].hp = 3;
  PvpTurnResult turn = pvpStep(battle, BATTLE_ATTACK, BATTLE_ATTACK_QUICK, 60);
  EXPECT_EQ(battle.side[1].hp, 0);
  EXPECT_TRUE(turn.battleEnded);
  EXPECT_EQ(turn.outcome, PVP_OUTCOME_A_WINS);
  EXPECT_EQ(turn.side[0].damageDealt, 3);  // dealt damage is capped to remaining hp
  EXPECT_EQ(battle.side[0].damageTotal, 3);
}

static void testSimultaneousKnockoutIsDraw() {
  PvpRuntime battle = begin(statsOf(100, 20, 1, 30), statsOf(100, 20, 1, 30));
  battle.side[0].hp = 1;
  battle.side[1].hp = 1;
  PvpTurnResult turn = pvpStep(battle, BATTLE_ATTACK, BATTLE_ATTACK, 50);
  EXPECT_EQ(battle.side[0].hp, 0);
  EXPECT_EQ(battle.side[1].hp, 0);
  EXPECT_EQ(turn.outcome, PVP_OUTCOME_DRAW);
}

static void testRestingSideCanStillDieToSimultaneousHit() {
  PvpRuntime battle = begin(statsOf(100, 20, 1, 30), statsOf(100, 20, 1, 30));
  battle.side[0].hp = 1;
  PvpTurnResult turn = pvpStep(battle, BATTLE_REST, BATTLE_ATTACK, 50);
  EXPECT_TRUE(turn.side[0].healed > 0);
  EXPECT_TRUE(battle.side[0].hp <= battle.side[0].maxHp);
}

static PvpRuntime atRoundLimit(uint16_t hpA, uint16_t maxA, uint16_t hpB, uint16_t maxB) {
  PvpRuntime battle = begin(statsOf(50, 50, 50, 20), statsOf(50, 50, 50, 20));
  battle.round = battle_core::MAX_TURN_ROUNDS;
  battle.side[0].hp = hpA;
  battle.side[0].maxHp = maxA;
  battle.side[1].hp = hpB;
  battle.side[1].maxHp = maxB;
  return battle;
}

static void testRoundLimitComparesHpPercentCrosswise() {
  EXPECT_EQ(pvpOutcome(atRoundLimit(51, 100, 30, 60)), PVP_OUTCOME_A_WINS);
  EXPECT_EQ(pvpOutcome(atRoundLimit(49, 100, 30, 60)), PVP_OUTCOME_B_WINS);
  EXPECT_EQ(pvpOutcome(atRoundLimit(50, 100, 30, 60)), PVP_OUTCOME_DRAW);
  EXPECT_EQ(pvpOutcome(atRoundLimit(40, 100, 25, 50)), PVP_OUTCOME_B_WINS);   // 40% < 50%
  EXPECT_EQ(pvpOutcome(atRoundLimit(60, 150, 40, 100)), PVP_OUTCOME_DRAW);    // 40% == 40%
  EXPECT_EQ(pvpOutcome(atRoundLimit(65535, 65535, 65534, 65535)), PVP_OUTCOME_A_WINS);  // no overflow
  EXPECT_EQ(pvpOutcome(atRoundLimit(1, 100, 0, 100)), PVP_OUTCOME_A_WINS);   // KO beats percent
}

static void testBattleEndsAtRoundLimit() {
  PvpRuntime battle = begin(statsOf(1, 200, 50, 5), statsOf(1, 200, 50, 5));
  PvpTurnResult turn = {};
  for (int i = 0; i < 40 && !turn.battleEnded; i++) turn = pvpStep(battle, BATTLE_DODGE, BATTLE_DODGE, 10);
  EXPECT_TRUE(turn.battleEnded);
  EXPECT_EQ(battle.round, battle_core::MAX_TURN_ROUNDS);
  EXPECT_EQ(turn.outcome, PVP_OUTCOME_DRAW);  // nobody attacked: equal percent
}

static void testRestLimitAndFailedRestBecomesDodge() {
  PvpRuntime battle = begin(statsOf(60, 30, 40, 20), statsOf(60, 30, 40, 20));
  battle.side[0].hp = 1;
  PvpTurnResult t1 = pvpStep(battle, BATTLE_REST, BATTLE_DODGE, 10);
  EXPECT_EQ(t1.side[0].healed, restHealExpected(battle.side[0].maxHp, 1));
  battle.side[0].hp = 1;
  pvpStep(battle, BATTLE_REST, BATTLE_DODGE, 10);
  EXPECT_EQ(battle.side[0].restUsesLeft, 0);
  battle.side[0].hp = 1;
  PvpRuntime copy = battle;
  PvpTurnResult t3 = pvpStep(battle, BATTLE_REST, BATTLE_ATTACK, 10);
  EXPECT_TRUE(t3.side[0].restFailed);
  EXPECT_EQ(t3.side[0].action, BATTLE_DODGE);
  EXPECT_EQ(t3.side[0].healed, 0);
  EXPECT_EQ(battle.side[0].restUsesLeft, 0);
  PvpTurnResult t4 = pvpStep(copy, BATTLE_DODGE, BATTLE_ATTACK, 10);
  EXPECT_TRUE(sameState(battle, copy));
  EXPECT_TRUE(!t4.side[0].restFailed);
}

static void testRestAtFullHpDoesNotOverheal() {
  PvpRuntime battle = begin(statsOf(60, 30, 40, 20), statsOf(60, 30, 40, 20));
  PvpTurnResult turn = pvpStep(battle, BATTLE_REST, BATTLE_DODGE, 10);
  EXPECT_EQ(turn.side[0].healed, 0);
  EXPECT_EQ(battle.side[0].hp, battle.side[0].maxHp);
  EXPECT_EQ(battle.side[0].restUsesLeft, 1);
}

static void testDodgeBlocksAndGrantsCounter() {
  // roll 10: A luck 10 (< 85 dodge ok), B luck 89.
  PvpRuntime battle = begin(statsOf(80, 30, 40, 20), statsOf(80, 30, 40, 20));
  PvpTurnResult turn = pvpStep(battle, BATTLE_DODGE, BATTLE_ATTACK, 10);
  EXPECT_TRUE(turn.side[0].dodged);
  EXPECT_TRUE(turn.side[0].counterGained);
  EXPECT_EQ(turn.side[1].damageDealt, 0);
  EXPECT_EQ(battle.side[0].hp, battle.side[0].maxHp);
  EXPECT_TRUE(battle.side[0].counterReady);

  PvpRuntime fresh = begin(statsOf(80, 30, 40, 20), statsOf(80, 30, 40, 20));
  PvpTurnResult plain = pvpStep(fresh, BATTLE_ATTACK, BATTLE_REST, 50);
  PvpTurnResult boosted = pvpStep(battle, BATTLE_ATTACK, BATTLE_REST, 50);
  EXPECT_TRUE(boosted.side[0].counterUsed);
  EXPECT_TRUE(boosted.side[0].damageDealt > plain.side[0].damageDealt);
  EXPECT_TRUE(!battle.side[0].counterReady);
}

static void testDodgeWithoutIncomingAttackGivesNoCounter() {
  PvpRuntime battle = begin(statsOf(80, 30, 40, 20), statsOf(80, 30, 40, 20));
  pvpStep(battle, BATTLE_DODGE, BATTLE_DODGE, 10);
  EXPECT_TRUE(!battle.side[0].counterReady);
  EXPECT_TRUE(!battle.side[1].counterReady);
}

static void testFailedDodgeReducesDamageToThird() {
  PvpRuntime ok = begin(statsOf(80, 30, 40, 20), statsOf(80, 30, 40, 20));
  PvpRuntime bad = ok;
  PvpRuntime none = ok;
  // Attacker B luck = 99 - roll; keep B's luck identical (60) via roll 39 vs the failed dodge needing A luck >= 85.
  // A luck equals roll, so use direct comparison: roll 85 -> A luck 85 (fails), B luck 14.
  PvpTurnResult failed = pvpStep(bad, BATTLE_DODGE, BATTLE_ATTACK, 85);
  PvpTurnResult plain = pvpStep(none, BATTLE_ATTACK, BATTLE_ATTACK, 85);
  EXPECT_TRUE(!failed.side[0].dodged);
  EXPECT_TRUE(!bad.side[0].counterReady);
  EXPECT_TRUE(failed.side[1].damageDealt > 0);
  EXPECT_TRUE(failed.side[1].damageDealt < plain.side[1].damageDealt);
  (void)ok;
}

static void testIncomingGuardMultipliersOrder() {
  BattleStats a = statsOf(80, 30, 1, 20);
  BattleStats b = statsOf(120, 30, 1, 20);
  uint16_t dmg[4] = {};
  const BattleAction defs[4] = { BATTLE_REST, BATTLE_ATTACK_QUICK, BATTLE_ATTACK, BATTLE_ATTACK_HEAVY };
  for (int i = 0; i < 4; i++) {
    PvpRuntime battle = begin(a, b);
    battle.side[0].hp = battle.side[0].maxHp;
    PvpTurnResult turn = pvpStep(battle, defs[i], BATTLE_ATTACK, 39);  // B luck 60
    dmg[i] = turn.side[1].damageDealt;
    EXPECT_TRUE(dmg[i] > 0);
  }
  EXPECT_TRUE(dmg[0] < dmg[1]);
  EXPECT_TRUE(dmg[1] < dmg[2]);
  EXPECT_TRUE(dmg[2] < dmg[3]);
}

static void testNoOverflowOrNegativeHpWithExtremeValidStats() {
  BattleStats mon = statsOf(999, 1, 999, 100, TYPE_FIRE);
  BattleStats weak = statsOf(1, 999, 1, 1, TYPE_GRASS);
  for (BattleAction actA : ALL_ACTIONS) {
    for (BattleAction actB : ALL_ACTIONS) {
      PvpRuntime battle = begin(mon, weak);
      for (int i = 0; i < 25; i++) {
        pvpStep(battle, actA, actB, (uint8_t)((i * 13) % 100));
        EXPECT_TRUE(battle.side[0].hp <= battle.side[0].maxHp);
        EXPECT_TRUE(battle.side[1].hp <= battle.side[1].maxHp);
      }
      EXPECT_TRUE(battle.side[0].damageTotal <= battle.side[1].maxHp * 3u);
    }
  }
}

// Unvalidated absurd stats used to wrap uint16 inside the shared damage formula
// (HEAVY ended up weaker than ATTACK). The result must stay monotonic and capped.
static void testCoreDamageDoesNotWrapOnAbsurdStats() {
  BattleStats big = statsOf(65535, 1, 1, 255);
  BattleStats paper = statsOf(1, 1, 1, 1);
  const uint16_t maxHp = 65535;
  const uint16_t cap = (uint16_t)((uint32_t)maxHp * battle_core::TURN_CAP_PCT / 100);
  uint16_t atk = battle_core::turnDamage(big, paper, maxHp, 50, false, 100);
  uint16_t heavy = battle_core::turnDamage(big, paper, maxHp, 50, false, battle_core::POWER_HEAVY_PCT);
  uint16_t counter = battle_core::turnDamage(big, paper, maxHp, 50, true, battle_core::POWER_HEAVY_PCT);
  EXPECT_TRUE(atk <= heavy);
  EXPECT_TRUE(heavy <= counter);
  EXPECT_TRUE(counter <= cap);
  EXPECT_EQ(battle_core::turnDamage(big, paper, maxHp, 50, true, battle_core::POWER_HEAVY_PCT), cap);
}

static void testStepAfterEndChangesNothing() {
  PvpRuntime battle = begin(statsOf(200, 20, 10, 50), statsOf(10, 20, 10, 5));
  battle.side[1].hp = 1;
  pvpStep(battle, BATTLE_ATTACK, BATTLE_ATTACK, 50);
  PvpRuntime snapshot = battle;
  uint32_t hash = pvpStateHash(battle);
  PvpTurnResult turn = pvpStep(battle, BATTLE_ATTACK, BATTLE_REST, 30);
  EXPECT_TRUE(sameState(battle, snapshot));
  EXPECT_EQ(pvpStateHash(battle), hash);
  EXPECT_TRUE(turn.battleEnded);
  EXPECT_EQ(turn.outcome, PVP_OUTCOME_A_WINS);
  EXPECT_EQ(turn.side[0].damageDealt, 0);
}

static void testHashReactsToEveryField() {
  PvpRuntime base = begin(statsOf(70, 50, 60, 20), statsOf(65, 55, 45, 19));
  uint32_t h = pvpStateHash(base);
  PvpRuntime v;
  v = base; v.round++; EXPECT_TRUE(pvpStateHash(v) != h);
  for (int i = 0; i < 2; i++) {
    v = base; v.side[i].hp--; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].maxHp++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].damageTotal++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].damageTotal += 256; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].restUsesLeft--; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].counterReady = true; EXPECT_TRUE(pvpStateHash(v) != h);
  }
  for (int i = 0; i < 2; i++) {
    v = base; v.side[i].dodgeHeat = 1; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.atk++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.def++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.spe++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.hp++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.level++; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.type1 = TYPE_FIRE; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.type2 = TYPE_FIRE; EXPECT_TRUE(pvpStateHash(v) != h);
    v = base; v.side[i].stats.atk += 256; EXPECT_TRUE(pvpStateHash(v) != h);
  }
  PvpRuntime swapped = base;
  swapped.side[0].hp = base.side[1].hp - 1;
  swapped.side[1].hp = base.side[0].hp;
  EXPECT_TRUE(pvpStateHash(swapped) != h);
}

static void testBeginUsesSameMaxHpAsPve() {
  BattleStats a = statsOf(70, 50, 60, 20);
  BattleStats b = statsOf(65, 55, 45, 19);
  PvpRuntime pvp = begin(a, b);
  BattleRuntime pve = beginBattleRuntime(a, b);
  EXPECT_EQ(pvp.side[0].maxHp, pve.playerMaxHp);
  EXPECT_EQ(pvp.side[1].maxHp, pve.enemyMaxHp);
  EXPECT_EQ(pvp.side[0].restUsesLeft, pve.restUsesLeft);
}

static void testPerspectiveHelpers() {
  PvpRuntime battle = begin(statsOf(120, 30, 1, 20), statsOf(60, 30, 1, 20));
  PvpTurnResult turn = pvpStep(battle, BATTLE_ATTACK, BATTLE_REST, 50);
  PvpTurnView asA = pvpTurnView(turn, PVP_SIDE_A);
  PvpTurnView asB = pvpTurnView(turn, PVP_SIDE_B);
  EXPECT_EQ(asA.mine.damageDealt, asB.theirs.damageDealt);
  EXPECT_EQ(asA.theirs.damageDealt, asB.mine.damageDealt);
  EXPECT_EQ(pvpMySide(battle, PVP_SIDE_B).hp, pvpTheirSide(battle, PVP_SIDE_A).hp);
  EXPECT_EQ(pvpViewOutcome(PVP_OUTCOME_A_WINS, PVP_SIDE_A), PVP_VIEW_WIN);
  EXPECT_EQ(pvpViewOutcome(PVP_OUTCOME_A_WINS, PVP_SIDE_B), PVP_VIEW_LOSS);
  EXPECT_EQ(pvpViewOutcome(PVP_OUTCOME_DRAW, PVP_SIDE_B), PVP_VIEW_DRAW);
  EXPECT_EQ(pvpViewOutcome(PVP_OUTCOME_ONGOING, PVP_SIDE_A), PVP_VIEW_ONGOING);
}

// Fixed script, exact values: any change to a formula, constant or order of
// operations has to show up here (and bump PVP_RULES_VERSION).
static void testGoldenScript() {
  BattleStats a = statsOf(70, 50, 60, 20, TYPE_FIRE);
  BattleStats b = statsOf(65, 55, 45, 19, TYPE_GRASS, TYPE_POISON);
  const uint8_t actA[12] = { 0, 2, 3, 1, 4, 0, 2, 3, 3, 0, 1, 2 };
  const uint8_t actB[12] = { 1, 0, 2, 3, 0, 4, 3, 0, 2, 2, 1, 0 };
  const uint8_t roll[12] = { 12, 87, 40, 63, 5, 99, 28, 71, 50, 33, 90, 18 };
  const uint16_t expect[12][4] = {
    {11, 0, 180, 169}, {0, 10, 170, 169}, {0, 0, 170, 169}, {5, 0, 170, 164},
    {0, 6, 173, 164}, {9, 0, 173, 164}, {5, 0, 173, 159}, {0, 2, 171, 159},
    {0, 3, 168, 159}, {16, 10, 158, 143}, {11, 6, 152, 132}, {17, 0, 152, 115},
  };
  PvpRuntime battle = begin(a, b);
  EXPECT_EQ(battle.side[0].maxHp, 180);
  EXPECT_EQ(pvpStateHash(battle), 1294108895u);
  for (int i = 0; i < 12; i++) {
    PvpTurnResult t = pvpStep(battle, actA[i], actB[i], roll[i]);
    EXPECT_EQ(t.side[0].damageDealt, expect[i][0]);
    EXPECT_EQ(t.side[1].damageDealt, expect[i][1]);
    EXPECT_EQ(battle.side[0].hp, expect[i][2]);
    EXPECT_EQ(battle.side[1].hp, expect[i][3]);
  }
  EXPECT_EQ(battle.round, 12);
  EXPECT_EQ(pvpStateHash(battle), 3922135648u);
}

// Three DODGEs in a row against ATTACK at roll 30: blocked at band 0 (limit 65) and band 1
// (limit 40), not blocked at band 2 (limit 20). Heat 2, 4, 6, then -1 per non-DODGE turn.
static void testGoldenDodgeHeat() {
  BattleStats a = statsOf(70, 50, 60, 20, TYPE_FIRE);
  BattleStats b = statsOf(65, 55, 45, 19, TYPE_GRASS, TYPE_POISON);
  // dodged, damage taken by A, heat of A, hp of A, counter gained, counter ready
  const uint16_t expect[5][6] = {
    {1, 0, 2, 180, 1, 1}, {1, 0, 4, 180, 1, 1}, {0, 2, 6, 178, 0, 1},
    {0, 8, 5, 170, 0, 0}, {0, 8, 4, 162, 0, 0},
  };
  PvpRuntime battle = begin(a, b);
  for (int i = 0; i < 5; i++) {
    uint8_t actA = i < 3 ? BATTLE_DODGE : BATTLE_ATTACK;
    PvpTurnResult t = pvpStep(battle, actA, BATTLE_ATTACK, 30);
    EXPECT_EQ(t.side[0].dodged, expect[i][0] != 0);
    EXPECT_EQ(t.side[1].damageDealt, expect[i][1]);
    EXPECT_EQ(battle.side[0].dodgeHeat, expect[i][2]);
    EXPECT_EQ(battle.side[0].hp, expect[i][3]);
    EXPECT_EQ(t.side[0].counterGained, expect[i][4] != 0);
    EXPECT_EQ(battle.side[0].counterReady, expect[i][5] != 0);
  }
  EXPECT_EQ(pvpStateHash(battle), 44189260u);
}

static void testStepOnUnstartedBattleIsRejected() {
  PvpRuntime zeroed = {};
  PvpTurnResult t = pvpStep(zeroed, BATTLE_ATTACK, BATTLE_ATTACK, 10);
  EXPECT_EQ(t.error, PVP_ERR_STATS);
  EXPECT_TRUE(!t.battleEnded);
  EXPECT_EQ(zeroed.round, 0);
  PvpRuntime failed;
  BattleStats bad = statsOf(70, 50, 60, 0);
  EXPECT_EQ(pvpBegin(failed, bad, bad), PVP_ERR_STATS);
  EXPECT_EQ(pvpStep(failed, BATTLE_ATTACK, BATTLE_ATTACK, 10).error, PVP_ERR_STATS);
}

static void testValidation() {
  BattleStats ok = statsOf(70, 50, 60, 20, TYPE_FIRE, TYPE_FAIRY);
  EXPECT_TRUE(pvpValidateStats(ok));
  BattleStats v = ok;
  v.level = 0; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.level = 100; EXPECT_TRUE(pvpValidateStats(v));
  v = ok; v.level = 101; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.atk = 0; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.atk = 999; EXPECT_TRUE(pvpValidateStats(v));
  v = ok; v.atk = 1000; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.def = 0; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.def = 1000; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.spe = 0; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.spe = 1000; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.hp = 0; EXPECT_TRUE(pvpValidateStats(v));
  v = ok; v.hp = 999; EXPECT_TRUE(pvpValidateStats(v));
  v = ok; v.hp = 1000; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.type1 = 19; EXPECT_TRUE(!pvpValidateStats(v));
  v = ok; v.type2 = 19; EXPECT_TRUE(!pvpValidateStats(v));

  PvpRuntime r;
  v = ok; v.atk = 1000;
  EXPECT_EQ(pvpBegin(r, ok, v), PVP_ERR_STATS);
  EXPECT_EQ(r.round, 0);
  EXPECT_EQ(r.side[0].maxHp, 0);
  EXPECT_EQ(pvpBegin(r, v, ok), PVP_ERR_STATS);

  EXPECT_TRUE(pvpValidAction(BATTLE_REST));
  EXPECT_TRUE(!pvpValidAction(5));
  EXPECT_TRUE(!pvpValidAction(255));

  PvpRuntime battle = begin(ok, ok);
  PvpRuntime snapshot = battle;
  EXPECT_EQ(pvpStep(battle, 5, BATTLE_ATTACK, 10).error, PVP_ERR_ACTION);
  EXPECT_EQ(pvpStep(battle, BATTLE_ATTACK, 200, 10).error, PVP_ERR_ACTION);
  EXPECT_EQ(pvpStep(battle, BATTLE_ATTACK, BATTLE_ATTACK, 100).error, PVP_ERR_LUCK);
  EXPECT_TRUE(sameState(battle, snapshot));
  EXPECT_EQ(pvpStep(battle, BATTLE_ATTACK, BATTLE_ATTACK, 99).error, PVP_OK);
  EXPECT_EQ(pvpRollFrom(0), 0);
  EXPECT_EQ(pvpRollFrom(99), 99);
  EXPECT_EQ(pvpRollFrom(100), 0);
  EXPECT_EQ(pvpRollFrom(4294967295u), 95);
}

// Damage must come from the ATTACKER's luck: A uses roll, B uses 99 - roll.
static void testDamageUsesAttackerLuck() {
  BattleStats sa = statsOf(90, 40, 1, 30);
  BattleStats sb = statsOf(60, 70, 1, 28);
  const uint8_t rolls[3] = { 20, 45, 80 };
  for (uint8_t roll : rolls) {
    PvpRuntime battle = begin(sa, sb);
    PvpTurnResult t = pvpStep(battle, BATTLE_ATTACK, BATTLE_ATTACK, roll);
    EXPECT_EQ(t.side[0].damageDealt,
              battle_core::turnDamage(sa, sb, battle.side[1].maxHp, roll, false, 100));
    EXPECT_EQ(t.side[1].damageDealt,
              battle_core::turnDamage(sb, sa, battle.side[0].maxHp, (uint8_t)(99 - roll), false, 100));
  }
}

static uint16_t guardedExpected(const PvpRuntime &battle, const BattleStats &att, const BattleStats &def,
                                uint8_t luck, uint8_t power, uint8_t guardPct) {
  uint16_t raw = battle_core::turnDamage(att, def, battle.side[0].maxHp, luck, false, power);
  uint16_t scaled = (uint16_t)((uint32_t)raw * guardPct / 100);
  return scaled == 0 ? 1 : scaled;
}

static void testQuickGuardDependsOnAttackerAction() {
  BattleStats sa = statsOf(80, 30, 1, 20);
  BattleStats sb = statsOf(120, 30, 1, 20);
  struct Case { BattleAction att; uint8_t power; uint8_t pct; };
  const Case cases[3] = {
    { BATTLE_ATTACK, 100, battle_core::GUARD_QUICK_PCT },
    { BATTLE_ATTACK_QUICK, battle_core::POWER_QUICK_PCT, PVP_GUARD_QUICK_VS_QUICK_PCT },
    { BATTLE_ATTACK_HEAVY, battle_core::POWER_HEAVY_PCT, PVP_GUARD_QUICK_VS_HEAVY_PCT },
  };
  for (const Case &c : cases) {
    PvpRuntime battle = begin(sa, sb);
    PvpTurnResult t = pvpStep(battle, BATTLE_ATTACK_QUICK, c.att, 39);  // B luck 60
    EXPECT_EQ(t.side[1].damageDealt, guardedExpected(battle, sb, sa, 60, c.power, c.pct));
  }
  const BattleAction defs[2] = { BATTLE_REST, BATTLE_ATTACK_HEAVY };
  const uint8_t pcts[2] = { battle_core::GUARD_REST_PCT, battle_core::GUARD_HEAVY_PCT };
  for (int i = 0; i < 2; i++) {
    PvpRuntime battle = begin(sa, sb);
    PvpTurnResult t = pvpStep(battle, defs[i], BATTLE_ATTACK, 39);
    EXPECT_EQ(t.side[1].damageDealt, guardedExpected(battle, sb, sa, 60, 100, pcts[i]));
  }
}

// A (defender) dodges B's attack. Success iff A luck (= roll) < limit.
static bool dodgeBlocks(uint8_t roll, BattleAction attackerAction, int priorDodges) {
  BattleStats s = statsOf(80, 30, 1, 20);
  PvpRuntime battle = begin(s, s);
  for (int i = 0; i < priorDodges; i++) pvpStep(battle, BATTLE_DODGE, BATTLE_DODGE, 50);
  return pvpStep(battle, BATTLE_DODGE, attackerAction, roll).side[0].dodged;
}

static void testDodgeThresholdsAndStreak() {
  EXPECT_TRUE(dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[0] - 1, BATTLE_ATTACK, 0));
  EXPECT_TRUE(!dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[0], BATTLE_ATTACK, 0));
  EXPECT_TRUE(dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[1] - 1, BATTLE_ATTACK, 1));
  EXPECT_TRUE(!dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[1], BATTLE_ATTACK, 1));
  EXPECT_TRUE(dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[2] - 1, BATTLE_ATTACK_HEAVY, 2));
  EXPECT_TRUE(!dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[2], BATTLE_ATTACK_HEAVY, 2));
  EXPECT_TRUE(!dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[2], BATTLE_ATTACK, 5));  // stays at the last limit
  EXPECT_TRUE(dodgeBlocks(PVP_DODGE_LIMIT_BY_BAND[2] - 1, BATTLE_ATTACK, 5));
  EXPECT_TRUE(dodgeBlocks(PVP_DODGE_VS_QUICK_LIMIT - 1, BATTLE_ATTACK_QUICK, 0));
  EXPECT_TRUE(!dodgeBlocks(PVP_DODGE_VS_QUICK_LIMIT, BATTLE_ATTACK_QUICK, 0));
  // heat 1 (one DODGE then one other turn) is already band 1: roll 50 passes limit 65 but not 40
  {
    BattleStats h = statsOf(80, 30, 1, 20);
    PvpRuntime cooled = begin(h, h);
    pvpStep(cooled, BATTLE_DODGE, BATTLE_DODGE, 50);
    pvpStep(cooled, BATTLE_ATTACK, BATTLE_DODGE, 50);
    EXPECT_EQ(cooled.side[0].dodgeHeat, 1);
    EXPECT_TRUE(!pvpStep(cooled, BATTLE_DODGE, BATTLE_ATTACK, 50).side[0].dodged);
  }
  // heat rises by STEP per DODGE and falls by DECAY on other turns
  BattleStats s = statsOf(80, 30, 1, 20);
  PvpRuntime battle = begin(s, s);
  pvpStep(battle, BATTLE_DODGE, BATTLE_DODGE, 50);
  EXPECT_EQ(battle.side[0].dodgeHeat, PVP_DODGE_HEAT_STEP);
  pvpStep(battle, BATTLE_ATTACK, BATTLE_DODGE, 50);
  EXPECT_EQ(battle.side[0].dodgeHeat, PVP_DODGE_HEAT_STEP - PVP_DODGE_HEAT_DECAY);
  EXPECT_EQ(battle.side[1].dodgeHeat, 2 * PVP_DODGE_HEAT_STEP);
  for (int i = 0; i < 10; i++) pvpStep(battle, BATTLE_DODGE, BATTLE_DODGE, 50);
  EXPECT_EQ(battle.side[1].dodgeHeat, PVP_DODGE_HEAT_MAX);  // saturates, never past the table
  EXPECT_TRUE(battle.side[0].dodgeHeat <= PVP_DODGE_HEAT_MAX);
}

static void testFailedDodgeDamageIsExactThird() {
  BattleStats sa = statsOf(80, 30, 1, 20);
  BattleStats sb = statsOf(120, 30, 1, 20);
  PvpRuntime battle = begin(sa, sb);
  uint16_t raw = battle_core::turnDamage(sb, sa, battle.side[0].maxHp, 14, false, 100);  // roll 85 -> B luck 14
  PvpTurnResult t = pvpStep(battle, BATTLE_DODGE, BATTLE_ATTACK, 85);
  EXPECT_EQ(t.side[1].damageDealt, raw / battle_core::FAILED_DODGE_DIVISOR);
}

static void testCounterSurvivesBlockAndEvade() {
  BattleStats a = statsOf(80, 30, 1, 20);
  BattleStats b = statsOf(80, 30, 1, 20);
  PvpRuntime blocked = begin(a, b);
  blocked.side[0].counterReady = true;
  pvpStep(blocked, BATTLE_ATTACK, BATTLE_DODGE, 80);  // B luck 19 blocks
  EXPECT_TRUE(blocked.side[0].counterReady);
  PvpRuntime evaded = begin(a, b);
  evaded.side[0].counterReady = true;
  pvpStep(evaded, BATTLE_ATTACK, BATTLE_ATTACK, 5);  // (5 + 0) % 100 < 10: B evades
  EXPECT_TRUE(evaded.side[0].counterReady);
  PvpRuntime hit = begin(a, b);
  hit.side[0].counterReady = true;
  pvpStep(hit, BATTLE_ATTACK, BATTLE_ATTACK, 50);
  EXPECT_TRUE(!hit.side[0].counterReady);
}

static void testFasterTargetNeedsStrictlyHigherSpe() {
  // roll 95: (95 + 40/2) % 100 = 15 -> evaded only with the 18% chance
  BattleStats attacker = statsOf(80, 30, 40, 20);
  BattleStats equal = statsOf(80, 30, 40, 20);
  BattleStats faster = statsOf(80, 30, 41, 20);
  PvpRuntime same = begin(attacker, equal);
  EXPECT_TRUE(!pvpStep(same, BATTLE_ATTACK, BATTLE_ATTACK, 95).side[1].evaded);
  PvpRuntime fast = begin(attacker, faster);
  PvpTurnResult t = pvpStep(fast, BATTLE_ATTACK, BATTLE_ATTACK, 95);
  EXPECT_TRUE(t.side[1].evaded);
}

static void testQuickPiercesDodgeButHeavyDoesNot() {
  EXPECT_TRUE(dodgeBlocks(35, BATTLE_ATTACK_HEAVY, 0));
  EXPECT_TRUE(!dodgeBlocks(35, BATTLE_ATTACK_QUICK, 0));
}


int main() {
  testGoldenScript();
  testGoldenDodgeHeat();
  testStepOnUnstartedBattleIsRejected();
  testValidation();
  testDamageUsesAttackerLuck();
  testQuickGuardDependsOnAttackerAction();
  testDodgeThresholdsAndStreak();
  testFailedDodgeDamageIsExactThird();
  testCounterSurvivesBlockAndEvade();
  testFasterTargetNeedsStrictlyHigherSpe();
  testQuickPiercesDodgeButHeavyDoesNot();
  testInvariantsAllActionPairs();
  testSymmetryWhenSidesAreSwapped();
  testKnockoutDecidesWinner();
  testSimultaneousKnockoutIsDraw();
  testRestingSideCanStillDieToSimultaneousHit();
  testRoundLimitComparesHpPercentCrosswise();
  testBattleEndsAtRoundLimit();
  testRestLimitAndFailedRestBecomesDodge();
  testRestAtFullHpDoesNotOverheal();
  testDodgeBlocksAndGrantsCounter();
  testDodgeWithoutIncomingAttackGivesNoCounter();
  testFailedDodgeReducesDamageToThird();
  testIncomingGuardMultipliersOrder();
  testNoOverflowOrNegativeHpWithExtremeValidStats();
  testCoreDamageDoesNotWrapOnAbsurdStats();
  testStepAfterEndChangesNothing();
  testHashReactsToEveryField();
  testBeginUsesSameMaxHpAsPve();
  testPerspectiveHelpers();

  if (failures) {
    std::cerr << failures << " Testfehler\n";
    return EXIT_FAILURE;
  }
  std::cout << "Alle pvp_battle.cpp-Tests bestanden\n";
  return EXIT_SUCCESS;
}
