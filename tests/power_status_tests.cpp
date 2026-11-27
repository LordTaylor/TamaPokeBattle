#include <cstdio>

#include "../power_status.h"

static int failures = 0;

static void check(bool ok, const char *what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

int main() {
  check(powerStateOf(-1, false, true) == POWER_NO_BATTERY, "no battery hides chip even on USB");
  check(powerStateOf(-1, true, true) == POWER_NO_BATTERY, "no battery wins over charging flag");
  check(powerStateOf(80, true, true) == POWER_CHARGING, "charging on USB");
  check(powerStateOf(100, false, true) == POWER_USB, "plugged but not charging is USB, not battery");
  check(powerStateOf(55, false, false) == POWER_BATTERY, "unplugged is battery");
  check(powerStateOf(0, false, false) == POWER_BATTERY, "0 percent is still a battery");

  check(powerChipKey(50, POWER_BATTERY) != powerChipKey(50, POWER_CHARGING), "state change changes key");
  check(powerChipKey(50, POWER_BATTERY) != powerChipKey(51, POWER_BATTERY), "percent change changes key");
  check(powerChipKey(250, POWER_BATTERY) == powerChipKey(100, POWER_BATTERY), "percent clamped high");
  check(powerChipKey(-5, POWER_NO_BATTERY) == powerChipKey(0, POWER_NO_BATTERY), "percent clamped low");

  if (failures) return 1;
  std::printf("power status tests: OK\n");
  return 0;
}
