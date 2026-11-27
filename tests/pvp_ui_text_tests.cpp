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

static void testFarthestRowDecides() {
  // a block above the center: the top row is farther than the bottom one
  expectEq(pvpMaxChars(34, 50, 2), pvpMaxChars(34, 34, 2), "farthest top", 34);
  // a block below the center: the bottom row decides
  expectEq(pvpMaxChars(400, 430, 1), pvpMaxChars(430, 430, 1), "farthest bottom", 430);
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
  testFarthestRowDecides();
  testFit();
  if (failures) {
    std::printf("pvp_ui_text tests: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("pvp_ui_text tests: all OK\n");
  return 0;
}
