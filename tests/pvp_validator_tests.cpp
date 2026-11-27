// Opponent READY validation against the Pokedex.
#include "dex.h"
#include "pvp_test_harness.h"
#include "pvp_validator.h"

static PvpReadyPayload readyFor(uint16_t dex, uint8_t level, uint8_t gene, uint8_t training) {
  const DexEntry &d = DEX_TBL[dex];
  PvpReadyPayload r;
  r.dex = dex;
  r.level = level;
  r.atk = (uint16_t)(d.bAtk * gene / 100 + level + training);
  r.def = (uint16_t)(d.bDef * gene / 100 + level + training);
  r.spe = (uint16_t)(d.bSpe * gene / 100 + level + training);
  r.type1 = d.type1;
  r.type2 = d.type2;
  return r;
}

static void testWholeLegalGridIsAccepted() {
  int bad = 0;
  for (uint16_t dex = 1; dex <= DEX_COUNT; dex++) {
    for (uint8_t level : {1, 50, 100}) {
      for (uint8_t gene : {PVP_GENE_MIN, PVP_GENE_MAX}) {
        for (uint8_t tr : {0, 100}) {
          if (pvpCheckOpponentReady(readyFor(dex, level, gene, tr)) != PVP_STATS_OK) bad++;
        }
      }
    }
  }
  EXPECT_EQ(bad, 0);
}

static void testCheatsAreRejected() {
  PvpReadyPayload ok = readyFor(6, 40, 100, 0);
  EXPECT_EQ(pvpCheckOpponentReady(ok), PVP_STATS_OK);

  PvpReadyPayload r = ok;
  r.hp = 1;  // would raise the rival's max HP without a desync
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_HP_NONZERO);

  // independent bounds (Charizard atk 84, level 40): 84*90/100+40 .. 84*110/100+40+100
  r = ok; r.atk = 84 * 110 / 100 + 40 + 100;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_OK);
  r.atk = (uint16_t)(r.atk + 1);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_STAT);
  r.atk = 84 * 90 / 100 + 40;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_OK);
  r.atk = (uint16_t)(r.atk - 1);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_STAT);

  PvpStatRange atk = pvpStatRange(DEX_TBL[6].bAtk, 40);
  r = ok; r.atk = (uint16_t)(atk.hi + 1);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_STAT);
  r = ok; r.atk = (uint16_t)(atk.lo - 1);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_STAT);
  PvpStatRange def = pvpStatRange(DEX_TBL[6].bDef, 40);
  r = ok; r.def = (uint16_t)(def.hi + 1);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_STAT);
  PvpStatRange spe = pvpStatRange(DEX_TBL[6].bSpe, 40);
  r = ok; r.spe = (uint16_t)(spe.hi + 1);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_STAT);

  r = ok; r.type1 = (uint8_t)(r.type1 == TYPE_WATER ? TYPE_FIRE : TYPE_WATER);
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_TYPE);
  r = ok; r.type2 = TYPE_DRAGON;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_TYPE);

  r = ok; r.dex = 0;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_DEX);
  r = ok; r.dex = DEX_COUNT + 1;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_DEX);
  r = ok; r.level = 0;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_LEVEL);
  r = ok; r.level = 101;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_LEVEL);
  r = ok; r.shiny = 2;
  EXPECT_EQ(pvpCheckOpponentReady(r), PVP_STATS_BAD_SHINY);
}

static void testLocalReadyCheckIgnoresHpButCatchesBadStats() {
  PvpReadyPayload r = readyFor(4, 20, 100, 0);
  r.hp = 77;  // raw pet value: the wire copy is zeroed anyway
  EXPECT_EQ(pvpCheckLocalReady(r), PVP_STATS_OK);
  r.atk = (uint16_t)(r.atk + 200);  // e.g. hand-edited backup
  EXPECT_EQ(pvpCheckLocalReady(r), PVP_STATS_BAD_STAT);
}

static void testReadyFromStatsAlwaysZeroesHp() {
  BattleStats s = {};
  s.hp = 500; s.atk = 50; s.def = 40; s.spe = 30; s.level = 12; s.type1 = TYPE_FIRE;
  PvpReadyPayload r = pvpReadyFromStats(4, true, s);
  EXPECT_EQ(r.hp, 0);
  EXPECT_EQ(r.dex, 4);
  EXPECT_EQ(r.shiny, 1);
  EXPECT_EQ(r.atk, 50);
  EXPECT_EQ(pvpReadyFromStats(-5, false, s).dex, 0);
}

int main() {
  testWholeLegalGridIsAccepted();
  testCheatsAreRejected();
  testLocalReadyCheckIgnoresHpButCatchesBadStats();
  testReadyFromStatsAlwaysZeroesHp();
  if (failures) {
    std::cerr << failures << " Fehler\n";
    return 1;
  }
  std::cout << "pvp_validator_tests: OK\n";
  return 0;
}
