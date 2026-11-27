// Rules of the PvP battle screen that decide what the player reads: round log text, countdown,
// low-HP threshold, blocked-award reason, paging (pure functions of pvp_ui_battle_logic).
#include <cstdio>
#include <cstring>

#include "../i18n.h"
#include "../pvp_ui_battle_logic.h"

static int failures = 0;

static void expectStr(const char *got, const char *want, const char *what) {
  if (std::strcmp(got, want) != 0) {
    std::printf("FAIL %s: got \"%s\", want \"%s\"\n", what, got, want);
    failures++;
  }
}

static void expectInt(long got, long want, const char *what) {
  if (got != want) {
    std::printf("FAIL %s: got %ld, want %ld\n", what, got, want);
    failures++;
  }
}

static PvpTurnView turn(BattleAction mineAction, BattleAction theirAction) {
  PvpTurnView v = {};
  v.mine.action = mineAction;
  v.theirs.action = theirAction;
  v.mine.typePct = 100;
  v.theirs.typePct = 100;
  return v;
}

static void testMineLine() {
  PvpLogLine line;
  PvpTurnView v = turn(BATTLE_ATTACK, BATTLE_DODGE);
  v.mine.damageDealt = 23;
  pvpMineLine(v, line);
  expectStr(line.text, "Hit 23", "plain hit");

  v.mine.typePct = 200;
  pvpMineLine(v, line);
  expectStr(line.text, "EFFECTIVE 23", "super effective");
  v.mine.typePct = 50;
  pvpMineLine(v, line);
  expectStr(line.text, "WEAK 23", "not very effective");

  v = turn(BATTLE_ATTACK_HEAVY, BATTLE_DODGE);
  v.theirs.dodged = true;
  pvpMineLine(v, line);
  expectStr(line.text, "Enemy dodged", "blocked by the rival");
  v.theirs.dodged = false;
  pvpMineLine(v, line);
  expectStr(line.text, "Missed!", "plain miss");

  v = turn(BATTLE_DODGE, BATTLE_ATTACK);
  v.mine.dodged = true;
  pvpMineLine(v, line);
  expectStr(line.text, "Dodged!", "my dodge worked");

  v = turn(BATTLE_REST, BATTLE_ATTACK);
  v.mine.restFailed = true;
  pvpMineLine(v, line);
  expectInt(line.tone, PVP_TONE_BAD, "failed rest is red");
  v.mine.restFailed = false;
  v.mine.rested = true;
  v.mine.healed = 7;
  pvpMineLine(v, line);
  expectStr(line.text, "Rest +7 Guard", "rest with heal");

  v = turn(BATTLE_ATTACK_QUICK, BATTLE_ATTACK);
  v.mine.damageDealt = 5;
  v.mine.counterGained = true;
  pvpMineLine(v, line);
  expectInt(line.tone, PVP_TONE_WARN, "counter note is amber");
  expectInt(std::strstr(line.text, "Hit 5 ") == line.text, 1, "counter note follows the hit");
}

static void testFoeLine() {
  PvpLogLine line;
  PvpTurnView v = turn(BATTLE_DODGE, BATTLE_ATTACK);
  v.theirs.damageDealt = 17;
  pvpFoeLine(v, line);
  expectStr(line.text, "Rival: hit 17", "rival hit");
  expectInt(line.tone, PVP_TONE_BAD, "rival hit is red");
  v.theirs.damageDealt = 0;
  pvpFoeLine(v, line);
  expectStr(line.text, "Rival: missed", "rival attack without damage");
  v.theirs.action = BATTLE_REST;
  pvpFoeLine(v, line);
  expectStr(line.text, "", "rival rested: nothing to report");
}

static void testClock() {
  expectInt(pvpRoundLeftPermille(1000, 1000, 20000), 1000, "clock at start");
  expectInt(pvpRoundLeftPermille(11000, 1000, 20000), 500, "clock half");
  expectInt(pvpRoundLeftPermille(21000, 1000, 20000), 0, "clock at the end");
  expectInt(pvpRoundLeftPermille(900, 1000, 20000), 1000, "time before the start is not spent");
  expectInt(pvpRoundLeftPermille(10u, 0xFFFFFFF0u, 20000), 998, "clock across the millis() wrap");
  expectInt(pvpRoundSecondsLeft(1000, 1000, 20000), 20, "20 s at start");
  expectInt(pvpRoundSecondsLeft(1001, 1000, 20000), 20, "rounded up");
  expectInt(pvpRoundSecondsLeft(20999, 1000, 20000), 1, "last second");
  expectInt(pvpRoundSecondsLeft(21000, 1000, 20000), 0, "zero stays zero");
  expectInt(pvpTimeTone(501), PVP_TONE_INK, "tone ok");
  expectInt(pvpTimeTone(500), PVP_TONE_WARN, "tone warn upper edge");
  expectInt(pvpTimeTone(250), PVP_TONE_WARN, "tone warn lower edge");
  expectInt(pvpTimeTone(249), PVP_TONE_BAD, "tone bad");
}

static void testRules() {
  expectInt(pvpHpLow(30, 100), 1, "30 % is low");
  expectInt(pvpHpLow(31, 100), 0, "31 % is not low");
  expectInt(pvpHpLow(0, 100), 0, "dead is not 'low'");
  expectInt(pvpBlockedText(0), PVP_S_BLOCKED_CLOCK, "no clock");
  expectInt(pvpBlockedText(1700000000u), PVP_S_BLOCKED_SAME_RIVAL, "same rival");
  expectInt(pvpPageCount(0, 5), 1, "no items = one page");
  expectInt(pvpPageCount(5, 5), 1, "5 items = one page");
  expectInt(pvpPageCount(6, 5), 2, "6 items = two pages");
  expectInt(pvpPageCount(8, 5), 2, "8 items = two pages");
}

int main() {
  setLang(LANG_EN);
  testMineLine();
  testFoeLine();
  testClock();
  testRules();
  if (failures) {
    std::printf("pvp ui battle logic tests: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("pvp ui battle logic tests OK\n");
  return 0;
}
