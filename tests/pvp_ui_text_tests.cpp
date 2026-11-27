// Round-screen chord geometry and text fitting (pure functions of pvp_ui_text).
#include <cstdio>
#include <cstring>

#include "../pvp_ui_text.h"

static int failures = 0;

static void expectEq(int got, int want, const char *what, int arg) {
  if (got != want) {
    std::printf("FAIL %s(%d): got %d, want %d\n", what, arg, got, want);
    failures++;
  }
}

static void testChordTable() {
  // spec C.0: y -> usable width, chars at size 1 / 2 / 3
  static const int ROWS[][5] = {
      {34, 226, 37, 18, 12},  {56, 287, 47, 23, 15},  {82, 338, 56, 28, 18},  {110, 379, 63, 31, 21},
      {160, 426, 71, 35, 23}, {233, 450, 75, 37, 25}, {290, 435, 72, 36, 24}, {320, 416, 69, 34, 23},
      {350, 386, 64, 32, 21}, {390, 328, 54, 27, 18}, {416, 272, 45, 22, 15}, {430, 232, 38, 19, 12},
      {444, 181, 30, 15, 10},
  };
  for (const auto &r : ROWS) {
    expectEq(pvpUsableWidth(r[0]), r[1], "usableWidth", r[0]);
    expectEq(pvpMaxChars(r[0], r[0], 1), r[2], "maxChars@1", r[0]);
    expectEq(pvpMaxChars(r[0], r[0], 2), r[3], "maxChars@2", r[0]);
    expectEq(pvpMaxChars(r[0], r[0], 3), r[4], "maxChars@3", r[0]);
  }
  expectEq(pvpChordHalfWidth(233), 233, "chordHalf", 233);
  expectEq(pvpChordHalfWidth(0), 0, "chordHalf", 0);
  expectEq(pvpChordHalfWidth(-5), 0, "chordHalf outside", -5);
  expectEq(pvpChordHalfWidth(470), 0, "chordHalf outside", 470);
  expectEq(pvpUsableWidth(0), 0, "usableWidth at the rim", 0);
  expectEq(pvpMaxChars(100, 300, 0), 0, "maxChars size 0", 0);
}

static void testFarthestRowDecides() {
  // a block above the center: the top row is farther than the bottom one
  expectEq(pvpMaxChars(34, 50, 2), pvpMaxChars(34, 34, 2), "farthest top", 34);
  // a block below the center: the bottom row decides
  expectEq(pvpMaxChars(400, 430, 1), pvpMaxChars(430, 430, 1), "farthest bottom", 430);
}

static void testCenter() {
  expectEq(pvpCenterX(0), 233, "centerX", 0);
  expectEq(pvpCenterX(120), 173, "centerX", 120);
  expectEq(pvpTextWidth(10, 2), 120, "textWidth", 10);
}

static void expectText(const char *src, size_t cap, int maxChars, const char *want) {
  char buf[16];
  std::memset(buf, 'X', sizeof(buf));
  pvpFitText(src, buf, cap, maxChars);
  if (std::strcmp(buf, want) != 0) {
    std::printf("FAIL fit(\"%s\", cap %zu, max %d): got \"%s\", want \"%s\"\n", src, cap, maxChars, buf, want);
    failures++;
  }
  if (cap < sizeof(buf) && buf[cap] != 'X' && cap > 0) {  // nothing written past dstSize
    std::printf("FAIL fit(\"%s\"): wrote past the buffer\n", src);
    failures++;
  }
}

static void testFit() {
  expectText("ABCDE", 16, 5, "ABCDE");
  expectText("ABCDEF", 16, 5, "ABC..");
  expectText("ABCDEF", 16, 2, "..");
  expectText("ABCDEF", 16, 1, ".");
  expectText("ABCDEF", 16, 0, "");
  expectText("ABCDEFGH", 5, 8, "AB..");  // the buffer is tighter than the limit
  expectText("AB", 1, 5, "");
}

int main() {
  testChordTable();
  testFarthestRowDecides();
  testCenter();
  testFit();
  if (failures) {
    std::printf("pvp_ui_text tests: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("pvp_ui_text tests: all OK\n");
  return 0;
}
