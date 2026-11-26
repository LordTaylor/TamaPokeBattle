#include <cstdlib>
#include <cstring>
#include <iostream>

#include "trainer.h"

static int failures = 0;

#define EXPECT_EQ(actual, expected) do { \
  auto a = (actual); \
  auto e = (expected); \
  if (!(a == e)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #actual \
              << " war " << +a << ", erwartet " << +e << "\n"; \
    failures++; \
  } \
} while (0)

#define EXPECT_TRUE(expr) EXPECT_EQ(!!(expr), true)

static void testLevelCostTable() {
  const struct { uint8_t level; uint16_t cost; } table[] = {
    {1, 12}, {2, 12}, {3, 13}, {5, 16}, {9, 45}, {10, 50}, {20, 100}, {49, 245}
  };
  for (const auto &row : table) EXPECT_EQ(trainerLevelCost(row.level), row.cost);
  EXPECT_EQ(trainerLevelCost(TRAINER_LEVEL_MAX), 0);
}

static void testPointsTable() {
  // wygrana: 3, +2 z wyzszym; remis: 2; przegrana: 1, -2 z nizszym; abort: 0
  EXPECT_EQ(trainerPointsDelta(PVP_WIN, 5, 9), 5);
  EXPECT_EQ(trainerPointsDelta(PVP_WIN, 5, 5), 3);
  EXPECT_EQ(trainerPointsDelta(PVP_WIN, 5, 2), 3);
  EXPECT_EQ(trainerPointsDelta(PVP_DRAW, 5, 9), 2);
  EXPECT_EQ(trainerPointsDelta(PVP_DRAW, 5, 5), 2);
  EXPECT_EQ(trainerPointsDelta(PVP_DRAW, 5, 2), 2);
  EXPECT_EQ(trainerPointsDelta(PVP_LOSS, 5, 9), 1);
  EXPECT_EQ(trainerPointsDelta(PVP_LOSS, 5, 5), 1);
  EXPECT_EQ(trainerPointsDelta(PVP_LOSS, 5, 2), -1);
  EXPECT_EQ(trainerPointsDelta(PVP_ABORTED, 5, 9), 0);
}

static void testMultiLevelUpKeepsRemainder() {
  TrainerStats s;
  s.progress = 11;
  // level 1 -> koszt 12; +5 = 16: awans, zostaje 4 (koszt lvl 2 = 12)
  TrainerAward award = trainerApplyResult(s, PVP_WIN, 9, true);
  EXPECT_EQ(award.levelsGained, 1);
  EXPECT_EQ(s.level, 2);
  EXPECT_EQ(s.progress, 4);
}

static void testMultiLevelUpLoop() {
  TrainerStats s;
  s.progress = 34;  // 34 + 3 = 37: 12 + 12 + 13 = 37 -> trzy awanse, reszta 0
  TrainerAward award = trainerApplyResult(s, PVP_WIN, 1, true);
  EXPECT_EQ(award.levelsGained, 3);
  EXPECT_EQ(s.level, 4);
  EXPECT_EQ(s.progress, 0);
}

static void testLossNeverDropsLevelOrGoesNegative() {
  TrainerStats s;
  s.level = 10;
  s.progress = 1;
  for (int i = 0; i < 20; i++) trainerApplyResult(s, PVP_LOSS, 1, true);  // -1 kazda
  EXPECT_EQ(s.level, 10);
  EXPECT_EQ(s.progress, 0);
  EXPECT_EQ(s.losses, 20);
}

static void testAbortChangesNothing() {
  TrainerStats s;
  s.level = 7;
  s.progress = 5;
  TrainerAward award = trainerApplyResult(s, PVP_ABORTED, 30, true);
  EXPECT_TRUE(!award.applied);
  EXPECT_TRUE(!award.blocked);
  EXPECT_EQ(s.level, 7);
  EXPECT_EQ(s.progress, 5);
  EXPECT_EQ(s.matchSeq, 0u);
  EXPECT_EQ(s.wins + s.losses + s.draws, 0);
}

static void testBlockedPointsStillCountMatch() {
  TrainerStats s;
  TrainerAward award = trainerApplyResult(s, PVP_WIN, 9, false);
  EXPECT_TRUE(!award.applied);
  EXPECT_TRUE(award.blocked);  // zablokowane przez guard, nie ABORTED
  EXPECT_EQ(s.wins, 1);
  EXPECT_EQ(s.matchSeq, 1u);
  EXPECT_EQ(s.progress, 0);
}

static void testCounterSaturation() {
  TrainerStats s;
  s.wins = s.losses = s.draws = 65535;
  trainerApplyResult(s, PVP_WIN, 1, false);
  trainerApplyResult(s, PVP_LOSS, 1, false);
  trainerApplyResult(s, PVP_DRAW, 1, false);
  EXPECT_EQ(s.wins, 65535);
  EXPECT_EQ(s.losses, 65535);
  EXPECT_EQ(s.draws, 65535);
  EXPECT_EQ(s.matchSeq, 3u);
}

static void testMaxLevelHasNoProgress() {
  TrainerStats s;
  s.level = TRAINER_LEVEL_MAX - 1;
  s.progress = 244;
  trainerApplyResult(s, PVP_WIN, 50, true);  // 244 + 5 >= 245
  EXPECT_EQ(s.level, TRAINER_LEVEL_MAX);
  EXPECT_EQ(s.progress, 0);  // nadwyzka porzucona na max poziomie
  s.progress = 0;
  trainerApplyResult(s, PVP_WIN, 50, true);
  EXPECT_EQ(s.level, TRAINER_LEVEL_MAX);
  EXPECT_EQ(s.progress, 0);
}

static void testSanitizeStats() {
  TrainerStats s;
  s.level = 0;
  s.progress = 3;
  sanitizeTrainerStats(s);
  EXPECT_EQ(s.level, 1);
  EXPECT_EQ(s.progress, 3);

  s.level = 200;
  s.progress = 77;
  sanitizeTrainerStats(s);
  EXPECT_EQ(s.level, TRAINER_LEVEL_MAX);
  EXPECT_EQ(s.progress, 0);

  s.level = 1;
  s.progress = 12;  // == koszt
  sanitizeTrainerStats(s);
  EXPECT_EQ(s.progress, 0);

  s.wins = 9;
  sanitizeTrainerStats(s);
  EXPECT_EQ(s.wins, 9);
}

static void testFarmGuard() {
  FarmGuard g;
  EXPECT_TRUE(farmGuardAllows(g, 1, 1000));
  farmGuardRecord(g, 1, 1000);
  EXPECT_TRUE(!farmGuardAllows(g, 1, 1000 + FARM_COOLDOWN_SEC - 1));
  EXPECT_TRUE(farmGuardAllows(g, 1, 1000 + FARM_COOLDOWN_SEC));
  EXPECT_TRUE(farmGuardAllows(g, 2, 1001));  // inny przeciwnik

  // cofniety zegar: blokada, a resync restartuje cooldown od nowego czasu
  EXPECT_TRUE(!farmGuardAllows(g, 1, 500));
  farmGuardResync(g, 1, 500);
  EXPECT_TRUE(!farmGuardAllows(g, 1, 501));
  EXPECT_TRUE(farmGuardAllows(g, 1, 500 + FARM_COOLDOWN_SEC));
  int count = 0;
  for (const FarmEntry &e : g.entries) count += e.epoch != 0;
  EXPECT_EQ(count, 1);
}

static void testFarmGuardBlocksWithoutValidClock() {
  FarmGuard g;
  EXPECT_TRUE(!farmGuardAllows(g, 1, 0));  // RTC niewazny: brak punktow
  farmGuardRecord(g, 1, 0);                // nie tworzy wpisu
  EXPECT_TRUE(farmGuardAllows(g, 1, 100));
  farmGuardRecord(g, 1, 100);
  farmGuardResync(g, 1, 0);                // zero nie kasuje wpisu
  EXPECT_TRUE(!farmGuardAllows(g, 1, 101));
}

static void testFarmGuardEvictsOldest() {
  FarmGuard g;
  farmGuardRecord(g, 1, 5000);
  farmGuardRecord(g, 2, 3000);  // najstarszy
  farmGuardRecord(g, 3, 4000);
  farmGuardRecord(g, 4, 6000);
  farmGuardRecord(g, 5, 7000);  // wypycha #2
  EXPECT_TRUE(farmGuardAllows(g, 2, 7001));
  EXPECT_TRUE(!farmGuardAllows(g, 1, 7001));
  EXPECT_TRUE(!farmGuardAllows(g, 3, 7001));
  EXPECT_TRUE(!farmGuardAllows(g, 4, 7001));
  EXPECT_TRUE(!farmGuardAllows(g, 5, 7001));
}

static void testHistoryRing() {
  PvpHistoryEntry h[PVP_HISTORY_SIZE];
  for (uint32_t i = 1; i <= 6; i++) pvpHistoryPush(h, "NICK", PVP_WIN, (uint8_t)i, i);
  EXPECT_EQ(h[0].seq, 6u);
  EXPECT_EQ(h[4].seq, 2u);  // seq 1 wypadl
  EXPECT_EQ(h[0].oppLevel, 6);
  EXPECT_TRUE(strcmp(h[0].nick, "NICK") == 0);
  pvpHistoryPush(h, "ABCDEFGHIJKLMNOP", PVP_LOSS, 3, 7);  // nick przyciety
  EXPECT_EQ(strlen(h[0].nick), (size_t)PVP_NICK_MAX);
}

static void testSanitizeHistoryDropsGarbage() {
  PvpHistoryEntry h[PVP_HISTORY_SIZE];
  memset(h, 0xFF, sizeof(h));
  sanitizePvpHistory(h);
  for (const PvpHistoryEntry &e : h) EXPECT_EQ(e.seq, 0u);
  pvpHistoryPush(h, "OK", PVP_WIN, 200, 1);
  sanitizePvpHistory(h);
  EXPECT_EQ(h[0].oppLevel, TRAINER_LEVEL_MAX);
}

int main() {
  testLevelCostTable();
  testPointsTable();
  testMultiLevelUpKeepsRemainder();
  testMultiLevelUpLoop();
  testLossNeverDropsLevelOrGoesNegative();
  testAbortChangesNothing();
  testBlockedPointsStillCountMatch();
  testCounterSaturation();
  testMaxLevelHasNoProgress();
  testSanitizeStats();
  testFarmGuard();
  testFarmGuardBlocksWithoutValidClock();
  testFarmGuardEvictsOldest();
  testHistoryRing();
  testSanitizeHistoryDropsGarbage();
  if (failures) {
    std::cerr << failures << " Testfehler\n";
    return EXIT_FAILURE;
  }
  std::cout << "Alle trainer.cpp-Tests bestanden\n";
  return EXIT_SUCCESS;
}
