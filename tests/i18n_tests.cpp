#include <cstdio>
#include <cstring>
#include <string>

#include "../i18n.h"
#include "../pet.h"


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

int main() {
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
  std::printf("i18n tests: all %d languages x %d strings OK\n", (int)LANG_COUNT, (int)STR_COUNT);
  return 0;
}
