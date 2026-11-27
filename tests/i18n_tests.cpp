#include <cstdio>
#include <cstring>
#include <string>

#include "../i18n.h"
#include "../pet.h"
#include "../pvp_strings.h"


static int failures = 0;

static void check(bool ok, const char *what, int lang, int id) {
  if (!ok) {
    std::printf("FAIL lang=%d id=%d: %s\n", lang, id, what);
    failures++;
  }
}

static bool isAscii(const char *s) {
  for (; *s; s++)
    if ((unsigned char)*s >= 0x80) return false;
  return true;
}

// Sequence of printf conversion specifiers ("%%" is skipped), e.g. "%u%lu%s".
static std::string specs(const char *s) {
  std::string out;
  for (; *s; s++) {
    if (*s != '%') continue;
    s++;
    if (*s == '%') continue;
    std::string spec = "%";
    while (*s && !strchr("diuxXcsfp", *s)) spec += *s++;
    if (*s) spec += *s;
    out += spec;
    if (!*s) break;
  }
  return out;
}

// Formats the string with the worst-case arguments of its specifier sequence (as in EN).
// Returns false for a sequence the test does not know (a new kind of format must be added here).
static bool formatWorst(const char *fmt, const std::string &spec, char *out, size_t cap) {
  static const char NICK[] = "ABCDEFGHIJ";  // PVP_NICK_LEN characters
  if (spec.empty()) std::snprintf(out, cap, "%s", fmt);
  else if (spec == "%u") std::snprintf(out, cap, fmt, 99u);
  else if (spec == "%u%u") std::snprintf(out, cap, fmt, 99u, 99u);
  else if (spec == "%+d") std::snprintf(out, cap, fmt, -99);
  else if (spec == "%s") std::snprintf(out, cap, fmt, NICK);
  else return false;
  return true;
}

static void checkPvpStrings() {
  for (int l = 0; l < LANG_COUNT; l++) {
    for (int id = 0; id < PVP_STR_COUNT; id++) {
      const char *t = pvpStrIn((PvpStrId)id, (Lang)l);
      check(t != nullptr && t[0] != '\0', "PvP: null or empty string", l, id);
      if (!t) continue;
      check(isAscii(t), "PvP: non-ASCII character", l, id);
      const char *en = pvpStrIn((PvpStrId)id, LANG_EN);
      std::string enSpec = specs(en);
      check(specs(t) == enSpec, "PvP: printf specifiers differ from EN", l, id);
      if (specs(t) != enSpec) continue;  // formatting with foreign arguments could crash the test
      char buf[96];
      bool known = formatWorst(t, enSpec, buf, sizeof(buf));
      check(known, "PvP: unknown specifier sequence", l, id);
      if (known) check((int)std::strlen(buf) <= pvpStrMaxLen((PvpStrId)id), "PvP: longer than maxlen", l, id);
    }
  }
  check(std::strcmp(pvpStrIn((PvpStrId)PVP_STR_COUNT, LANG_EN), "") == 0, "PvP: out of range id is not empty", 0, 0);
}

int main() {
  checkPvpStrings();
  for (int l = 0; l < LANG_COUNT; l++) {
    setLang((Lang)l);
    for (int id = 0; id < STR_COUNT; id++) {
      const char *t = T((StrId)id);
      check(t != nullptr && t[0] != '\0', "null or empty string", l, id);
      if (!t) continue;
      check(isAscii(t), "non-ASCII character", l, id);
      setLang(LANG_EN);
      std::string en = specs(T((StrId)id));
      setLang((Lang)l);
      check(specs(t) == en, "printf specifiers differ from EN", l, id);
    }
    for (int m = 0; m < MED_COUNT; m++) {
      const char *a = medalName(m), *b = medalLabel(m), *c = medalDesc(m);
      check(a && a[0] && isAscii(a), "medalName", l, m);
      check(b && b[0] && isAscii(b), "medalLabel", l, m);
      check(c && c[0] && isAscii(c), "medalDesc", l, m);
    }
  }
  if (failures) {
    std::printf("i18n tests: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("i18n tests: all %d languages x %d strings + %d PvP strings OK\n", (int)LANG_COUNT, (int)STR_COUNT,
              (int)PVP_STR_COUNT);
  return 0;
}
